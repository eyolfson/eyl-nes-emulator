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

#include "ppu.h"

#include "console.h"

#include <stdio.h>

static void ppu_register_ctrl_write(struct nes_emulator_console *console,
                                    uint8_t value)
{
	/* Generate an NMI at the start of the vertical blanking interval
	   (0: off; 1: on) */
	uint8_t v = (value & (1 << 7)) >> 7;
	if (v == 0) {
		console->ppu.nmi_output = false;
	}
	else {
		/* Generate another NMI if it's toggled */
		if (!(console->ppu.nmi_output)
		    && console->ppu.nmi_occurred) {
			cpu_generate_nmi(console);
			/* Allow the CPU to execute it's next instruction */
			console->cpu.nmi_delay = true;
		}
		console->ppu.nmi_output = true;
	}

	/* PPU master/slave select (0: read backdrop from EXT pins;
	                            1: output color on EXT pins) */
	/* TODO: Remove */
	uint8_t p = (value & (1 << 6)) >> 6;
	if (p == 1) { printf("PPU master/slave select unimplemented\n"); }

	/* Sprite size (0: 8x8; 1: 8x16) */
	uint8_t h = (value & (1 << 5)) >> 5;
	/* TODO: Remove */
	if (h == 1) { printf("PPU sprite size unimplemented\n"); }

	/* Background pattern table address (0: $0000; 1: $1000) */
	uint8_t b = (value & (1 << 4)) >> 4;
	if (b == 0) {
		console->ppu.background_address = 0x0000;
	}
	else {
		console->ppu.background_address = 0x1000;
	}

	/* Sprite pattern table address for 8x8 sprites
	   (0: $0000; 1: $1000; ignored in 8x16 mode) */
	uint8_t s = (value & (1 << 3)) >> 3;
	if (s == 0) {
		console->ppu.sprite_address = 0x0000;
	}
	else {
		console->ppu.sprite_address = 0x1000;
	}

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

	uint16_t t = console->ppu.internal_registers.t;
	t &= ~(0x0003 << 10);
	t += (n << 10);
	console->ppu.internal_registers.t = t;
}

static void ppu_register_mask_write(struct nes_emulator_console *console,
                                    uint8_t value)
{
	console->ppu.mask = value;
}

static uint8_t ppu_register_status_read(struct nes_emulator_console *console)
{
	uint8_t value = 0;
	/* Vertical blank has started */
	if (console->ppu.nmi_occurred) {
		/* TODO: This should probably be cycle 1, odd-even timing? */
		if (!(console->ppu.scan_line == 241
		      && console->ppu.cycle == 2)) {
			value |= 0x80;
		}
		console->ppu.nmi_occurred = false;
	}
	/* Sprite 0 Hit */
	if (console->ppu.is_sprite_0_hit) {
		value |= 0x40;
		console->ppu.is_sprite_0_hit = false;
	}

	/* Sprite overflow */
	if (console->ppu.is_sprite_overflow) {
		value |= 0x20;
		console->ppu.is_sprite_overflow = false;
	}

	console->ppu.internal_registers.w = 0;

	return value;
}

static void ppu_register_oam_addr_write(struct nes_emulator_console *console,
                                        uint8_t value)
{
	console->ppu.oam_address = value;
}

static void ppu_register_oam_data_write(struct nes_emulator_console *console,
                                        uint8_t value)
{
	console->ppu.oam[console->ppu.oam_address] = value;
	console->ppu.oam_address += 1;
}

static void ppu_register_scroll_write(struct nes_emulator_console *console,
                                      uint8_t value)
{
	if (console->ppu.scroll_is_x) {
		console->ppu.scroll_x = value;
		console->ppu.scroll_is_x = false;
		if (value != 0) { printf("PPU x scrolling unimplemented\n"); }
	}
	else {
		console->ppu.scroll_y = value;
		console->ppu.scroll_is_x = true;
		if (value != 0) { printf("PPU y scrolling unimplemented\n"); }
	}

	if (console->ppu.internal_registers.w == 0) {
		uint8_t x = value;
		uint8_t coarse_x = (x & 0xF8) >> 3;
		uint8_t fine_x = x & 0x07;
		console->ppu.internal_registers.x = fine_x;
		uint16_t t = console->ppu.internal_registers.t;
		t &= ~(0x001F);
		t += coarse_x;
		console->ppu.internal_registers.t = t;
		console->ppu.internal_registers.w = 1;
	}
	else {
		uint8_t y = value;
		uint8_t coarse_y = (y & 0xF8) >> 3;
		uint8_t fine_y = y & 0x07;
		uint16_t t = console->ppu.internal_registers.t;
		t &= ~(0x73E0);
		t += fine_y << 12;
		t += coarse_y << 5;
		console->ppu.internal_registers.t = t;
		console->ppu.internal_registers.w = 0;
	}
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

	if (console->ppu.internal_registers.w == 0) {
		uint8_t d = value;
		d &= 0x3F;
		uint16_t t = console->ppu.internal_registers.t;
		t &= ~(0xFF00);
		t += (d << 8);
		console->ppu.internal_registers.t = t;
		console->ppu.internal_registers.w = 1;
	}
	else {
		uint8_t d = value;
		uint16_t t = console->ppu.internal_registers.t;
		t += d;
		t &= ~(0x00FF);
		console->ppu.internal_registers.t = t;
		console->ppu.internal_registers.v = t;
		console->ppu.internal_registers.w = 0;
	}
}

static uint8_t ppu_register_data_read(struct nes_emulator_console *console)
{
	uint8_t m = ppu_bus_read(console, console->ppu.computed_address);
	console->ppu.computed_address += console->ppu.computed_address_increment;
	return m;
}

static void ppu_register_data_write(struct nes_emulator_console *console,
                                    uint8_t value)
{
	ppu_bus_write(console, console->ppu.computed_address, value);
	console->ppu.computed_address += console->ppu.computed_address_increment;
}

uint8_t ppu_cpu_bus_read(struct nes_emulator_console *console,
                         uint16_t address)
{
	switch (address % 8) {
	case 2:
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
	case 1:
		ppu_register_mask_write(console, value);
		break;
	case 3:
		ppu_register_oam_addr_write(console, value);
		break;
	case 4:
		ppu_register_oam_data_write(console, value);
		break;
	case 5:
		ppu_register_scroll_write(console, value);
		break;
	case 6:
		ppu_register_addr_write(console, value);
		break;
	case 7:
		ppu_register_data_write(console, value);
		break;
	default:
		break;
	}
}
