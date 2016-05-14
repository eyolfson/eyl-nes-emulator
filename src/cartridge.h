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

#ifdef __cpluscplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct nes_emulator_console;

struct nes_emulator_cartridge {
	uint8_t *chr_rom;
	uint8_t *prg_rom_bank_1;
	uint8_t *prg_rom_bank_2;
	uint8_t mirroring;
	bool owns_chr_rom;
};

uint8_t cartridge_cpu_bus_read(struct nes_emulator_console *console,
                               uint16_t address);
void cartridge_cpu_bus_write(struct nes_emulator_console *console,
                             uint16_t address,
                             uint8_t value);
uint8_t cartridge_ppu_bus_read(struct nes_emulator_console *console,
                               uint16_t address);
void cartridge_ppu_bus_write(struct nes_emulator_console *console,
                             uint16_t address,
                             uint8_t value);

#ifdef __cpluscplus
}
#endif
