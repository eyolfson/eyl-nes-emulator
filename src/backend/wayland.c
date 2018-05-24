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

#include "wayland_private.h"

#include "wayland_buffer.h"

#include <string.h>

#include "../exit_code.h"

static const int32_t WIDTH = 256 * SCALE;
static const int32_t HEIGHT = 240 * SCALE;

static void registry_global(void *data,
                            struct wl_registry *wl_registry,
                            uint32_t name,
                            const char *interface,
                            uint32_t version)
{
	struct wayland *wayland = (struct wayland *) data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		wayland->compositor = wl_registry_bind(
			wl_registry, name, &wl_compositor_interface, version);
	}
	else if (strcmp(interface, wl_shm_interface.name) == 0) {
		wayland->shm = wl_registry_bind(
			wl_registry, name, &wl_shm_interface, version);
	}
	else if (strcmp(interface, zxdg_shell_v6_interface.name) == 0) {
		wayland->shell = wl_registry_bind(
			wl_registry, name, &zxdg_shell_v6_interface, version);
	}
	else if (strcmp(interface, wl_seat_interface.name) == 0) {
		wayland->seat = wl_registry_bind(
			wl_registry, name, &wl_seat_interface, version);
	}
}

static void registry_global_remove(void *data,
                                   struct wl_registry *wl_registry,
                                   uint32_t name)
{
	(void) (data);
	(void) (wl_registry);
	(void) (name);
}

static struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static void shell_ping(void *data, struct zxdg_shell_v6 *xdg_shell,
                       uint32_t serial)
{
	(void) (data);

	zxdg_shell_v6_pong(xdg_shell, serial);
}

static struct zxdg_shell_v6_listener shell_listener = {
	.ping = shell_ping,
};

static void shell_surface_configure(void *data,
                                    struct zxdg_surface_v6 *shell_surface,
                                    uint32_t serial)
{
	(void) data;

	zxdg_surface_v6_ack_configure(shell_surface, serial);
}

static struct zxdg_surface_v6_listener shell_surface_listener = {
	.configure = shell_surface_configure,
};

static void toplevel_configure(void *data,
                               struct zxdg_toplevel_v6 *toplevel,
                               int32_t width,
                               int32_t height,
                               struct wl_array *states)
{
	(void) data;
	(void) toplevel;
	(void) width;
	(void) height;
	(void) states;
}

static void toplevel_close(void *data,
                           struct zxdg_toplevel_v6 *toplevel)
{
	(void) data;
	(void) toplevel;
}

static struct zxdg_toplevel_v6_listener toplevel_listener = {
	.configure = toplevel_configure,
	.close = toplevel_close,
};

static void frame_callback_done(void *data,
                                struct wl_callback *wl_callback,
                                uint32_t time);

struct wl_callback_listener frame_callback_listener = {
	.done = frame_callback_done,
};

static void frame_callback_done(void *data,
                                struct wl_callback *wl_callback,
                                uint32_t time)
{
	(void) (data);
	/* TODO: check the time to ensure we don't have a frame miss */
	(void) (time);

	wl_callback_destroy(wl_callback);
}

static void keyboard_keymap(void *data,
                            struct wl_keyboard *wl_keyboard,
                            uint32_t format,
                            int32_t fd,
                            uint32_t size)
{
	(void) (data);
	(void) (wl_keyboard);
	(void) (format);
	(void) (fd);
	(void) (size);
}

static void keyboard_enter(void *data,
                           struct wl_keyboard *wl_keyboard,
                           uint32_t serial,
                           struct wl_surface *surface,
                           struct wl_array *keys)
{
	(void) (data);
	(void) (wl_keyboard);
	(void) (serial);
	(void) (surface);
	(void) (keys);
}

static void keyboard_leave(void *data,
                           struct wl_keyboard *wl_keyboard,
                           uint32_t serial,
                           struct wl_surface *surface)
{
	(void) (data);
	(void) (wl_keyboard);
	(void) (serial);
	(void) (surface);
}

