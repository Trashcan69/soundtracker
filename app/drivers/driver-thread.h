
/*
 * The Real SoundTracker - Common driver's multithread stuff (header)
 *
 * Copyright (C) 2018 Yury Aliaev
 * Copyright (C) 2019 Michael Krause
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

#define DRIVER_THREAD_STUFF GThread* tid; GMutex mutex; GCond cond;\
    gboolean running, processing, active;

#define DRIVER_THREAD_LOOP_BEGIN(dp, always_wait) g_mutex_lock(&dp->mutex);\
    dp->running = TRUE;\
    do {\
        if (always_wait || !dp->processing)\
            g_cond_wait(&dp->cond, &dp->mutex);\
    if (!dp->active)\
        break;

#define DRIVER_THREAD_ERROR goto out;

#define DRIVER_THREAD_LOOP_END(dp) } while(dp->active); \
out: \
    g_mutex_unlock(&dp->mutex);\
    dp->running = FALSE;

#define DRIVER_THREAD_INIT(dp) g_mutex_init(&dp->mutex); g_cond_init(&dp->cond);\
    d->running = FALSE; d->processing = FALSE; d->active = FALSE;

#define DRIVER_THREAD_CLEAR(dp) g_mutex_clear(&dp->mutex); g_cond_clear(&dp->cond);

#define DRIVER_THREAD_NEW(dp, loop_fn, orig_ptr, name) if (!dp->running) {\
    dp->active = TRUE;\
    dp->processing = FALSE;\
    dp->tid = g_thread_new(name, loop_fn, orig_ptr);\
}

#define DRIVER_THREAD_CANCEL(dp) if (dp->running) {\
    dp->active = FALSE;\
    dp->processing = FALSE;\
    g_cond_signal(&dp->cond);\
    g_thread_join(dp->tid);\
}

#define DRIVER_THREAD_STOP(dp) if (!dp->running) return;\
    if (!dp->processing) return;\
    dp->processing = FALSE;

#define DRIVER_THREAD_WAIT(dp) g_mutex_lock(&dp->mutex);\
    g_mutex_unlock(&dp->mutex);

#define DRIVER_THREAD_RESUME(dp) if (!dp->running) return FALSE;\
    if (dp->processing) return FALSE;\
    dp->processing = TRUE;\
    g_cond_signal(&dp->cond);

#define DRIVER_THREAD_COMMIT(dp) g_mutex_lock(&dp->mutex);\
    g_cond_signal(&dp->cond);\
    g_mutex_unlock(&dp->mutex);
