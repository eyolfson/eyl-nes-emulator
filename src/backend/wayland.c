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

static const int32_t WIDTH = 256 * 4;
static const int32_t HEIGHT = 240 * 4;

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
	else if (strcmp(interface, xdg_shell_interface.name) == 0) {
		wayland->shell = wl_registry_bind(
			wl_registry, name, &xdg_shell_interface, version);
	}
}

static void registry_global_remove(void *data,
                                   struct wl_registry *wl_registry,
                                   uint32_t name)
{
}

static struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static void shell_ping(void *data, struct xdg_shell *xdg_shell, uint32_t serial)
{
	xdg_shell_pong(xdg_shell, serial);
}

static struct xdg_shell_listener shell_listener = {
	.ping = shell_ping,
};

static void frame_callback_done(void *data, struct wl_callback *wl_callback,
                                uint32_t time);

struct wl_callback_listener frame_callback_listener = {
	.done = frame_callback_done,
};

static void frame_callback_done(void *data, struct wl_callback *wl_callback,
                                uint32_t time)
{
	/* TODO: check the time to ensure we don't have a frame miss */
}

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
	    || wayland->shell == NULL) {
		if (wayland->compositor != NULL) {
			wl_compositor_destroy(wayland->compositor);
		}
		if (wayland->shm != NULL) {
			wl_shm_destroy(wayland->shm);
		}
		if (wayland->shell != NULL) {
			xdg_shell_destroy(wayland->shell);
		}
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}

	xdg_shell_use_unstable_version(wayland->shell,
	                               XDG_SHELL_VERSION_CURRENT);
	xdg_shell_add_listener(wayland->shell, &shell_listener, NULL);

	wayland->surface = wl_compositor_create_surface(wayland->compositor);
	if (wayland->surface == NULL) {
		xdg_shell_destroy(wayland->shell);
		wl_shm_destroy(wayland->shm);
		wl_compositor_destroy(wayland->compositor);
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}

	wayland->shell_surface = xdg_shell_get_xdg_surface(wayland->shell,
	                                                   wayland->surface);
	if (wayland->shell_surface == NULL) {
		wl_surface_destroy(wayland->surface);
		xdg_shell_destroy(wayland->shell);
		wl_shm_destroy(wayland->shm);
		wl_compositor_destroy(wayland->compositor);
		wl_registry_destroy(wayland->registry);
		wl_display_disconnect(wayland->display);
		return EXIT_CODE_WAYLAND_BIT;
	}

	wayland->width = WIDTH;
	wayland->height = HEIGHT;

	xdg_surface_set_title(wayland->shell_surface, "NES Emulator");
	xdg_surface_set_window_geometry(wayland->shell_surface,
	                                0, 0,
	                                wayland->width, wayland->height);

	uint8_t exit_code = init_wayland_buffer(wayland);
	if (exit_code != 0) {
		xdg_surface_destroy(wayland->shell_surface);
		wl_surface_destroy(wayland->surface);
		xdg_shell_destroy(wayland->shell);
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
		xdg_surface_destroy(wayland->shell_surface);
		wl_surface_destroy(wayland->surface);
		xdg_shell_destroy(wayland->shell);
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
	wl_callback_destroy(wayland->frame_callback);
	uint8_t exit_code = fini_wayland_buffer(wayland);
	xdg_surface_destroy(wayland->shell_surface);
	wl_surface_destroy(wayland->surface);
	xdg_shell_destroy(wayland->shell);
	wl_shm_destroy(wayland->shm);
	wl_compositor_destroy(wayland->compositor);
	wl_registry_destroy(wayland->registry);
	wl_display_disconnect(wayland->display);
	return exit_code;
}
