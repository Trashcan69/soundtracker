
/*
 * The Real SoundTracker - sample editor extensions
 *
 * Copyright (C) 2023 Yury Aliaev
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <glib/gi18n.h>

#include <libxml/parser.h>

#include "gui.h"
#include "gui-settings.h"
#include "history.h"
#include "poll.h"
#include "sample-editor.h"
#include "st-subs.h"

static gboolean show_error = TRUE;

typedef enum {
    PAR_CONST,
    PAR_INT,
    PAR_FLOAT,
    PAR_STRING,
    PAR_VARIANT,
    PAR_REPEAT /* Should be the last one */
} ParType;

/* XML tags and attributes */
#define TAG_COMMAND (const xmlChar*)"command"
#define TAG_MENUITEM (const xmlChar*)"menuitem"
#define TAG_PARAMETER (const xmlChar*)"parameter"
#define TAG_MENU (const xmlChar*)"menu"
#define TAG_ITEM (const xmlChar*)"item")
#define ATTR_LABEL (const xmlChar*)"label"
#define ATTR_TITLE (const xmlChar*)"title"
#define ATTR_TYPE (const xmlChar*)"type"
#define ATTR_REPEAT (const xmlChar*)"repeat"
#define ATTR_REPNAME (const xmlChar*)"repname"
#define ATTR_DIGITS (const xmlChar*)"digits"
#define ATTR_STEP (const xmlChar*)"step"
#define ATTR_MIN (const xmlChar*)"min"
#define ATTR_MAX (const xmlChar*)"max"
#define ATTR_DEFAULT (const xmlChar*)"default"
#define ATTR_NAME (const xmlChar*)"name"
#define ATTR_VALUE (const xmlChar*)"value"
#define ATTR_KEY (const xmlChar*)"key"
#define ATTR_DELIMITER (const xmlChar*)"delimiter"
#define ATTR_SUFFIX (const xmlChar*)"suffix"
#define ATTR_OPTIONAL (const xmlChar*)"optional"
#define ATTR_DEPEND (const xmlChar*)"depend"
#define ATTR_VERSION (const xmlChar*)"version"

struct Params {
    ParType type;
    const char* name;
};

static const struct Params params[] =
{
    {PAR_CONST, "const"}, {PAR_INT, "int"}, {PAR_FLOAT, "float"},
    {PAR_STRING, "string"}, {PAR_VARIANT, "variant"}
};

struct ExtEntry {
    ParType type;
    GtkWidget *box, *opt_check, *label, *input, *depend_master;
    const gchar *key, *delim, *suffix;
    const gchar **variants, **keys;
    gint num_variants;
};

#define TYPE_EXT_DIALOG (ext_dialog_get_type())
#define EXT_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_EXT_DIALOG, ExtDialog))

typedef struct _ExtDialog ExtDialog;
typedef struct _ExtDialogClass ExtDialogClass;

struct _ExtDialog {
    GtkDialog dialog;
    GtkWidget* vbox;
    struct ExtEntry *entries;
    gint len, alloc_len, rep_start;
    const gchar* command;
    gboolean shown;
    gint rep_index, rep_num, rep_count;
};

struct _ExtDialogClass {
    GtkDialogClass parent_class;
};

static ExtDialog* last_dialog;

static void ext_dialog_run(ExtDialog* d);

static void
ext_dialog_init(ExtDialog* d)
{
    d->len = 0;
    d->alloc_len = 8;
    d->entries = g_new(struct ExtEntry, d->alloc_len);
    d->shown = FALSE;
    d->rep_num = 0;
};

static void
ext_dialog_class_init(ExtDialogClass* c)
{
};

G_DEFINE_TYPE(ExtDialog, ext_dialog, GTK_TYPE_DIALOG);

static void
master_toggled(GtkToggleButton* tb,
    GtkWidget* w)
{
    gtk_widget_set_sensitive(w, gtk_toggle_button_get_active(tb));
}

static void
master_state_changed(GtkWidget* master,
    GtkStateType s,
    GtkWidget* w)
{
    gtk_widget_set_sensitive(w,
        gtk_widget_get_state(master) != GTK_STATE_INSENSITIVE &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(master)));
}

static void
err_dialog_resp(GtkWidget* w,
    gint response,
    GtkTextBuffer* b)
{
    if (response == GTK_RESPONSE_APPLY) {
        GtkTextIter start, end;

        gtk_text_buffer_get_bounds(b, &start, &end);
        gtk_text_buffer_delete(b, &start, &end);
    } else
        gtk_widget_hide(w);
}

