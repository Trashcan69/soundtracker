
/*
 * The Real SoundTracker - error handling functions
 *
 * Copyright (C) 1999-2019 Michael Krause
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

#include "audio-subs.h"

void error_error(const char* text)
{
    audio_backpipe_write(AUDIO_BACKPIPE_ERROR_MESSAGE, text);
}

void error_warning(const char* text)
{
    audio_backpipe_write(AUDIO_BACKPIPE_WARNING_MESSAGE, text);
}

void error_errno(const char *text)
{
    audio_backpipe_write(AUDIO_BACKPIPE_ERRNO_MESSAGE, text, (gint)errno);
}