static void keyboard_key(void *data,
                         struct wl_keyboard *wl_keyboard,
                         uint32_t serial,
                         uint32_t time,
                         uint32_t key,
                         uint32_t state)
{
	(void) (data);
	(void) (wl_keyboard);
	(void) (serial);
	(void) (time);

	struct wayland *wayland = (struct wayland *) data;

	const uint8_t BUTTON_A      = 1 << 7;
	const uint8_t BUTTON_B      = 1 << 6;
	const uint8_t BUTTON_SELECT = 1 << 5;
	const uint8_t BUTTON_START  = 1 << 4;
	const uint8_t BUTTON_UP     = 1 << 3;
	const uint8_t BUTTON_DOWN   = 1 << 2;
	const uint8_t BUTTON_LEFT   = 1 << 1;
	const uint8_t BUTTON_RIGHT  = 1 << 0;

	switch (state) {
	case 0:
		switch (key) {
		case 17:
			wayland->joypad1_press &= ~(BUTTON_UP);
			break;
		case 30:
			wayland->joypad1_press &= ~(BUTTON_LEFT);
			break;
		case 31:
			wayland->joypad1_press &= ~(BUTTON_DOWN);
			break;
		case 32:
			wayland->joypad1_press &= ~(BUTTON_RIGHT);
			break;
		case 34:
			wayland->joypad1_press &= ~(BUTTON_SELECT);
			break;
		case 35:
			wayland->joypad1_press &= ~(BUTTON_START);
			break;
		case 37:
			wayland->joypad1_press &= ~(BUTTON_B);
			break;
		case 38:
			wayland->joypad1_press &= ~(BUTTON_A);
			break;
		}
		break;
	case 1:
		switch (key) {
		case 17:
			wayland->joypad1_press |= BUTTON_UP;
			wayland->joypad1_state |= BUTTON_UP;
			break;
		case 30:
			wayland->joypad1_press |= BUTTON_LEFT;
			wayland->joypad1_state |= BUTTON_LEFT;
			break;
		case 31:
			wayland->joypad1_press |= BUTTON_DOWN;
			wayland->joypad1_state |= BUTTON_DOWN;
			break;
		case 32:
			wayland->joypad1_press |= BUTTON_RIGHT;
			wayland->joypad1_state |= BUTTON_RIGHT;
			break;
		case 34:
			wayland->joypad1_press |= BUTTON_SELECT;
			wayland->joypad1_state |= BUTTON_SELECT;
			break;
		case 35:
			wayland->joypad1_press |= BUTTON_START;
			wayland->joypad1_state |= BUTTON_START;
			break;
		case 37:
			wayland->joypad1_press |= BUTTON_B;
			wayland->joypad1_state |= BUTTON_B;
			break;
		case 38:
			wayland->joypad1_press |= BUTTON_A;
			wayland->joypad1_state |= BUTTON_A;
			break;
		}
		break;
	}
}

static void keyboard_modifiers(void *data,
                               struct wl_keyboard *wl_keyboard,
                               uint32_t serial,
                               uint32_t mods_depressed,
                               uint32_t mods_latched,
                               uint32_t mods_locked,
                               uint32_t group)
{
	(void) (data);
	(void) (wl_keyboard);
	(void) (serial);
	(void) (mods_depressed);
	(void) (mods_latched);
	(void) (mods_locked);
	(void) (group);
}

static void keyboard_repeat_info(void *data,
                                 struct wl_keyboard *wl_keyboard,
                                 int32_t rate,
                                 int32_t delay)
{
	(void) (data);
	(void) (wl_keyboard);
	(void) (rate);
	(void) (delay);
}

struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