static void
update_message(const gchar* const err_buf,
    const gint bufsize)
{
    static GtkTextBuffer* buffer = NULL;
    static GtkWidget* err_dialog;

    if (!buffer) {
        GtkWidget *vbox, *thing;

        buffer = gtk_text_buffer_new(NULL);

        err_dialog = gtk_dialog_new_with_buttons(_("Message from extension"),
            NULL, 0, GTK_STOCK_CLEAR, GTK_RESPONSE_APPLY,
            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
        g_object_set(G_OBJECT(err_dialog), "has-separator", TRUE, NULL);
        gui_dialog_adjust(err_dialog, GTK_RESPONSE_CLOSE);
        gui_dialog_connect_data(err_dialog, G_CALLBACK(err_dialog_resp), buffer);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(err_dialog));
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

        thing = gtk_text_view_new_with_buffer(buffer);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(thing), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(thing), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(thing), GTK_WRAP_WORD);
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
        gtk_widget_show_all(vbox);
    }

    gtk_text_buffer_insert_at_cursor(buffer, err_buf, bufsize);
    if (!gtk_widget_get_visible(err_dialog))
        gtk_window_present(GTK_WINDOW(err_dialog));
}

static void
progress_resp(GtkWidget* d,
    gint resp,
    gpointer data)
{
    if (resp == GTK_RESPONSE_APPLY) {
        pid_t* pid = data;
        show_error = FALSE;
        kill(*pid, SIGKILL);
    }
}

static gboolean
progress_del(void)
{
    return TRUE;
}

static void
update_progress(const gdouble fraction,
    const gboolean close,
    gint* pid)
{
    static GtkWidget *d = NULL, *progress;

    if (!d) {
        GtkWidget *vbox;

        d = gtk_dialog_new_with_buttons(_("Progress"),
            NULL, 0, GTK_STOCK_STOP, GTK_RESPONSE_APPLY, NULL);
        g_object_set(G_OBJECT(d), "has-separator", TRUE, NULL);
        g_signal_connect(d, "response",
            G_CALLBACK(progress_resp), pid);
        g_signal_connect(d, "delete_event",
            G_CALLBACK(progress_del), NULL);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(d));
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

        progress = gtk_progress_bar_new();
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), _("Processing..."));
        gtk_box_pack_start(GTK_BOX(vbox), progress, FALSE, FALSE, 0);
        gtk_widget_show_all(vbox);
    }

    if (close)
        gtk_widget_hide(d);
    else {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), fraction);
        if (!gtk_widget_get_visible(d))
            gtk_window_present(GTK_WINDOW(d));
    }
}

static struct ExtEntry*
alloc_entry(ExtDialog* d,
    gint* i)
{
    struct ExtEntry* retval;
    gint n = d->len++;

    if (d->len > d->alloc_len) {
        struct ExtEntry* tmp;

        d->alloc_len += d->alloc_len >> 1;
        tmp = g_renew(struct ExtEntry, d->entries, d->alloc_len);
        d->entries = tmp;
    }

    memset(retval = &d->entries[n], 0, sizeof(struct ExtEntry));
    if (i)
        *i = n;

    return retval;
}

