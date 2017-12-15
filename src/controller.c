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

#include "controller.h"

#include "console.h"

void nes_emulator_console_add_controller_backend(
	struct nes_emulator_console *console,
	struct nes_emulator_controller_backend *controller_backend)
{
	if (console->controller == NULL) {
		console->controller = controller_backend;
	}
}

uint8_t controller_read(struct nes_emulator_console *console)
{
	/* TODO: indicate that only one controller supported */
	if (console->controller != NULL) {
		struct nes_emulator_controller_backend *backend;
		backend = console->controller;
		return backend->controller1_read(backend->pointer);
	}

	for (size_t i = 0; i < PPU_BACKENDS_MAX; ++i) {
		struct nes_emulator_ppu_backend *backend;
		backend = console->ppu.backends[i];
		if (backend != NULL) {
			return backend->joypad1_read(backend->pointer);
		}
	}
	return 0;
}
