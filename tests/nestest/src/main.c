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

#include "../../../src/args.h"
#include "../../../src/exit_code.h"
#include "../../../src/console.h"
#include "../../../src/cpu.h"
#include "../../../src/nes_emulator.h"

#include <stdio.h>

int main(int argc, char **argv)
{
	struct memory_mapping mm;
	uint8_t exit_code;

	exit_code = init_memory_mapping_from_args(argc, argv, &mm);
	if (exit_code != 0) {
		return exit_code;
	}

	struct nes_emulator_console *console;
	exit_code = nes_emulator_console_init(&console);
	if (exit_code != 0) {
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}

	struct nes_emulator_cartridge *cartridge;
	exit_code = nes_emulator_cartridge_init(&cartridge, mm.data, mm.size);
	if (exit_code != 0) {
		nes_emulator_console_fini(&console);
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}

	nes_emulator_console_insert_cartridge(console, cartridge);

	struct registers *registers = &console->cpu.registers;
	registers->pc = 0xC000;
	while (exit_code == 0) {
		printf("%04X "
		       "                                           "
		       "A:%02X X:%02X Y:%02X P:%02X SP:%02X "
		       "CYC:%3d SL:%d\n",
		       registers->pc, registers->a, registers->x,
		       registers->y, registers->p, registers->s,
		       console->ppu.cycle, console->ppu.scan_line);
		exit_code = nes_emulator_console_step(console);
		if (registers->pc == 0x0001) { break; }
	}

	nes_emulator_cartridge_fini(&cartridge);
	nes_emulator_console_fini(&console);
	exit_code |= fini_memory_mapping(&mm);
	return exit_code;
}
