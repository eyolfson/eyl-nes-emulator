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

#include "cartridge.h"
#include "console.h"

/* TODO: Assume horizontal arrangement / vertical mirroring (64x30 tilemap) */
/* Each nametable is 1024 bytes (0x400) */
/* It consists of 960 8x8 tiles to form the background */
/* Each of these tiles are a byte */
/* 32 horizontal, 30 vertical tiles */
/* There are 64 bytes remaining */
/* Screen is divided into 64 32x32 tiles called attribute tiles */

static uint8_t palette_ppu_bus_read(struct nes_emulator_console *console,
                                    uint16_t address)
{
	uint8_t i = address - 0x3F00;
	switch (i) {
	case 0x00:
	case 0x10:
		return console->ppu.palette[0x00];
	case 0x04:
	case 0x14:
		return console->ppu.palette[0x04];
	case 0x08:
	case 0x18:
		return console->ppu.palette[0x08];
	case 0x0C:
	case 0x1C:
		return console->ppu.palette[0x0C];
	default:
		return console->ppu.palette[i];
	}
}

static void palette_ppu_bus_write(struct nes_emulator_console *console,
                                  uint16_t address,
                                  uint8_t value)
{
	uint8_t i = address - 0x3F00;
	switch (i) {
	case 0x00:
	case 0x10:
		console->ppu.palette[0x00] = value;
		break;
	case 0x04:
	case 0x14:
		console->ppu.palette[0x04] = value;
		break;
	case 0x08:
	case 0x18:
		console->ppu.palette[0x08] = value;
		break;
	case 0x0C:
	case 0x1C:
		console->ppu.palette[0x0C] = value;
		break;
	default:
		console->ppu.palette[i] = value;
		break;
	}
}

uint8_t ppu_bus_read(struct nes_emulator_console *console,
                     uint16_t address)
{
	if (address < 0x2000) {
		return cartridge_ppu_bus_read(console, address);
	}
	else if (address < 0x2800) {
		return console->ppu.ram[address - 0x2000];
	}
	else if (address < 0x3000) {
		/* Mirroring */
		return console->ppu.ram[address - 0x2800];
	}
	else if (address < 0x3F00) {
		return ppu_bus_read(console, address - 0x1000);
	}
	else if (address < 0x3F20) {
		return palette_ppu_bus_read(console, address);
	}
	else {
		uint16_t mirrored_address = (address % 0x20) + 0x3F00;
		return palette_ppu_bus_read(console, mirrored_address);
	}
}

void ppu_bus_write(struct nes_emulator_console *console,
                   uint16_t address,
                   uint8_t value)
{
	if (address < 0x2000) {
		return;
	}
	else if (address < 0x2800) {
		console->ppu.ram[address - 0x2000] = value;
	}
	else if (address < 0x3000) {
		console->ppu.ram[address - 0x2800] = value;
	}
	else if (address < 0x3F00) {
		ppu_bus_write(console, address - 0x1000, value);
	}
	else if (address < 0x3F20) {
		palette_ppu_bus_write(console, address, value);
	}
	else {
		uint16_t mirrored_address = (address % 0x20) + 0x3F00;
		palette_ppu_bus_write(console, mirrored_address, value);
	}
}

void ppu_init(struct nes_emulator_console *console)
{
	console->ppu.computed_address_is_high = true;
	console->ppu.computed_address_increment = 1;
	console->ppu.computed_address = 0;
	console->ppu.oam_address = 0;
	console->ppu.background_address = 0x1000;
	console->ppu.sprite_address = 0x0000;
	console->ppu.nametable_address = 0x2000;
	console->ppu.generate_nmi = false;
	console->ppu.trigger_vblank = false;
	console->ppu.scroll_is_x = true;
	console->ppu.scroll_x = 0;
	console->ppu.scroll_y = 0;
	console->ppu.cycle = 0;
	console->ppu.scan_line = 241;
}

#include <stdio.h>

