
/*
 * The Real SoundTracker - time buffer
 *
 * Copyright (C) 1999-2019 Michael Krause
 * Copyright (C) 2020-2023 Yury Aliaev
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

#include "time-buffer.h"

#include <glib.h>

typedef struct time_buffer_item {
    double time;
    /* then user data follows */
} time_buffer_item;

struct time_buffer {
    GMutex mutex;
    GQueue buffer;
};

time_buffer*
time_buffer_new(void)
{
    time_buffer* t = g_new(time_buffer, 1);

    if (t) {
        g_mutex_init(&t->mutex);
        g_queue_init(&t->buffer);
    }

    return t;
}

/* Take care on whether we need to free the buffer contents first
or just use g_list_free_full()
void time_buffer_destroy(time_buffer* t)
{
    if (t) {
        g_list_free(t->list);
        g_mutex_clear(&t->mutex);
        g_free(t);
    }
}
*/

void time_buffer_clear(time_buffer* t)
{
    GList* l;

    for (l = t->buffer.head; l; l = l->next)
        g_free(l->data);
    g_queue_clear(&t->buffer);
}

void
time_buffer_add(time_buffer* t,
    void* item,
    double time)
{
    time_buffer_item* a = item;
    GList *h;

    g_mutex_lock(&t->mutex);
    h = t->buffer.head;
    a->time = time;

    if (h) {
        /* A special case sometimes emerging when samples stop on their own.
           During one time chunk the sample in one channel can finish earlier
           than the sample in one of the previously rendered channels. So a
           kind of sorting is needed */
        if (((time_buffer_item *)h->data)->time > time) {
            while ((h = h->next)) {
                if (((time_buffer_item *)h->data)->time < time) {
                    g_queue_insert_before(&t->buffer, h, item);
                    break;
                }
            }
            if (!h)
                g_queue_push_tail(&t->buffer, item);

            g_mutex_unlock(&t->mutex);
            return;
        }
    }
    g_queue_push_head(&t->buffer, item);
    g_mutex_unlock(&t->mutex);

    return;
}

gboolean time_buffer_foreach(time_buffer* t,
    double time,
    void (*foreach_func)(gpointer data, gpointer user_data),
    gpointer user_data)
{
    GList* l;
    gboolean retval = FALSE;

    g_mutex_lock(&t->mutex);
    while ((l = t->buffer.tail)) {
        gpointer data;

        if (((time_buffer_item*)l->data)->time > time)
            break;
        data = g_queue_pop_tail(&t->buffer);
        foreach_func(data, user_data);
        g_free(data);
        retval = TRUE;
    }
    g_mutex_unlock(&t->mutex);

    return retval;
}

gpointer time_buffer_get(time_buffer* t,
    double time)
{
    gpointer result = NULL;
    GList* l;

    g_mutex_lock(&t->mutex);
    l = t->buffer.tail;

    if (l)
        if (((time_buffer_item*)l->data)->time <= time) {
            for (; l->prev; l = t->buffer.tail) {
                if (((time_buffer_item*)l->prev->data)->time > time)
                    break;
                g_free(g_queue_pop_tail(&t->buffer));
            }
            result = g_queue_pop_tail(&t->buffer);
        }

    g_mutex_unlock(&t->mutex);
    return result;
}
