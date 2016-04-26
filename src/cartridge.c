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

#include "cartridge.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "console.h"
#include "exit_code.h"

static const uint16_t HEADER_SIZE = 16;               /* 16 B */
static const uint16_t PRG_ROM_SIZE_PER_UNIT = 0x4000; /* 16 KiB */
static const uint16_t CHR_ROM_SIZE_PER_UNIT = 0x2000; /*  8 KiB */

uint8_t nes_emulator_cartridge_init(struct nes_emulator_cartridge **cartridge,
                                    uint8_t *data,
                                    size_t size)
{
	struct nes_emulator_cartridge *c;

	c = malloc(sizeof(struct nes_emulator_cartridge));
	if (c == NULL) {
		return EXIT_CODE_OS_ERROR_BIT;
	}

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

	if (size != expected_size) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	c->prg_rom_bank_1 = data + HEADER_SIZE;
	if (prg_rom_units == 1) {
		c->prg_rom_bank_2 = data + HEADER_SIZE;
	}
	else if (prg_rom_units == 2) {
		c->prg_rom_bank_2 = data + HEADER_SIZE + PRG_ROM_SIZE_PER_UNIT;
	}

	/* TODO: Assume horizontal arrangement / vertical mirroring */
	if ((data[6] != 0x00) && (data[6] != 0x01)) {
		return EXIT_CODE_UNIMPLEMENTED_BIT;
	}

	if (data[6] == 0x01) {
		c->chr_rom = data + HEADER_SIZE
		             + PRG_ROM_SIZE_PER_UNIT * prg_rom_units;
	}

	*cartridge = c;
	return 0;
}

void nes_emulator_cartridge_fini(struct nes_emulator_cartridge **cartridge)
{
	free(*cartridge);
	*cartridge = NULL;
}

uint8_t cartridge_cpu_bus_read(struct nes_emulator_console *console,
                               uint16_t address)
{
	if (address < 0x8000) {
		return 0;
	}
	else if (address < 0xC000) {
		return *(console->cartridge->prg_rom_bank_1 + (address - 0x8000));
	}
	else {
		return *(console->cartridge->prg_rom_bank_2 + (address - 0xC000));
	}
}

void cartridge_cpu_bus_write(struct nes_emulator_console *console,
                             uint16_t address,
                             uint8_t value)
{
}

uint8_t cartridge_ppu_bus_read(struct nes_emulator_console *console,
                               uint16_t address)
{
	return *(console->cartridge->chr_rom);
}
