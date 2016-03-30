/*
 * Copyright 2016 Jonathan Eyolfson
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "wayland.h"

#include <string.h>

#include "exit_code.h"

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

	xdg_surface_set_title(wayland->shell_surface, "NES Emulator");

	return 0;
}

uint8_t fini_wayland(struct wayland *wayland)
{
	xdg_surface_destroy(wayland->shell_surface);
	wl_surface_destroy(wayland->surface);
	xdg_shell_destroy(wayland->shell);
	wl_shm_destroy(wayland->shm);
	wl_compositor_destroy(wayland->compositor);
	wl_registry_destroy(wayland->registry);
	wl_display_disconnect(wayland->display);
	return 0;
}
