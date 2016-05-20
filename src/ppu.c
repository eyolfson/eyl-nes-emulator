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

#include "cartridge.h"
#include "console.h"

void nes_emulator_console_add_ppu_backend(
	struct nes_emulator_console *console,
	struct nes_emulator_ppu_backend *ppu_backend)
{
	for (size_t i = 0; i < PPU_BACKENDS_MAX; ++i) {
		if (console->ppu.backends[i] == NULL) {
			console->ppu.backends[i] = ppu_backend;
			break;
		}
	}
}
static void render_pixel(struct nes_emulator_console *console,
                         uint8_t x,
                         uint8_t y,
                         uint8_t c)
{
	for (size_t i = 0; i < PPU_BACKENDS_MAX; ++i) {
		struct nes_emulator_ppu_backend *backend;
		backend = console->ppu.backends[i];
		if (backend != NULL) {
			backend->render_pixel(backend->pointer, x, y, c);
		}
	}
}

static void vertical_blank(struct nes_emulator_console *console)
{
	for (size_t i = 0; i < PPU_BACKENDS_MAX; ++i) {
		struct nes_emulator_ppu_backend *backend;
		backend = console->ppu.backends[i];
		if (backend != NULL) {
			backend->vertical_blank(backend->pointer);
		}
	}
}

static uint16_t get_tile_address(struct nes_emulator_console *console)
{
	uint16_t v = console->ppu.internal_registers.v;
	return 0x2000 | (v & 0x0FFF);
}

static uint16_t get_attribute_address(struct nes_emulator_console *console)
{
	uint16_t v = console->ppu.internal_registers.v;
	return 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
}

static void coarse_x_increment(struct nes_emulator_console *console)
{
	uint16_t v = console->ppu.internal_registers.v;
	if ((v & 0x001F) == 0x001F) {
		v &= ~0x001F;
		v ^= 0x0400;
	}
	else {
		v += 1;
	}
	console->ppu.internal_registers.v = v;
}

static void fine_x_increment(struct nes_emulator_console *console)
{
	uint8_t x = console->ppu.internal_registers.x;
	if (x == 7) {
		coarse_x_increment(console);
		x = 0;
	}
	else {
		x += 1;
	}
	console->ppu.internal_registers.x = x;
}

static void coarse_y_increment(struct nes_emulator_console *console)
{
	uint16_t v = console->ppu.internal_registers.v;
	uint8_t coarse_y = (v & 0x03E0) >> 5;
	if (coarse_y == 29) {
		coarse_y = 0;
		v ^= 0x0800;
	}
	else if (coarse_y == 31) {
		coarse_y = 0;
	}
	else {
		coarse_y += 1;
	}
	v = (v & ~0x03E0) | (coarse_y << 5);
	console->ppu.internal_registers.v = v;
}

static void fine_y_increment(struct nes_emulator_console *console)
{
	uint16_t v = console->ppu.internal_registers.v;
	if ((v & 0x7000) == 0x7000) {
		v &= ~0x7000;
		console->ppu.internal_registers.v = v;
		coarse_y_increment(console);
	}
	else {
		v += 0x1000;
		console->ppu.internal_registers.v = v;
	}
}

static bool is_rendering_disabled(struct nes_emulator_console *console)
{
	return (console->ppu.mask & 0x18) == 0x00;
}

static bool is_show_background(struct nes_emulator_console *console)
{
	return (console->ppu.mask & 0x08) == 0x08;
}

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
		switch (console->cartridge->mirroring) {
		case 0:
			if (address < 0x2C00) {
				return console->ppu.ram[address - 0x2000];
			}
			else {
				return console->ppu.ram[address - 0x2C00];
			}
		case 1:
			return console->ppu.ram[address - 0x2800];
		default:
			return 0;
		}
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
	else if (address < 0x8000) {
		return ppu_bus_read(console, address - 0x4000);
	}
	else if (address < 0xC000) {
		return ppu_bus_read(console, address - 0x8000);
	}
	else {
		return ppu_bus_read(console, address - 0xC000);
	}
}

