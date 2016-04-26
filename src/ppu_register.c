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

#include "ppu.h"

#include "console.h"

static uint8_t ppu_register_status_read(struct nes_emulator_console *console)
{
	/* Vertical blank has started */
	/* Sprite 0 Hit */
	/* Sprite overflow */
}

static void ppu_register_ctrl_write(struct nes_emulator_console *console,
                                    uint8_t value)
{
	/* Generate an NMI at the start of the vertical blanking interval
	   (0: off; 1: on) */
	uint8_t v = (value & (1 << 7)) >> 7;

	/* PPU master/slave select (0: read backdrop from EXT pins;
	                            1: output color on EXT pins) */
	uint8_t p = (value & (1 << 6)) >> 6;

	/* Sprite size (0: 8x8; 1: 8x16) */
	uint8_t h = (value & (1 << 5)) >> 5;

	/* Background pattern table address (0: $0000; 1: $1000) */
	uint8_t b = (value & (1 << 4)) >> 4;

	/* Sprite pattern table address for 8x8 sprites
	   (0: $0000; 1: $1000; ignored in 8x16 mode) */
	uint8_t s = (value & (1 << 3)) >> 3;

	/* VRAM address increment per CPU read/write of PPUDATA
	   (0: add 1, going across; 1: add 32, going down) */
	uint8_t i = (value & (1 << 2)) >> 2;
	switch (i) {
	case 0:
		console->ppu.computed_address_increment = 1;
		break;
	case 1:
		console->ppu.computed_address_increment = 32;
		break;
	}

	/* Base nametable address
	   (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00) */
	uint8_t n = (value & 0x03);
	switch (n) {
	case 0:
		console->ppu.nametable_address = 0x2000;
		break;
	case 1:
		console->ppu.nametable_address = 0x2400;
		break;
	case 2:
		console->ppu.nametable_address = 0x2800;
		break;
	case 3:
		console->ppu.nametable_address = 0x2C00;
		break;
	}
}

static void ppu_register_data_write(struct nes_emulator_console *console,
                                    uint8_t value)
{
	ppu_bus_write(console, console->ppu.computed_address, value);
	console->ppu.computed_address += console->ppu.computed_address_increment;
}

static uint8_t ppu_register_data_read(struct nes_emulator_console *console)
{
	uint8_t m = ppu_bus_read(console, console->ppu.computed_address);
	console->ppu.computed_address += console->ppu.computed_address_increment;
	return m;
}

static void ppu_register_addr_write(struct nes_emulator_console *console,
                                    uint8_t value)
{
	if (console->ppu.computed_address_is_high) {
		console->ppu.computed_address &= 0x00FF;
		console->ppu.computed_address |= (value << 8);
		console->ppu.computed_address_is_high = false;
	}
	else {
		console->ppu.computed_address &= 0xFF00;
		console->ppu.computed_address |= value;
		console->ppu.computed_address_is_high = true;
	}
}

uint8_t ppu_cpu_bus_read(struct nes_emulator_console *console,
                         uint16_t address)
{
	switch (address % 8) {
	case 0:
		return ppu_register_status_read(console);
	case 7:
		return ppu_register_data_read(console);
	default:
		return 0;
	}
}

void ppu_cpu_bus_write(struct nes_emulator_console *console,
                       uint16_t address,
                       uint8_t value)
{
	switch (address % 8) {
	case 0:
		ppu_register_ctrl_write(console, value);
		break;
	case 6:
		ppu_register_addr_write(console, value);
	case 7:
		ppu_register_data_write(console, value);
	default:
		break;
	}
}