static void
repeat_changed(GtkSpinButton* sb,
    ExtDialog* d)
{
    gint i, new_num = gtk_spin_button_get_value_as_int(sb);
    gint first_hidden;
    GList* l = gtk_container_get_children(GTK_CONTAINER(d->vbox));

    if (new_num < d->rep_num) {
        /* + 1 and - 1 because of the separators */
        first_hidden = d->rep_index + new_num * (d->rep_count + 1) - 1;

        for (i = 0; l; l = l->next, i++)
            if (i >= first_hidden) {
                GtkWidget* box = l->data;

                if (gtk_widget_get_visible(box))
                    gtk_widget_hide(box);
                else
                    break;
            }
    } else if (new_num > d->rep_num) {
        gint stop_index = d->rep_index + new_num * (d->rep_count + 1) - 1;

        first_hidden = d->rep_index + d->rep_num * (d->rep_count + 1) - 1;
        /* Show hidden entries */
        for (i = 0; l != NULL && i < stop_index; l = l->next, i++)
            if (i >= first_hidden)
                gtk_widget_show(GTK_WIDGET(l->data));

        stop_index -= i;
        /* No more entries, we are to create some new */
        for (i = 0; i < stop_index; i += d->rep_count + 1) {
            gint j, k;
            GtkWidget* thing = gtk_hseparator_new();

            gtk_box_pack_start(GTK_BOX(d->vbox), thing, FALSE, FALSE, 0);
            gtk_widget_show(thing);

            for(j = 0; j < d->rep_count; j++) {
                GtkAdjustment* adj;
                GtkListStore* ls;
                GtkTreeIter iter;
                /* Order makes sense! Never exchange two following lines! */
                struct ExtEntry *n_e = alloc_entry(d, NULL);
                struct ExtEntry *e = &d->entries[d->rep_start + j];

                switch (n_e->type = e->type) {
                case PAR_INT:
                case PAR_FLOAT:
                    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(e->input));

                    n_e->input = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(gtk_spin_button_get_value(GTK_SPIN_BUTTON(e->input)),
                        gtk_adjustment_get_lower(adj), gtk_adjustment_get_upper(adj), gtk_adjustment_get_step_increment(adj),
                        gtk_adjustment_get_page_increment(adj), 0.0)), 0.0, gtk_spin_button_get_digits(GTK_SPIN_BUTTON(e->input)));
                    break;
                case PAR_STRING:
                    n_e->input = gtk_entry_new();
                    break;
                case PAR_VARIANT:
                    ls = gtk_list_store_new(1, G_TYPE_STRING);
                    n_e->variants = g_new(const gchar*, e->num_variants);

                    for(k = 0; k < e->num_variants; k++) {
                        n_e->variants[k] = e->variants[k];
                        gtk_list_store_append(ls, &iter);
                        gtk_list_store_set(ls, &iter, 0, e->keys[k], -1);
                    }
                    n_e->input = gui_combo_new(ls);
                    gtk_combo_box_set_active(GTK_COMBO_BOX(n_e->input),
                        gtk_combo_box_get_active(GTK_COMBO_BOX(e->input)));
                    break;
                default:
                    break;
                }

                n_e->key = e->key;
                n_e->delim = e->delim;
                n_e->suffix = e->suffix;
                if (e->label)
                    n_e->label = gtk_label_new(gtk_label_get_text(GTK_LABEL(e->label)));
                if (e->opt_check)
                    n_e->opt_check = gtk_check_button_new();

                /* No dependencies for repeated entries, that's too complicated
                    and probably almost useless */

                if (n_e->input || n_e->label || n_e->opt_check) {
                    n_e->box = gtk_hbox_new(FALSE, 2);
                    if (n_e->opt_check)
                        gtk_box_pack_start(GTK_BOX(n_e->box), n_e->opt_check, FALSE, FALSE, 0);
                    if (n_e->label)
                        gtk_box_pack_start(GTK_BOX(n_e->box), n_e->label, FALSE, FALSE, 0);
                    if (n_e->input)
                        gtk_box_pack_end(GTK_BOX(n_e->box), n_e->input, FALSE, FALSE, 0);

                    gtk_box_pack_start(GTK_BOX(d->vbox), n_e->box, FALSE, FALSE, 0);
                    gtk_widget_show_all(n_e->box);
                }
            }
        }
    }

    d->rep_num = new_num;
}

static void
last_dialog_run(void)
{
    g_assert(last_dialog != NULL);
    ext_dialog_run(last_dialog);
}

