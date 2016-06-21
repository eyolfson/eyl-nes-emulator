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

#include <assert.h>

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
	uint16_t nametable_bits = (console->ppu.control & 0x03) << 10;
	return 0x2000 | nametable_bits | (v & 0x03FF);
}

static uint16_t get_attribute_address(struct nes_emulator_console *console)
{
	uint16_t v = console->ppu.internal_registers.v;
	uint16_t nametable_bits = (console->ppu.control & 0x03) << 10;
	return 0x23C0 | nametable_bits | (v & 0x0C00)
	       | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
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

static bool control_sprite_size_8_x_16(struct nes_emulator_console *console)
{
	return (console->ppu.control & 0x20) == 0x20;
}

static bool is_rendering_disabled(struct nes_emulator_console *console)
{
	return (console->ppu.mask & 0x18) == 0x00;
}

static bool mask_show_leftmost_background(struct nes_emulator_console *console)
{
	return (console->ppu.mask & 0x02) == 0x02;
}

static bool mask_show_leftmost_sprites(struct nes_emulator_console *console)
{
	return (console->ppu.mask & 0x04) == 0x04;
}

static bool mask_show_background(struct nes_emulator_console *console)
{
	return (console->ppu.mask & 0x08) == 0x08;
}

static bool mask_show_sprites(struct nes_emulator_console *console)
{
	return (console->ppu.mask & 0x10) == 0x10;
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

static uint16_t get_mirror_index(struct nes_emulator_console *console,
                                 uint16_t address)
{
	assert(address >=0x2800 && address < 0x3000);
	uint16_t index;
	switch (console->cartridge->mirroring) {
	case 0:
		if (address < 0x2C00) {
			index = address - 0x2000;
		}
		else {
			index = address - 0x2C00;
		}
		break;
	case 1:
		index = address - 0x2800;
		break;
	default:
		return 0;
	}
	return index;
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
		uint16_t index = get_mirror_index(console, address);
		return console->ppu.ram[index];
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
		uint16_t index = get_mirror_index(console, address);
		console->ppu.ram[index] = value;
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
	console->ppu.secondary_oam_entries = 0;

	console->ppu.computed_address_increment = 1;
	console->ppu.oam_address = 0;
	console->ppu.background_address = 0x1000;
	console->ppu.sprite_address = 0x0000;
	console->ppu.nametable_address = 0x2000;
	console->ppu.nmi_output = false;
	console->ppu.nmi_occurred = false;
	console->ppu.is_sprite_overflow = false;
	console->ppu.control = 0;
	console->ppu.status = 0;
	console->ppu.read_buffer = 0;
	console->ppu.mask = 0;
	console->ppu.cycle = 0;
	console->ppu.scan_line = 241;
	for (size_t i = 0; i < PPU_BACKENDS_MAX; ++i) {
		console->ppu.backends[i] = NULL;
	}
	console->ppu.internal_registers.t = 0;
	console->ppu.internal_registers.v = 0;
	console->ppu.internal_registers.w = 0;
	console->ppu.internal_registers.x = 0;
}

static void populate_secondary_oam(struct nes_emulator_console *console,
                                   uint8_t y)
{
	uint8_t entries = 0;
	console->ppu.is_sprite_0_in_secondary = false;
	for (uint8_t i = 0; i < 64; ++i) {
		uint8_t offset = i * 4;
		uint8_t y_top = console->ppu.oam[offset];
		/* Check would overflow */
		if (y_top >= 0xF8) {
			continue;
		}
		uint8_t sprite_height = 8;
		if (control_sprite_size_8_x_16(console)) {
			sprite_height = 16;
		}
		if (y >= y_top && y <= (y_top + (sprite_height - 1))) {
			/* Copy bytes to Secondary OAM */
			if (entries < 8) {
				for (uint8_t j = 0; j < 4; ++j) {
					console->ppu.secondary_oam[entries * 4 + j] =
						console->ppu.oam[offset + j];
				}
			}
			if (i == 0) {
				console->ppu.is_sprite_0_in_secondary = true;
			}
			++entries;
		}
	}

	if (entries > 8) {
		console->ppu.is_sprite_overflow = true;
		console->ppu.secondary_oam_entries = 8;
	}
	else {
		console->ppu.secondary_oam_entries = entries;
	}
}

static void sprite_pixel(struct nes_emulator_console *console,
                         uint8_t x,
                         uint8_t y,
                         uint8_t *pixel_value,
                         uint8_t *pixel_colour)
{
	*pixel_value = 0;
	if (y == 0) {
		return;
	}
	y -= 1;

	for (uint8_t i = 0; i < console->ppu.secondary_oam_entries; ++i) {
		uint8_t offset = i * 4;
		uint8_t y_top = console->ppu.secondary_oam[offset];
		uint8_t y_offset = y - y_top;

		uint8_t attribute = console->ppu.secondary_oam[offset + 2];
		bool flip_vertical = attribute & 0x80;
		bool flip_horizontal = attribute & 0x40;

		uint8_t x_left = console->ppu.secondary_oam[offset + 3];
		if (!(x >= x_left && x <= (x_left + 7))) {
			continue;
		}
		uint8_t x_offset = x - x_left;

		/* This tile is within range */
		uint8_t tile_index = console->ppu.secondary_oam[offset + 1];
		uint16_t sprite_address = console->ppu.sprite_address;

		if (control_sprite_size_8_x_16(console)) {
			sprite_address = 0x0000;
		}
		if (flip_vertical) {
			y_offset = 7 - y_offset;
		}
		if (flip_horizontal) {
			x_offset = 7 - x_offset;
		}
		uint8_t pixel_index = pixel_index = y_offset * 8 + x_offset;
		uint8_t pixel_byte_offset = pixel_index / 8;
		uint8_t pixel_bit_position = 7 - pixel_index % 8;

		/* TODO: Refactor */
		const uint8_t BYTES_PER_TILE = 16;
		uint16_t low_byte_address = sprite_address
		                            + tile_index * BYTES_PER_TILE
		                            + pixel_byte_offset;
		const uint8_t HIGH_BYTE_OFFSET = 8;
		uint16_t high_byte_address = sprite_address
		                             + tile_index * BYTES_PER_TILE
		                             + pixel_byte_offset
		                             + HIGH_BYTE_OFFSET;
		uint8_t low_byte = ppu_bus_read(console, low_byte_address);
		uint8_t high_byte = ppu_bus_read(console, high_byte_address);
		*pixel_value = 0;
		if (low_byte & (1 << pixel_bit_position)) {
			*pixel_value |= 0x01;
		}
		if (high_byte & (1 << pixel_bit_position)) {
			*pixel_value |= 0x02;
		}
		if (*pixel_value == 0) {
			continue;
		}

		/* Lookup palette */
		uint8_t palette_index = attribute & 0x03;
		uint16_t palette_address = 0x3F10 + 4 * palette_index + *pixel_value;
		*pixel_colour = ppu_bus_read(console, palette_address);
		return;
	}
}

static uint8_t background_pixel_value(
	struct nes_emulator_console *console)
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
	uint16_t high_byte_address = low_byte_address
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

static uint8_t background_pixel_colour(struct nes_emulator_console *console,
                                       uint8_t pixel_value)
{
	/* Default background colour */
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

static void ppu_vertical_blank_start(struct nes_emulator_console *console)
{
	vertical_blank(console);

	console->ppu.nmi_occurred = true;

	if (mask_show_background(console)) {
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
}

static void reset_horizontal(struct nes_emulator_console *console)
{
	const uint16_t MASK = 0x041F;
	uint16_t t = console->ppu.internal_registers.t;
	uint16_t v = console->ppu.internal_registers.v;
	v = (v & ~MASK) | (t & MASK);
	console->ppu.internal_registers.v = v;
}

static void copy_vertical(struct nes_emulator_console *console)
{
	const uint16_t MASK = 0x7BE0;
	uint16_t t = console->ppu.internal_registers.t;
	uint16_t v = console->ppu.internal_registers.v;
	v = (v & ~MASK) | (t & MASK);
	console->ppu.internal_registers.v = v;
}

static bool handle_pixel(struct nes_emulator_console *console,
                         uint8_t x,
                         uint8_t y)
{
	uint8_t bg_pixel_value = 0;
	uint8_t bg_pixel_colour = ppu_bus_read(console, 0x3F00);

	uint8_t sprite_pixel_value = 0;
	uint8_t sprite_pixel_colour;

	if (x < 8) {
		if (mask_show_leftmost_background(console)) {
			bg_pixel_value = background_pixel_value(console);
			bg_pixel_colour = background_pixel_colour(
				console, bg_pixel_value);
		}
		if (mask_show_leftmost_sprites(console)) {
			sprite_pixel(console, x, y,
			             &sprite_pixel_value, &sprite_pixel_colour);
		}
	}
	else {
		if (mask_show_background(console)) {
			bg_pixel_value = background_pixel_value(console);
			bg_pixel_colour = background_pixel_colour(
				console, bg_pixel_value);
		}
		if (mask_show_sprites(console)) {
			sprite_pixel(console, x, y,
			             &sprite_pixel_value, &sprite_pixel_colour);
		}
	}

	if (sprite_pixel_value != 0) {
		if (bg_pixel_value != 0 && x != 255) {
			console->ppu.status = 0x40;
		}
		render_pixel(console, x, y, sprite_pixel_colour);
	}
	else {
		render_pixel(console, x, y, bg_pixel_colour);
	}

	return true;
}

static void ppu_single_cycle(struct nes_emulator_console *console,
                             int16_t scan_line,
                             uint16_t cycle)
{
	/* Draw the pixel */
	if (scan_line >= 0 && scan_line < 240) {
		uint8_t y = scan_line;
		if (y > 0 && cycle == 0) {
			/* TODO: might need +1? */
			if (is_rendering_disabled(console)) {
				console->ppu.secondary_oam_entries = 0;
			}
			else {
				populate_secondary_oam(console, y - 1);
			}
		}
		else if (cycle >= 1 && cycle <= 256) {
			uint8_t x = cycle - 1;

			if (!handle_pixel(console, x, y)) {
				return;
			}

			if (!mask_show_background(console)) {
				return;
			}

			fine_x_increment(console);
			if (cycle == 256) {
				fine_y_increment(console);
			}
		}
		else if (cycle == 257) {
			if (mask_show_background(console)) {
				reset_horizontal(console);
			}
		}
	}

	if (scan_line == 240 && cycle == 0) {
		populate_secondary_oam(console, scan_line - 1);
	}

	if (scan_line == 241 && cycle == 1) {
		ppu_vertical_blank_start(console);
	}

	/* TODO: probably related to even odd frames */
	if (scan_line == -1 && cycle == 2) {
		ppu_vertical_blank_end(console);
	}
	if (scan_line == -1) {
		if (cycle == 0) {
			/* TODO: should be in vertical blank end, but the
			         timing is wrong */
			console->ppu.status = 0;
			console->ppu.is_sprite_overflow = false;
		}
		if (mask_show_background(console)) {
			if (cycle == 257) {
				reset_horizontal(console);
			}
			else if (cycle == 280) {
				copy_vertical(console);
			}
		}
	}
}

uint8_t ppu_step(struct nes_emulator_console *console)
{
	uint16_t cycle = console->ppu.cycle;
	int16_t scan_line = console->ppu.scan_line;
	for (uint16_t i = 0; i < console->cpu_step_cycles * 3; ++i) {

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
