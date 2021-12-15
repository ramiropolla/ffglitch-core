/*
 * Copyright (C) 2022 Ramiro Polla
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef AVUTIL_QUICKJS_SDL_H
#define AVUTIL_QUICKJS_SDL_H

#include <SDL.h>

#include "libavutil/quickjs/quickjs-libc.h"

void ff_quickjs_sdl_init(JSContext *ctx, JSValueConst global_object);
int ff_quickjs_sdl_do_events(void);
void ff_quickjs_sdl_event_add(const SDL_Event *event);

#endif
