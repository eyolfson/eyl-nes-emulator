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

#include "args.h"
#include "exit_code.h"
#include "cpu.h"
#include "prg_rom.h"
#include "utils.h"
#include "wayland.h"

uint8_t main(int argc, char **argv)
{
	struct memory_mapping mm;
	uint8_t exit_code;

	exit_code = init_memory_mapping_from_args(argc, argv, &mm);

	if (exit_code == 0) {
		exit_code = check_rom_size_raw(mm.data, mm.size);
	}

	load_rom_into_memory(mm.data, mm.size);

	if (exit_code == 0) {
		struct registers registers;
		init_registers(&registers);
		reset_program_counter(&registers);
		while (exit_code == 0) {
			exit_code = execute_instruction(&registers);
		}
	}

	exit_code |= fini_memory_mapping(&mm);
	return exit_code;
}
