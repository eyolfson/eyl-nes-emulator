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
	else if (address < 0x4000) {
		uint16_t mirrored_address = (address % 0x20) + 0x3F00;
		return palette_ppu_bus_read(console, mirrored_address);
	}
	else {
		return ppu_bus_read(console, address % 0x4000);
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
	else if (address < 0x4000) {
		uint16_t mirrored_address = (address % 0x20) + 0x3F00;
		palette_ppu_bus_write(console, mirrored_address, value);
	}
	else {
		ppu_bus_write(console, address % 0x4000, value);
	}
}

void ppu_init(struct nes_emulator_console *console)
{
	for (int i = 0; i < PPU_RAM_SIZE; ++i) {
		console->ppu.ram[i] = 0;
	}
	for (int i = 0; i < PPU_PALETTE_SIZE; ++i) {
		console->ppu.palette[i] = 0;
	}
	for (int i = 0; i < PPU_OAM_SIZE; ++i) {
		console->ppu.oam[i] = 0;
	}

	console->ppu.computed_address_is_high = true;
	console->ppu.computed_address_increment = 1;
	console->ppu.computed_address = 0;
	console->ppu.oam_address = 0;
	console->ppu.background_address = 0x1000;
	console->ppu.sprite_address = 0x0000;
	console->ppu.nametable_address = 0x2000;
	console->ppu.nmi_output = false;
	console->ppu.nmi_occurred = false;
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

static uint8_t background_pixel_value(struct nes_emulator_console *console,
                                      uint8_t x,
                                      uint8_t y)
{
	const uint8_t TILE_ROWS = 32;
	uint8_t tile_row = x / 8;
	uint8_t tile_column = y / 8;

	/* Lookup tile index in nametable */
	uint16_t nametable_address = console->ppu.nametable_address;
	uint16_t tile_nametable_address = nametable_address
	                                  + tile_column * TILE_ROWS
	                                  + tile_row;
	uint8_t tile_index = ppu_bus_read(console, tile_nametable_address);

	/* Lookup referenced tile */
	const uint8_t TILE_PIXELS_PER_ROW = 8;
	uint8_t tile_x = x % 8;
	uint8_t tile_y = y % 8;
	uint8_t pixel_index = tile_y * TILE_PIXELS_PER_ROW + tile_x;
	uint8_t pixel_byte_offset = pixel_index / 8;
	uint8_t pixel_bit_position = 7 - pixel_index % 8;

	uint16_t background_address = console->ppu.background_address;
	const uint8_t BYTES_PER_TILE = 16;
	uint16_t low_byte_address = background_address
	                            + tile_index * BYTES_PER_TILE
	                            + pixel_byte_offset;
	const uint8_t HIGH_BYTE_OFFSET = 8;
	uint16_t high_byte_address = background_address
	                             + tile_index * BYTES_PER_TILE
	                             + pixel_byte_offset
	                             + HIGH_BYTE_OFFSET;
	uint8_t low_byte = ppu_bus_read(console, low_byte_address);
	uint8_t high_byte = ppu_bus_read(console, high_byte_address);
	uint8_t pixel_value = 0;
	if (low_byte & (1 << pixel_bit_position)) {
		pixel_value |= 0x01;
	}
	if (high_byte & (1 << pixel_bit_position)) {
		pixel_value |= 0x02;
	}
	return pixel_value;
}

static uint8_t debug_background_pixel(struct nes_emulator_console *console,
                                      uint8_t x,
                                      uint8_t y)
{
	uint8_t pixel_value = background_pixel_value(console, x, y);

	/* Default background color */
	if (pixel_value == 0) {
		return ppu_bus_read(console, 0x3F00);
	}

	uint16_t nametable_address = console->ppu.nametable_address;

	/* Lookup attribute */
	const uint16_t ATTRIBUTE_OFFSET = 960;
	const uint8_t ATTRIBUTE_ROWS = 8;
	uint8_t attribute_row = x / 32;
	uint8_t attribute_column = y / 32;
	uint16_t attribute_address = nametable_address
	                             + ATTRIBUTE_OFFSET
	                             + attribute_column * ATTRIBUTE_ROWS
	                             + attribute_row;
	uint8_t attribute_byte = ppu_bus_read(console, attribute_address);
	uint8_t quadrat_x = (x / 16) % 2;
	uint8_t quadrat_y = (y / 16) % 2;
	uint8_t quadrat_index = quadrat_y * 2 + quadrat_x;
	uint8_t attribute_value = 0;
	switch (quadrat_index) {
	case 0:
		attribute_value = attribute_byte & 0x03;
		break;
	case 1:
		attribute_value = (attribute_byte & 0x0C) >> 2;
		break;
	case 2:
		attribute_value = (attribute_byte & 0x30) >> 4;
		break;
	case 3:
		attribute_value = (attribute_byte & 0xC0) >> 6;
		break;
	}

	uint16_t palette_address = 0x3F00 + 4 * attribute_value + pixel_value;
	return ppu_bus_read(console, palette_address);
}

/* TODO: Refactor to backend */
struct wayland *wayland_ppu;
void paint_pixel(struct wayland *wayland, uint8_t x, uint8_t y, uint8_t c);
void render_frame(struct wayland *wayland);

static bool is_rendering_disabled(struct nes_emulator_console *console)
{
	return (console->ppu.mask & 0x18) == 0x00;
}

static void ppu_single_cycle(struct nes_emulator_console *console,
                             int16_t scan_line,
                             uint16_t cycle)
{
	/* Draw the pixel */
	if (scan_line >= 0 && scan_line < 240) {
		if (cycle >= 1 && cycle <= 256) {
			uint8_t x = cycle - 1;
			uint8_t y = scan_line;
			paint_pixel(wayland_ppu, x, y,
				debug_background_pixel(console, x, y));
		}
	}

	if (scan_line == 241 && cycle == 1) {
		render_frame(wayland_ppu);

		console->ppu.nmi_occurred = true;

		if (console->ppu.nmi_output) {
			cpu_generate_nmi(console);
		}
	}

	/* TODO: probably related to even odd frames */
	if (scan_line == -1 && cycle == 2) {
		console->ppu.nmi_occurred = false;
	}
}

uint8_t ppu_step(struct nes_emulator_console *console)
{
	uint16_t cycle = console->ppu.cycle;
	int16_t scan_line = console->ppu.scan_line;
	for (uint8_t i = 0; i < console->cpu_step_cycles * 3; ++i) {


		ppu_single_cycle(console, scan_line, cycle);

		cycle += 1;
		if (cycle > 340) {
			cycle = 0;
			scan_line += 1;
			if (scan_line > 260) {
				scan_line = -1;
			}
		}

		/* Even odd frames? */
		/*
		if (scan_line == -1 && cycle == 339) {
			scan_line = 0;
			cycle = 0;
		}
		*/
	}
	console->ppu.cycle = cycle;
	console->ppu.scan_line = scan_line;
	return 0;
}