static void
ext_dialog_run(ExtDialog* d)
{
    gint response;
    gchar** args;
    pid_t pid;
    int from_pipe[2], to_pipe[2], err_pipe[2];
    static GRegex *repl_rate = NULL, *repl_len, *repl_time, *repl_time1, *repl_channels;

    if (!repl_rate) {
        repl_rate = g_regex_new("%r", 0, 0, NULL);
        repl_len = g_regex_new("%l", 0, 0, NULL);
        repl_time = g_regex_new("%t", 0, 0, NULL);
        /* Slightly decreased duration used to work around some Sox quirks */
        repl_time1 = g_regex_new("%d", 0, 0, NULL);
        repl_channels = g_regex_new("%c", 0, 0, NULL);
    }

    if (!d->shown) {
        GtkAllocation a;
        GtkWidget *mainvbox, *sw;
        gint i;

        d->shown = TRUE;
        gtk_dialog_add_buttons(GTK_DIALOG(d),
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

        mainvbox = gtk_dialog_get_content_area(GTK_DIALOG(d));
        sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start(GTK_BOX(mainvbox), sw, TRUE, TRUE, 0);

        d->vbox = gtk_vbox_new(FALSE, 4);
        gtk_container_set_border_width(GTK_CONTAINER(d->vbox), 4);
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), d->vbox);
        gtk_viewport_set_shadow_type(GTK_VIEWPORT(gtk_bin_get_child(GTK_BIN(sw))), GTK_SHADOW_NONE);

        for (i = 0; i < d->len; i++) {
            struct ExtEntry* e = &d->entries[i];
            GtkWidget* hbox = e->box;
            GtkWidget* master = e->depend_master;

            if (hbox) {
                gtk_box_pack_start(GTK_BOX(d->vbox), hbox, FALSE, FALSE, 0);
                if (master) {
                    g_signal_connect(master, "toggled",
                        G_CALLBACK(master_toggled), hbox);
                    g_signal_connect(master, "state_changed",
                        G_CALLBACK(master_state_changed), hbox);
                    gtk_widget_set_sensitive(hbox,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(master)) &&
                        gtk_widget_get_sensitive(master));
                }
            }

            if (e->type == PAR_REPEAT) {
                GtkWidget* thing = gtk_hseparator_new();

                d->rep_index = i + 1;
                d->rep_num = 1;
                g_signal_connect(e->input, "value-changed",
                    G_CALLBACK(repeat_changed), d);
                gtk_box_pack_start(GTK_BOX(d->vbox), thing, FALSE, FALSE, 0);
            }
        }
        if (d->rep_num)
            d->rep_count = i - d->rep_index;

        gtk_widget_show_all(GTK_WIDGET(d));
        gtk_widget_get_allocation(d->vbox, &a);
        gtk_widget_set_size_request(sw, -1, a.height);
    }

    response = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_hide(GTK_WIDGET(d));
    last_dialog = d;
    sample_editor_log_action(last_dialog_run);

    if (response == GTK_RESPONSE_OK) {
        gchar *tmp, *par_str, *line = g_strdup(d->command);
        STSample* s = sample_editor_get_sample();
        gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
        gint i;

        for (i = 0; i < d->len; i++) {
            struct ExtEntry* e = &d->entries[i];

            if (e->type != PAR_REPEAT &&
                (!e->opt_check ||
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e->opt_check))) &&
                (!e->box || (gtk_widget_get_visible(e->box) && gtk_widget_get_sensitive(e->box))) &&
                (!e->depend_master || (
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e->depend_master)) &&
                /* For boxless constants which may nevertheless be dependent */
                gtk_widget_get_sensitive(e->depend_master)))) {

                /* delimiter, key, suffix */
                tmp = g_strconcat(line, e->delim ? e->delim : " ", e->key, NULL);
                g_free(line);
                line = tmp;

                switch (e->type) {
                case PAR_INT:
                case PAR_STRING:
                    tmp = g_strconcat(line, gtk_entry_get_text(GTK_ENTRY(e->input)), e->suffix, NULL);
                    g_free(line);
                    line = tmp;
                    break;
                case PAR_FLOAT:
                    tmp = g_strconcat(line, g_ascii_dtostr(buf, sizeof(buf),
                        gtk_spin_button_get_value(GTK_SPIN_BUTTON(e->input))), e->suffix, NULL);
                    g_free(line);
                    line = tmp;
                    break;
                case PAR_VARIANT:
                    tmp = g_strconcat(line,
                        e->variants[gtk_combo_box_get_active(GTK_COMBO_BOX(e->input))],
                        e->suffix, NULL);
                    g_free(line);
                    line = tmp;
                    break;
                default:
                    break;
                }
            }
        }
        par_str = g_strdup_printf("%li", lrint(SAMPLE_GET_RATE(s)));
        tmp = g_regex_replace(repl_rate, line, -1, 0, par_str, 0, NULL);
        g_free(line);
        g_free(par_str);
        line = tmp;

        par_str = g_strdup_printf("%i", s->sample.length);
        tmp = g_regex_replace(repl_len, line, -1, 0, par_str, 0, NULL);
        g_free(line);
        g_free(par_str);
        line = tmp;

        tmp = g_regex_replace(repl_channels, line, -1, 0,
            s->sample.flags & ST_SAMPLE_STEREO ? "2" : "1", 0, NULL);
        g_free(line);
        line = tmp;

        g_ascii_dtostr(buf, sizeof(buf), (gdouble)s->sample.length / (gdouble)(SAMPLE_GET_RATE(s)));
        tmp = g_regex_replace(repl_time, line, -1, 0, buf, 0, NULL);
        g_free(line);
        line = tmp;

        g_ascii_dtostr(buf, sizeof(buf), (gdouble)s->sample.length / (gdouble)(SAMPLE_GET_RATE(s)) - 0.01);
        tmp = g_regex_replace(repl_time1, line, -1, 0, buf, 0, NULL);
        g_free(line);
        line = tmp;

        args = g_strsplit(line, " ", -1);
        g_free(line);

        errno = 0;
        if (pipe(from_pipe) || pipe(to_pipe) || pipe(err_pipe)) {
            gui_errno_dialog(mainwindow, _("Error while pipes creation"), errno);
            goto end;
        }

        if ((pid = fork())) {
            gint stts;

            if (pid == -1)
                gui_errno_dialog(mainwindow, _("Fork failed"), errno);
            else {
                gint wstatus, towrite, readlen, num_read, gotback = 0, err_read, ss, se;
                gsize rlen = 0;
                gboolean ready = FALSE, error = FALSE, whole = FALSE;
                const gboolean stereo = s->sample.flags & ST_SAMPLE_STEREO;
                struct pollfd pfd = { to_pipe[0], POLLIN, 0 };
                struct pollfd err_pfd = { err_pipe[0], POLLIN, 0 };
                void *new_sdata, *buf, *sdata, *writebuf;
                static gchar err_buf[4096];

                show_error = TRUE;

                close(from_pipe[0]);
                close(to_pipe[1]);
                close(err_pipe[1]);

                sample_editor_get_selection(&ss, &se);
                if (ss == -1) {
                    ss = 0;
                    se = s->sample.length;
                    whole = TRUE;
                }
                readlen = (se - ss) * sizeof(s->sample.data[0]);
                if (stereo) {
                    readlen <<= 1;
                    writebuf = sdata = malloc(readlen);
                    st_convert_to_int(&s->sample.data[ss], sdata, s->sample.length, se - ss);
                } else
                    sdata = &s->sample.data[ss];
                buf = new_sdata = malloc(readlen);
                towrite = readlen;

                do {
                    gint written = write(from_pipe[1], sdata, MIN(towrite, 4096));

                    if (written == -1) {
                        if (show_error) /* Already killed */
                            gui_errno_dialog(mainwindow, _("Communication error (writing)"), errno);
                        else
                            kill(pid, SIGKILL);
                        /* Don't try to read anymore */
                        ready = TRUE;
                        break;
                    }
                    towrite -= written;
                    sdata += written;

                    if (!ready && poll(&pfd, 1, 0)) {
                        num_read = read(to_pipe[0], buf, MIN(readlen - gotback, 4096));

                        if (num_read == -1) {
                            if (show_error)
                                gui_errno_dialog(mainwindow, _("Communication error (reading)"), errno);
                            else
                                kill(pid, SIGKILL);
                            /* Don't try to read anymore */
                            error = TRUE;
                            break;
                        }
                        buf += num_read;
                        gotback += num_read;
                        rlen += num_read;
                        if (!num_read)
                            ready = TRUE;
                    }

                    if (poll(&err_pfd, 1, 0)) {
                        err_read = read(err_pipe[0], err_buf, sizeof(err_buf));
                        if (err_read)
                            update_message(err_buf, err_read);
                    }
                    update_progress((gdouble)gotback / (gdouble)readlen, FALSE, &pid);
                    while (gtk_events_pending())
                        gtk_main_iteration();
                } while (towrite);

                close(from_pipe[1]); /* Thus sending EOF to the child */

                /* An extension can buffer its data and get output after all
                   input data will be passed to it. So we need to drain the rest of the
                   data. Also, we don't expect more bytes than have been sent */
                if (!ready && !error)
                    do {
                        num_read = read(to_pipe[0], buf, MIN(readlen - gotback, 4096));

                        if (num_read == -1) {
                            if (show_error)
                                gui_errno_dialog(mainwindow, _("Communication error (reading)"), errno);
                            else
                                kill(pid, SIGKILL);
                            /* Don't try to read anymore */
                            error = TRUE;
                            break;
                        }
                        buf += num_read;
                        gotback += num_read;
                        rlen += num_read;

                        if (poll(&err_pfd, 1, 0)) {
                            err_read = read(err_pipe[0], err_buf, sizeof(err_buf));
                            if (err_read)
                                update_message(err_buf, err_read);
                        }
                        update_progress((gdouble)gotback / (gdouble)readlen, FALSE, &pid);
                        while (gtk_events_pending())
                            gtk_main_iteration();
                    } while (num_read);

                close(to_pipe[0]);
                update_progress(0.0, TRUE, NULL);

                /* Also we still may have some output from STDERR... */
                do {
                    err_read = read(err_pipe[0], err_buf, sizeof(err_buf));
                    if (err_read)
                        update_message(err_buf, err_read);
                } while (err_read);
                close(err_pipe[0]);

                pid = waitpid(pid, &wstatus, 0);
                if (pid == -1) {
                    if (show_error)
                        gui_errno_dialog(mainwindow, _("Error while waiting for the extension"), errno);
                    error = TRUE;
                } else if (!WIFEXITED(wstatus)) {
                    static GtkWidget* d = NULL;

                    if (show_error)
                        gui_error_dialog(&d, _("Extension exited abnormally"), FALSE);
                    error = TRUE;
                } else if ((stts = WEXITSTATUS(wstatus))) {
                    if (show_error) {
                        gchar* buf = g_strdup_printf(_("Extension %s reports an error"), args[0]);

                        gui_errno_dialog(mainwindow, buf, stts);
                        g_free(buf);
                    }
                    error = TRUE;
                }

                if (!error) {
                    if (!sample_editor_check_and_log_sample(s,
                        N_("Sample processing"), HISTORY_FLAG_LOG_ALL, 0)) {
                        g_free(new_sdata);
                        goto end;
                    }
                    g_mutex_lock(&s->sample.lock);

                    if (whole) {
                        if (stereo) {
                            st_convert_to_nonint(new_sdata, s->sample.data, se - ss, se - ss);
                            g_free(new_sdata);
                            /* We can obtain less data from an extension than has been sent */
                            s->sample.length = (rlen / sizeof(s->sample.data[0])) >> 1;
                        } else {
                            g_free(s->sample.data);
                            s->sample.data = new_sdata;
                            s->sample.length = rlen / sizeof(s->sample.data[0]);
                        }
                        st_sample_fix_loop(s);
                    } else { /* Fragment */
                        if (stereo)
                            st_convert_to_nonint(new_sdata, &s->sample.data[ss],
                                s->sample.length, se - ss);
                        else
                            memcpy(&s->sample.data[ss], new_sdata, readlen);
                        g_free(new_sdata);
                    }

                    g_mutex_unlock(&s->sample.lock);
                    sample_editor_update();
                    if (!whole)
                        sample_editor_set_selection(ss, se);
                } else
                    g_free(new_sdata);

                if (stereo)
                    g_free(writebuf);
            }
        } else {
            close(from_pipe[1]);
            if (dup2(from_pipe[0], STDIN_FILENO) != STDIN_FILENO)
                exit(errno);
            close(from_pipe[0]);

            close(to_pipe[0]);
            if (dup2(to_pipe[1], STDOUT_FILENO) != STDOUT_FILENO)
                exit(errno);
            close(to_pipe[1]);

            close(err_pipe[0]);
            if (dup2(err_pipe[1], STDERR_FILENO) != STDERR_FILENO)
                exit(errno);
            close(err_pipe[1]);

            if (execvp(args[0], args))
                exit(errno);
        }
