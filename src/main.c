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

#include "args.h"
#include "exit_code.h"

#include "nes_emulator.h"
#include "backend/wayland.h"

int main(int argc, char **argv)
{
	struct nes_emulator_console *console;
	struct nes_emulator_cartridge *cartridge;
	struct memory_mapping mm;
	uint8_t exit_code;

	exit_code = init_memory_mapping_from_args(argc, argv, &mm);
	if (exit_code != 0) {
		return exit_code;
	}

	exit_code = nes_emulator_console_init(&console);
	if (exit_code != 0) {
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}

	exit_code = nes_emulator_cartridge_init(&cartridge, mm.data, mm.size);
	if (exit_code != 0) {
		nes_emulator_console_fini(&console);
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}

	struct nes_emulator_ppu_backend *ppu_backend;
	exit_code = nes_emulator_ppu_backend_wayland_init(&ppu_backend);
	if (exit_code != 0) {
		nes_emulator_cartridge_fini(&cartridge);
		nes_emulator_console_fini(&console);
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}
	nes_emulator_console_add_ppu_backend(console, ppu_backend);

	nes_emulator_console_insert_cartridge(console, cartridge);

	while (exit_code == 0) {
		exit_code = nes_emulator_console_step(console);
	}

	exit_code |= nes_emulator_ppu_backend_wayland_fini(&ppu_backend);
	nes_emulator_cartridge_fini(&cartridge);
	nes_emulator_console_fini(&console);
	exit_code |= fini_memory_mapping(&mm);
	return exit_code;
}
