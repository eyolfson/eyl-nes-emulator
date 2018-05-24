/*
 * Copyright 2016 Jonathan Eyolfson
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <wayland-client-protocol.h>

#include "xdg-shell-client-protocol.h"

#define SCALE 2

struct wayland {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zxdg_shell_v6 *shell;
	struct wl_surface *surface;
	struct zxdg_surface_v6 *shell_surface;
	struct zxdg_toplevel_v6 *toplevel;
	int32_t width;
	int32_t height;
	int32_t fd;
	uint32_t *data;
	size_t capacity;
	uint32_t *front_data;
	uint32_t *back_data;
	struct wl_shm_pool *shm_pool;
	struct wl_buffer *front_buffer;
	struct wl_buffer *back_buffer;
	struct wl_callback *frame_callback;

	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	uint8_t joypad1_press;
	uint8_t joypad1_state;
};

uint8_t init_wayland(struct wayland *wayland);
uint8_t fini_wayland(struct wayland *wayland);

struct wl_callback_listener frame_callback_listener;

#ifdef __cplusplus
}
#endif