end:
        g_strfreev(args);
    }
}

static void
_fill_menu(xmlNodePtr node,
    GtkWidget* menu,
    gchar* command)
{
    xmlNodePtr child = node->xmlChildrenNode;

    for (; child; child = child->next) {
        xmlChar* label;

        if (!xmlStrcasecmp(child->name, TAG_COMMAND))
            command = (gchar *)child->xmlChildrenNode->content;
        else if ((label = xmlGetProp(child, ATTR_LABEL))) {
            GtkWidget* item = NULL;

            if (!xmlStrcasecmp(child->name, TAG_MENUITEM)) {
                xmlNodePtr param = child->xmlChildrenNode;
                ExtDialog *dialog = EXT_DIALOG(g_object_new(ext_dialog_get_type(),
                    "has-separator", TRUE, "title",
                    _((const gchar*)xmlGetProp(child, ATTR_TITLE)),
                    "modal", TRUE, "transient-for", GTK_WINDOW(mainwindow), NULL));

                dialog->command = command;
                for(; param; param = param->next) {
                    xmlChar* prop;

                    if (!xmlStrcasecmp(param->name, TAG_PARAMETER) &&
                        (prop = xmlGetProp(param, ATTR_TYPE))) {
                        gint i;
                        xmlChar* rep_prop = xmlGetProp(param, ATTR_REPEAT);
                        struct ExtEntry *e;

                        /* Additional entry for repeating set of parametes */
                        if (rep_prop) {
                            xmlChar* rep_name = xmlGetProp(param, ATTR_REPNAME);
                            if (rep_name) {
                                e = alloc_entry(dialog, &dialog->rep_start);
                                dialog->rep_start++; /* Repeat starts on the next entry */

                                e->type = PAR_REPEAT;
                                e->label = gtk_label_new(_((const gchar*)rep_name));
                                e->input = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(1.0,
                                    1.0, atoi((gchar *)rep_prop), 1.0, 1.0, 0.0)), 0.0, 0);

                                e->box = gtk_hbox_new(FALSE, 2);
                                gtk_box_pack_start(GTK_BOX(e->box), e->label, FALSE, FALSE, 0);
                                gtk_box_pack_end(GTK_BOX(e->box), e->input, FALSE, FALSE, 0);
                            }
                        }

                        e = alloc_entry(dialog, NULL);
                        for (i = 0; i < ARRAY_SIZE(params); i++)
                            if (!xmlStrcasecmp(prop, (const xmlChar*)params[i].name)) {
                                gdouble min, max, def, step;
                                gint digits, num;
                                xmlNodePtr item;
                                GtkListStore* ls;
                                GtkTreeIter iter;

                                e->type = params[i].type;
                                switch (e->type) {
                                case PAR_INT:
                                case PAR_FLOAT:
                                    prop = xmlGetProp(param, ATTR_DIGITS);
                                    digits = prop ? atoi((const char*)prop) : 0;
                                    prop = xmlGetProp(param, ATTR_STEP);
                                    step = prop ? g_ascii_strtod((const char*)prop, NULL) : 1.0;
                                    prop = xmlGetProp(param, ATTR_MIN);
                                    min = prop ? g_ascii_strtod((const char*)prop, NULL) : 0.0;
                                    prop = xmlGetProp(param, ATTR_MAX);
                                    max = prop ? g_ascii_strtod((const char*)prop, NULL) : 1000.0;
                                    prop = xmlGetProp(param, ATTR_DEFAULT);
                                    def = prop ? g_ascii_strtod((const char*)prop, NULL) : (min + max) * 0.5;
                                    e->input = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(def,
                                        min, max, step, (gfloat)(min + max) / 10.0, 0.0)), 0.0, digits);
                                    break;
                                case PAR_STRING:
                                    e->input = gtk_entry_new();
                                    break;
                                case PAR_VARIANT:
                                    for (item = param->xmlChildrenNode, num = 0; item; item = item->next)
                                        if (!xmlStrcasecmp(item->name, TAG_ITEM)
                                            num++;
                                    e->variants = g_new(const gchar*, num);
                                    e->keys = g_new(const gchar*, num);

                                    ls = gtk_list_store_new(1, G_TYPE_STRING);
                                    for (item = param->xmlChildrenNode, num = 0; item; item = item->next)
                                        if (!xmlStrcasecmp(item->name, TAG_ITEM) {
                                            gtk_list_store_append(ls, &iter);
                                            gtk_list_store_set(ls, &iter, 0,
                                                e->keys[num] = _((const gchar*)xmlGetProp(item, ATTR_NAME)), -1);
                                            e->variants[num++] = (const gchar*)xmlGetProp(item, ATTR_VALUE);
                                        }
                                    e->num_variants = num;
                                    e->input = gui_combo_new(ls);
                                    prop = xmlGetProp(item, ATTR_DEFAULT);
                                    gtk_combo_box_set_active(GTK_COMBO_BOX(e->input),
                                    prop ? atoi((const gchar*)prop) : 0);
                                break;
                                default:
                                    break;
                                }
                                break;
                            }

                        if (!(e->key = (const gchar*)xmlGetProp(param, ATTR_KEY)))
                            /* "value" is a synonym for "key" */
                            e->key = (const gchar*)xmlGetProp(param, ATTR_VALUE);
                        e->delim = (const gchar*)xmlGetProp(param, ATTR_DELIMITER);
                        e->suffix = (const gchar*)xmlGetProp(param, ATTR_SUFFIX);
                        if ((prop = xmlGetProp(param, ATTR_NAME)))
                            e->label = gtk_label_new(_((const gchar*)prop));
                        if ((prop = xmlGetProp(param, ATTR_OPTIONAL))) {
                            e->opt_check = gtk_check_button_new();
                            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->opt_check),
                                !xmlStrcasecmp(prop, (const xmlChar*)"true"));
                        }
                        if ((prop = xmlGetProp(param, ATTR_DEPEND)))
                            e->depend_master = dialog->entries[atoi((const gchar*)prop)].opt_check;

                        if (e->input || e->label || e->opt_check) {
                            e->box = gtk_hbox_new(FALSE, 2);
                            if (e->opt_check)
                                gtk_box_pack_start(GTK_BOX(e->box), e->opt_check, FALSE, FALSE, 0);
                            if (e->label)
                                gtk_box_pack_start(GTK_BOX(e->box), e->label, FALSE, FALSE, 0);
                            if (e->input)
                                gtk_box_pack_end(GTK_BOX(e->box), e->input, FALSE, FALSE, 0);
                        }
                    }
                }

                item = gtk_menu_item_new_with_label(_((const gchar*)label));
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                g_signal_connect_swapped(item, "activate",
                    G_CALLBACK(ext_dialog_run), dialog);
            } else if (!xmlStrcasecmp(child->name, TAG_MENU)) {
                GList* l = gtk_container_get_children(GTK_CONTAINER(menu));
                GtkWidget* submenu;
                gchar* loc_label = _((const gchar*)label);

                /* Look for an existing menu item */
                for(; l; l = l->next) {
                    item = l->data;
                    if (!strcmp(gtk_menu_item_get_label(GTK_MENU_ITEM(item)), loc_label)) {
                        submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
                        break;
                    }
                }
                /* No item, create a new one */
                if (!l) {
                    item = gtk_menu_item_new_with_label(loc_label);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                    submenu = gtk_menu_new();
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
                }
                _fill_menu(child, submenu, command);
            }
        }
    }
}