static void debug_tile(struct nes_emulator_console *console,
                       uint16_t address)
{
	uint8_t color_index[8][8] = { 0 };
	uint8_t x = 0;
	for (uint16_t i = address; i < address + 8; ++i) {
		uint8_t b = ppu_bus_read(console, i);
		for (uint8_t j = 0; j < 8; ++j) {
			if (b & (1 << j)) {
				color_index[x][7 - j] |= 0x02;
			}
		}
		++x;
	}
	x = 0;
	for (uint16_t i = address + 8; i < address + 16; ++i) {
		uint8_t b = ppu_bus_read(console, i);
		for (uint8_t j = 0; j < 8; ++j) {
			if (b & (1 << j)) {
				color_index[x][7 - j] |= 0x01;
			}
		}
		++x;
	}

	printf("Tile 0x%04x\n", address);
	for (uint8_t i = 0; i < 8; ++i) {
		for (uint8_t j = 0; j < 8; ++j) {
			if (j != 0) { printf(" "); }
			printf("%x", color_index[i][j]);
		}
		printf("\n");
	}
}

static uint8_t debug_background_pixel(struct nes_emulator_console *console,
                                      uint8_t x, uint8_t y)
{
	/* Lookup in nametable */
	uint16_t index_address = console->ppu.nametable_address
	                         + (y / 8) * 32 + (x / 8);
	uint8_t index = ppu_bus_read(console, index_address);

	/* Lookup referenced tile */
	uint8_t column_offset = y % 8;
	uint8_t high = ppu_bus_read(console,
		console->ppu.background_address + index * 16 + column_offset);
	uint8_t low = ppu_bus_read(console,
		console->ppu.background_address + index * 16 + 8
		+ column_offset);
	uint8_t pixel_value = 0;
	if ((1 << (7 - (x % 8))) & high) {
		pixel_value |= 0x02;
	}
	if ((1 << (7 - (x % 8))) & low) {
		pixel_value |= 0x01;
	}

	if (pixel_value == 0) {
		return ppu_bus_read(console, 0x3F00);
	}

	/* Lookup attribute */
	uint16_t attribute_address = console->ppu.nametable_address
	                             + 960 + (y / 32) * 8 + (x / 32);
	uint8_t attribute = ppu_bus_read(console, attribute_address);

	/* Lookup palette */
	uint8_t palette_index = 0;
	if ((x / 16) % 2) {
		palette_index |= 0x01;
	}
	if ((y / 16) % 2) {
		palette_index |= 0x02;
	}

	uint16_t palette_offset = 0x3F00 + 4 * palette_index;
	uint16_t palette_address = palette_offset + pixel_value;
	return ppu_bus_read(console, palette_address);
}

/* TODO: Refactor to backend */
struct wayland *wayland_ppu;
void paint_pixel(struct wayland *wayland, uint8_t x, uint8_t y, uint8_t c);
void render_frame(struct wayland *wayland);

uint8_t ppu_step(struct nes_emulator_console *console)
{
	uint16_t cycle = console->ppu.cycle;
	int16_t scan_line = console->ppu.scan_line;
	for (uint8_t i = 0; i < console->cpu_step_cycles * 3; ++i) {
		/* Draw the pixel */
		if (scan_line >= 0 && scan_line < 240) {
			if (cycle >= 1 && scan_line <= 256) {
				paint_pixel(wayland_ppu, cycle - 1, scan_line,
					debug_background_pixel(console,
						cycle - 1, scan_line));
			}
		}

		if (scan_line == 241 && cycle == 1) {
			render_frame(wayland_ppu);
			console->ppu.trigger_vblank = true;
			if (console->ppu.generate_nmi) {
				cpu_generate_nmi(console);
			}
		}
		cycle += 1;
		if (cycle > 340) {
			cycle = 0;
			scan_line += 1;
			if (scan_line > 260) {
				scan_line = -1;
			}
		}
	}
	console->ppu.cycle = cycle;
	console->ppu.scan_line = scan_line;
	return 0;
}