void ppu_bus_write(struct nes_emulator_console *console,
                   uint16_t address,
                   uint8_t value)
{
	int16_t scan_line = console->ppu.scan_line;
	if (scan_line >= 0 && scan_line < 240) {
		if (!is_rendering_disabled(console)) {
			return;
		}
	}

	if (address < 0x2000) {
		cartridge_ppu_bus_write(console, address, value);
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
	else if (address < 0x8000) {
		ppu_bus_write(console, address - 0x4000, value);
	}
	else if (address < 0xC000) {
		ppu_bus_write(console, address - 0x8000, value);
	}
	else {
		ppu_bus_write(console, address - 0xC000, value);
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
	for (int i = 0; i < PPU_SECONDARY_OAM_SIZE; ++i) {
		console->ppu.secondary_oam[i] = 0;
	}

	console->ppu.computed_address_increment = 1;
	console->ppu.oam_address = 0;
	console->ppu.background_address = 0x1000;
	console->ppu.sprite_address = 0x0000;
	console->ppu.nametable_address = 0x2000;
	console->ppu.nmi_output = false;
	console->ppu.nmi_occurred = false;
	console->ppu.is_sprite_0_hit_frame = false;
	console->ppu.is_sprite_0_hit = false;
	console->ppu.is_sprite_overflow = false;
	console->ppu.cycle = 0;
	console->ppu.scan_line = 241;
	for (size_t i = 0; i < PPU_BACKENDS_MAX; ++i) {
		console->ppu.backends[i] = NULL;
	}
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
                                      uint16_t nametable_address,
                                      uint8_t x,
                                      uint8_t y);

static void populate_secondary_oam(struct nes_emulator_console *console,
                                   uint8_t y)
{

}


static void oam_render(struct nes_emulator_console *console,
                       uint8_t x,
                       uint8_t y,
                       uint8_t found)
{
	for (uint8_t i = 0; i < found; ++i) {
		uint8_t offset = i * 4;
		uint8_t y_top = console->ppu.secondary_oam[offset];
		uint8_t y_offset = y - y_top;

		uint8_t x_left = console->ppu.secondary_oam[offset + 3];
		if (x_left >= 0xF8) {
			continue;
		}
		if (!(x >= x_left && x <= (x_left + 7))) {
			continue;
		}
		uint8_t x_offset = x - x_left;

		/* This tile is within range */
		uint8_t tile_index = console->ppu.secondary_oam[offset + 1];
		uint16_t sprite_address = console->ppu.sprite_address;
		uint8_t pixel_index = y_offset * 8 + x_offset;
		uint8_t pixel_byte_offset = pixel_index / 8;
		uint8_t pixel_bit_position = 7 - pixel_index % 8;

		/* TODO: Refactor */
		const uint8_t BYTES_PER_TILE = 16;
		uint16_t low_byte_address = sprite_address
		                            + tile_index * BYTES_PER_TILE
		                            + pixel_byte_offset;
		const uint8_t high_byte_offset = 8;
		uint16_t high_byte_address = sprite_address
		                             + tile_index * BYTES_PER_TILE
		                             + pixel_byte_offset
		                             + high_byte_offset;
		uint8_t low_byte = ppu_bus_read(console, low_byte_address);
		uint8_t high_byte = ppu_bus_read(console, high_byte_address);
		uint8_t pixel_value = 0;
		if (low_byte & (1 << pixel_bit_position)) {
			pixel_value |= 0x01;
		}
		if (high_byte & (1 << pixel_bit_position)) {
			pixel_value |= 0x02;
		}
		if (pixel_value == 0) {
			continue;
		}

		uint16_t nametable_address = console->ppu.nametable_address;
		if (i == 0
		    && background_pixel_value(console, nametable_address, x, y)
		       != 0) {
			if (!(console->ppu.is_sprite_0_hit_frame)) {
				if ((console->ppu.mask & 0x18) == 0x18) {
					console->ppu.is_sprite_0_hit = true;
					console->ppu.is_sprite_0_hit_frame = true;
				}
			}
		}

		/* Lookup palette */
		uint8_t palette_index = console->ppu.secondary_oam[offset + 2] & 0x03;
		uint16_t palette_address = 0x3F10 + 4 * palette_index + pixel_value;
		uint8_t pixel_color = ppu_bus_read(console, palette_address);
		render_pixel(console, x, y , pixel_color);
		break;
	}
}

static void oam_probe(struct nes_emulator_console *console,
                      uint8_t y)
{
	uint8_t found = 0;
	console->ppu.is_sprite_0_in_secondary = false;
	for (uint8_t i = 0; i < 64; ++i) {
		uint8_t offset = i * 4;
		uint8_t y_top = console->ppu.oam[offset];
		/* Check would overflow */
		if (y_top >= 0xF8) {
			continue;
		}
		if (y >= y_top && y <= (y_top + 7)) {
			/* Copy bytes to secondary OAM */
			if (found < 8) {
				for (uint8_t j = 0; j < 4; ++j) {
					console->ppu.secondary_oam[found * 4 + j] =
						console->ppu.oam[offset + j];
				}
			}
			if (i == 0) {
				console->ppu.is_sprite_0_in_secondary = true;
			}
			++found;
		}
	}
	if (found > 8) {
		console->ppu.is_sprite_overflow = true;
	}
	for (uint8_t x = 0; ; ++x) {
		oam_render(console, x, y, found < 8 ? found : 8);
		if (x == 0xFF) {
			break;
		}
	}
}

static void debug_oam(struct nes_emulator_console *console)
{
	for (uint8_t y = 0; y < 240; ++y) {
		oam_probe(console, y);
	}
}

static uint8_t bg_pixel_value(struct nes_emulator_console *console)
{
  uint16_t tile_address = get_tile_address(console);
	uint8_t fine_x = console->ppu.internal_registers.x;
	uint16_t v = console->ppu.internal_registers.v;
	uint8_t fine_y = (v & 0x7000) >> 12;
	uint8_t tile_index = ppu_bus_read(console, tile_address);

	const uint8_t TILE_PIXELS_PER_ROW = 8;
	uint8_t pixel_index = fine_y * TILE_PIXELS_PER_ROW + fine_x;
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

static uint8_t bg_pixel(struct nes_emulator_console *console)
{
	if (!is_show_background(console)) {
		return 0x00;
	}
	uint8_t pixel_value = bg_pixel_value(console);

	/* Default background color */
	if (pixel_value == 0) {
		return ppu_bus_read(console, 0x3F00);
	}

	/* Lookup attribute */
	uint16_t attribute_address = get_attribute_address(console);
	uint8_t attribute_byte = ppu_bus_read(console, attribute_address);

	uint16_t v = console->ppu.internal_registers.v;
	uint8_t shift = (v & 0x0040) >> 4 | (v & 0x0002);
	uint8_t mask = 0x03 << shift;
	uint8_t attribute_value = (attribute_byte & mask) >> shift;

	const uint8_t ENTRY_SIZE = 4;
	uint8_t palette_index = ENTRY_SIZE * attribute_value + pixel_value;
	return console->ppu.palette[palette_index];
}

static uint8_t background_pixel_value(struct nes_emulator_console *console,
                                      uint16_t nametable_address,
                                      uint8_t x,
                                      uint8_t y)
{
	const uint8_t TILE_ROWS = 32;
	uint8_t tile_row = x / 8;
	uint8_t tile_column = y / 8;

	/* Lookup tile index in nametable */
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
	const uint8_t high_byte_offset = 8;
	uint16_t high_byte_address = background_address
	                             + tile_index * BYTES_PER_TILE
	                             + pixel_byte_offset
	                             + high_byte_offset;
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

static uint8_t background_pixel(struct nes_emulator_console *console,
                                uint16_t nametable_address,
                                uint8_t x,
                                uint8_t y)
{
	uint8_t pixel_value = background_pixel_value(console, nametable_address,
	                                             x, y);

	/* Default background color */
	if (pixel_value == 0) {
		return ppu_bus_read(console, 0x3F00);
	}

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

static uint8_t debug_background_pixel(struct nes_emulator_console *console,
                                      uint8_t x,
                                      uint8_t y)
{
	if (!is_show_background(console)) {
		return 0x00;
	}
	uint8_t x_scroll = 0;
	uint16_t x_offset = x + x_scroll;
	uint16_t nametable_address = console->ppu.nametable_address;
	uint8_t y_scroll = 0;
	uint16_t y_offset = y + y_scroll;
	if (x_offset >= 256) {
		x_offset -= 256;
		nametable_address ^= 0x0400;
	}
	if (y_offset >= 256) {
		y_offset -= 240;
		nametable_address ^= 0x0800;
	}
	return background_pixel(console, nametable_address, x_offset, y_offset);
}

static void ppu_vertical_blank_start(struct nes_emulator_console *console)
{
	debug_oam(console);

	vertical_blank(console);

	console->ppu.is_sprite_0_hit_frame = false;
	console->ppu.is_sprite_overflow = false;

	console->ppu.nmi_occurred = true;

	if (is_show_background(console)) {
		uint16_t t = console->ppu.internal_registers.t;
		console->ppu.internal_registers.v = t;
	}

	if (console->ppu.nmi_output) {
		cpu_generate_nmi(console);
	}
}

static void ppu_vertical_blank_end(struct nes_emulator_console *console)
{
	console->ppu.nmi_occurred = false;
	console->ppu.is_sprite_0_hit_frame = false;
	console->ppu.is_sprite_overflow = false;
}

static void reset_horizontal(struct nes_emulator_console *console)
{
	const uint16_t MASK = 0x041F;
	uint16_t t = console->ppu.internal_registers.t;
	uint16_t v = console->ppu.internal_registers.v;
	v = (v & ~MASK) | (t & MASK);
	console->ppu.internal_registers.v = v;
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

			if (!is_show_background(console)) {
				render_pixel(console, x, y, 0x00);
				return;
			}

			uint8_t bg_pv = bg_pixel(console);
			render_pixel(console, x, y, bg_pv);

			fine_x_increment(console);
			if (cycle == 256) {
				fine_y_increment(console);
			}
		}
		else if (cycle == 257) {
			if (is_show_background(console)) {
				reset_horizontal(console);
			}
		}
	}

	if (scan_line == 241 && cycle == 1) {
		ppu_vertical_blank_start(console);
	}

	/* TODO: probably related to even odd frames */
	if (scan_line == -1 && cycle == 2) {
		ppu_vertical_blank_end(console);
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