void
sample_editor_extensions_fill_menu(void)
{
    GtkWidget* thing;
    xmlDocPtr doc;
    gint i;
    gboolean success = FALSE;

    thing = sample_editor_get_widget("extensions_submenu");
    if (!thing)
        return;

    if (!gui_settings.se_extension_path.lines || !gui_settings.se_extension_path.num) {
        const gchar* homedir = g_getenv("HOME");

        if (!homedir)
            homedir = g_get_home_dir();

        gui_settings.se_extension_path.num = gui_settings.se_extension_path.num_allocated = 2;
        gui_settings.se_extension_path.lines = g_new(gchar*, 2);
        gui_settings.se_extension_path.lines[0] = g_strdup(DATADIR "/" PACKAGE "/extensions/sample-editor");
        gui_settings.se_extension_path.lines[1] = g_strconcat(homedir, "/.soundtracker/extensions/sample-editor", NULL);
    }

    for (i = 0; i < gui_settings.se_extension_path.num; i++) {
        gchar* path = gui_filename_from_utf8(gui_settings.se_extension_path.lines[i]);

        if (path) {
            GDir* dir = g_dir_open(path, 0, NULL);

            if (dir) { /* Silently ignoring errors */
                const gchar* name;

                while((name = g_dir_read_name(dir))) {
                    gsize len = strlen(name);

                    if(len > 5) {
                        const gchar* const ext = ".menu";

                        if (!g_ascii_strncasecmp(ext, &name[len - 5], 5)) {
                            gchar *pn = g_strconcat(path, "/", name, NULL);

                            doc = xmlParseFile(pn);
                            if (doc) {
                                xmlNodePtr cur = xmlDocGetRootElement(doc);

                                if (cur && !xmlStrcasecmp(cur->name, (const xmlChar*)PACKAGE) &&
                                    xmlStrcasecmp(xmlGetProp(cur, ATTR_VERSION), (const xmlChar*)VERSION) <= 0) {
                                    _fill_menu(cur, thing, NULL);
                                success = TRUE;
                                } else /* XML is being kept since we use some strings directly from it */
                                    xmlFreeDoc(doc);
                            }
                            g_free(pn);
                        }
                    }
                }
                g_dir_close(dir);
            }
            g_free(path);
        }
    }

    if (success) {
        gtk_widget_show_all(sample_editor_get_widget("extensions_menu"));
        gtk_widget_show(sample_editor_get_widget("separator3"));
    }
}