uint8_t init_wayland(struct wayland *wayland)
{
	wayland->display = wl_display_connect(NULL);
	if (wayland->display == NULL) {
		return EXIT_CODE_WAYLAND_BIT;
	}

	wayland->registry = wl_display_get_registry(wayland->display);
	if (wayland->registry == NULL) {
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}

	wl_registry_add_listener(wayland->registry, &registry_listener,
	                         wayland);
	wl_display_dispatch(wayland->display);
	if (wayland->compositor == NULL || wayland->shm == NULL
	    || wayland->shell == NULL || wayland->seat == NULL) {
		if (wayland->compositor != NULL) {
			wl_compositor_destroy(wayland->compositor);
		}
		if (wayland->shm != NULL) {
			wl_shm_destroy(wayland->shm);
		}
		if (wayland->shell != NULL) {
			zxdg_shell_v6_destroy(wayland->shell);
		}
		if (wayland->seat != NULL) {
			wl_seat_destroy(wayland->seat);
		}
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}

	zxdg_shell_v6_add_listener(wayland->shell, &shell_listener, NULL);

	wayland->surface = wl_compositor_create_surface(wayland->compositor);
	if (wayland->surface == NULL) {
		wl_seat_destroy(wayland->seat);
		zxdg_shell_v6_destroy(wayland->shell);
		wl_shm_destroy(wayland->shm);
		wl_compositor_destroy(wayland->compositor);
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}

	wayland->shell_surface = zxdg_shell_v6_get_xdg_surface(
		wayland->shell, wayland->surface);
	if (wayland->shell_surface == NULL) {
		wl_surface_destroy(wayland->surface);
		wl_seat_destroy(wayland->seat);
		zxdg_shell_v6_destroy(wayland->shell);
		wl_shm_destroy(wayland->shm);
		wl_compositor_destroy(wayland->compositor);
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}
	zxdg_surface_v6_add_listener(wayland->shell_surface,
	                             &shell_surface_listener, NULL);

	wayland->width = WIDTH;
	wayland->height = HEIGHT;

	wayland->toplevel = zxdg_surface_v6_get_toplevel(wayland->shell_surface);
	if (wayland->toplevel == NULL) {
		zxdg_surface_v6_destroy(wayland->shell_surface);
		wl_surface_destroy(wayland->surface);
		wl_seat_destroy(wayland->seat);
		zxdg_shell_v6_destroy(wayland->shell);
		wl_shm_destroy(wayland->shm);
		wl_compositor_destroy(wayland->compositor);
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}
	zxdg_toplevel_v6_add_listener(wayland->toplevel, &toplevel_listener,
	                              NULL);

	zxdg_toplevel_v6_set_title(wayland->toplevel, "NES Emulator");
	zxdg_toplevel_v6_set_app_id(wayland->toplevel, "io.eyl.NESEmulator");
	zxdg_surface_v6_set_window_geometry(wayland->shell_surface,
	                                    0, 0,
	                                    wayland->width, wayland->height);
	wl_surface_commit(wayland->surface);
	wl_display_roundtrip(wayland->display);

	wayland->keyboard = wl_seat_get_keyboard(wayland->seat);
	if (wayland->keyboard == NULL) {
		zxdg_toplevel_v6_destroy(wayland->toplevel);
		zxdg_surface_v6_destroy(wayland->shell_surface);
		wl_surface_destroy(wayland->surface);
		wl_seat_destroy(wayland->seat);
		zxdg_shell_v6_destroy(wayland->shell);
		wl_shm_destroy(wayland->shm);
		wl_compositor_destroy(wayland->compositor);
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}
	wl_keyboard_add_listener(wayland->keyboard, &keyboard_listener,
	                         wayland);

	uint8_t exit_code = init_wayland_buffer(wayland);
	if (exit_code != 0) {
		wl_keyboard_release(wayland->keyboard);
		zxdg_toplevel_v6_destroy(wayland->toplevel);
		zxdg_surface_v6_destroy(wayland->shell_surface);
		wl_surface_destroy(wayland->surface);
		wl_seat_destroy(wayland->seat);
		zxdg_shell_v6_destroy(wayland->shell);
		wl_shm_destroy(wayland->shm);
		wl_compositor_destroy(wayland->compositor);
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return exit_code;
	}

	for (int64_t i = 0; i < (wayland->width * wayland->height); ++i) {
		wayland->front_data[i] = 0xFF0000FF;
	}
	for (int64_t i = 0; i < (wayland->width * wayland->height); ++i) {
		wayland->back_data[i] = 0xFF00FF00;
	}

	wayland->frame_callback = wl_surface_frame(wayland->surface);
	if (wayland->frame_callback == NULL) {
		exit_code = EXIT_CODE_WAYLAND_BIT;
		exit_code |= fini_wayland_buffer(wayland);
		wl_keyboard_release(wayland->keyboard);
		zxdg_toplevel_v6_destroy(wayland->toplevel);
		zxdg_surface_v6_destroy(wayland->shell_surface);
		wl_surface_destroy(wayland->surface);
		wl_seat_destroy(wayland->seat);
		zxdg_shell_v6_destroy(wayland->shell);
		wl_shm_destroy(wayland->shm);
		wl_compositor_destroy(wayland->compositor);
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return exit_code;
	}

	wl_callback_add_listener(wayland->frame_callback,
	                         &frame_callback_listener, wayland);

	wl_surface_damage(wayland->surface, 0, 0,
	                  wayland->width, wayland->height);
	wl_surface_attach(wayland->surface, wayland->front_buffer, 0, 0);
	wl_surface_commit(wayland->surface);

	wl_display_flush(wayland->display);

	return 0;
}

uint8_t fini_wayland(struct wayland *wayland)
{
	wl_display_roundtrip(wayland->display);
	zxdg_toplevel_v6_destroy(wayland->toplevel);
	wl_keyboard_release(wayland->keyboard);
	wl_seat_destroy(wayland->seat);
	uint8_t exit_code = fini_wayland_buffer(wayland);
	zxdg_surface_v6_destroy(wayland->shell_surface);
	wl_surface_destroy(wayland->surface);
	zxdg_shell_v6_destroy(wayland->shell);
	wl_shm_destroy(wayland->shm);
	wl_compositor_destroy(wayland->compositor);
	wl_registry_destroy(wayland->registry);
	wl_display_disconnect(wayland->display);
	return exit_code;
}
