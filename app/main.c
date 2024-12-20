
/*
 * The Real SoundTracker - main program
 *
 * Copyright (C) 1998-2019 Michael Krause
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "audio.h"
#include "audioconfig.h"
#include "file-operations.h"
#include "gui-settings.h"
#include "gui.h"
#include "history.h"
#include "keys.h"
#include "midi-settings.h"
#include "midi.h"
#include "preferences.h"
#include "tips-dialog.h"
#include "track-editor.h"
#include "xm.h"

#include <glib.h>
#include <gtk/gtk.h>

XM* xm = NULL;

#ifndef DEBUG
static void
sigsegv_handler(int parameter)
{
    signal(SIGSEGV, SIG_DFL);

    if (xm != NULL) {
        gboolean is_error = XM_Save(xm, "crash-save.xm", "crash-save.xm", FALSE, FALSE);
        printf("*** SIGSEGV caught.\n*** Saved current XM to 'crash-save.xm' in current directory.\n    (%s)\n", is_error ? "failed" : "succeed");
        exit(1);
    }
}
#endif

int main(int argc,
    char* argv[])
{
    extern st_driver
        driver_out_dummy,
        driver_in_dummy,
#ifdef DRIVER_OSS
        driver_out_oss, driver_in_oss,
#endif
#ifdef DRIVER_ALSA
        driver_out_alsa1x, driver_in_alsa1x,
#endif
#ifdef DRIVER_SGI
        driver_out_irix,
#endif
#ifdef DRIVER_JACK
        driver_out_jack, driver_in_jack,
#endif
#ifdef DRIVER_SUN
        driver_out_sun, driver_in_sun,
#endif
#ifdef DRIVER_PULSE
        driver_out_pulse,
#endif
#ifdef DRIVER_SDL
        driver_out_sdl,
#endif
#if 0
	driver_out_test,
#endif
#if defined(_WIN32)
        driver_out_dsound,
#endif
        mixer_kbfloat,
        mixer_integer32;

    /* Preventing any logging during initialization */
    history_skip = TRUE;
    if (!audio_init()) {
        fprintf(stderr, "Can't init audio thread.\n");
        return 1;
    }

    /* In case we run setuid root, the main thread must not have root
       privileges -- it must be set back to the calling user ID!  The
       setresuid() stuff is for gtk+-1.2.10 on Linux. */
#ifdef HAVE_SETRESUID
    /* These aren't in the header files, so we prototype them here.
     */
    {
        int setresuid(uid_t ruid, uid_t euid, uid_t suid);
        int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
        setresuid(getuid(), getuid(), getuid());
        setresgid(getgid(), getgid(), getgid());
    }
#else
    seteuid(getuid());
    setegid(getgid());
#endif

#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);
#endif

    gtk_init(&argc, &argv);
    prefs_init();
    tips_dialog_load_settings();

    if (!gui_splash()) {
        fprintf(stderr, "GUI Initialization failed.\n");
        return 1;
    }

    /* To decrease distance between list rows */
    gtk_rc_parse_string("style \"compact\" { GtkTreeView::vertical-separator = 0 }\n"
                        "class \"GtkTreeView\" style \"compact\"\n"
                        /* To cause keycodes to be displayed as list */
                        "style \"list\" {GtkComboBox::appears-as-list = 1}\n"
                        "widget \"*.keyconfig_combo\" style \"list\"");

    mixers = g_list_append(mixers,
        &mixer_kbfloat);
    mixers = g_list_append(mixers,
        &mixer_integer32);

#if 0
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_test);
#endif

#ifdef DRIVER_OSS
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_oss);
    drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
        &driver_in_oss);
#endif

#ifdef DRIVER_SGI
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_irix);
#endif

#ifdef DRIVER_JACK
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_jack);
    drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
        &driver_in_jack);
#endif

#ifdef DRIVER_SUN
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_sun);
    drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
        &driver_in_sun);
#endif

#ifdef _WIN32
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_dsound);
#endif

#ifdef DRIVER_ALSA
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_alsa1x);
    drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
        &driver_in_alsa1x);
#endif

#ifdef DRIVER_ESD
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_esd);
#endif

#ifdef DRIVER_SDL
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_sdl);
#endif

#ifdef DRIVER_PULSE
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
        &driver_out_pulse);
#endif

    if (g_list_length(drivers[DRIVER_OUTPUT]) == 0) {
        drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
            &driver_out_dummy);
    }
    if (g_list_length(drivers[DRIVER_INPUT]) == 0) {
        drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
            &driver_in_dummy);
    }

    g_assert(g_list_length(mixers) >= 1);

    fileops_tmpclean();
    gui_settings_load_config();
    audioconfig_load_mixer_config(); // in case gui_init already loads a module

    if (gui_final(argc, argv)) {
        audioconfig_load_config();
        track_editor_load_config();
#if defined(DRIVER_ALSA)
        midi_load_config();
        midi_init();
#endif
#ifndef DEBUG
        signal(SIGSEGV, sigsegv_handler);
#endif
        history_skip = FALSE;
        gtk_main();

        gui_play_stop(); /* so that audio driver is shut down correctly. */

        if (gui_settings.save_settings_on_exit) {
            keys_save_config();
            gui_settings_save_config();
            audioconfig_save_config();
        }
        gui_settings_save_config_always();
        tips_dialog_save_settings();
        track_editor_save_config();
#if defined(DRIVER_ALSA)
        midi_save_config();
        midi_fini();
#endif
        prefs_save();
        prefs_close();

        fileops_tmpclean();
        audioconfig_shutdown(); /* Closing all opened drivers */
        return 0;
    } else {
        fprintf(stderr, "GUI Initialization failed.\n");
        return 1;
    }
}
