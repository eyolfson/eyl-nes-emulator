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

#include "utils.h"

#include "cpu.h"
#include "exit_code.h"
#include "memory_bus.h"
#include "prg_rom.h"

static const uint16_t HEADER_SIZE = 16;               /* 16 B */
static const uint16_t PRG_ROM_SIZE_PER_UNIT = 0x4000; /* 16 KiB */
static const uint16_t CHR_ROM_SIZE_PER_UNIT = 0x2000; /*  8 KiB */

uint8_t check_rom_size_raw(uint8_t *data, size_t size)
{
	if (data[0] != 'N') {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
	if (data[1] != 'E') {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
	if (data[2] != 'S') {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
	if (data[3] != 0x1A) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	uint8_t prg_rom_units = data[4];
	if (prg_rom_units > 2) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	uint8_t chr_rom_units = data[5];
	if (chr_rom_units > 1) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	uint16_t expected_size = HEADER_SIZE;
	expected_size += prg_rom_units * PRG_ROM_SIZE_PER_UNIT;
	expected_size += chr_rom_units * CHR_ROM_SIZE_PER_UNIT;

	if (size == expected_size) {
		return 0;
	}
	else {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
}

void load_rom_into_memory(uint8_t *data, size_t size)
{
	uint8_t prg_rom_units = data[4];

	prg_rom_set_bank_1(data + HEADER_SIZE);

	if (prg_rom_units == 1) {
		prg_rom_set_bank_2(data + HEADER_SIZE);
	}
	else if (prg_rom_units == 2) {
		prg_rom_set_bank_2(data + HEADER_SIZE + PRG_ROM_SIZE_PER_UNIT);
	}
}

void reset_program_counter(struct registers *registers)
{
	registers->pc = memory_read(0xFFFC) + (memory_read(0xFFFD) << 8);
}
