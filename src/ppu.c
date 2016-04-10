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

#include <stdbool.h>

/* TODO: Assume horizontal arrangement / vertical mirroring (64x30 tilemap) */

static uint8_t *chr_rom_data;

static struct wayland *wayland;

#define RAM_SIZE 0x800 /* 2 KiB */
static uint8_t ram[RAM_SIZE];

#define PALETTE_RAM_SIZE 0x20 /* 32 B */
static uint8_t palette_ram[PALETTE_RAM_SIZE];

#define OAM_SIZE 0x100 /* 256 B */
static uint8_t object_attribute_memory[OAM_SIZE];

static bool computed_address_is_high = true;
static uint16_t computed_address = 0;
static uint8_t computed_address_increment = 1;

static uint16_t background_address = 0x1000;
static uint16_t nametable_address = 0x2000;

/* Each nametable is 1024 bytes (0x400) */
/* It consists of 960 8x8 tiles to form the background */
/* Each of these tiles are a byte */
/* 32 horizontal, 30 vertical tiles */
/* There are 64 bytes remaining */
/* Screen is divided into 64 32x32 tiles called attribute tiles */

static uint8_t bus_read(uint16_t address)
{
	if (address < 0x2000) {
		return chr_rom_data[address];
	}
	else if (address < 0x2800) {
		return ram[address - 0x2000];
	}
	else if (address < 0x3000) {
		/* Mirroring */
		return ram[address - 0x2800];
	}
	else if (address < 0x3F00) {
		return bus_read(address - 0x1000);
	}
	else if (address < 0x3F20) {
		/* TODO: Internal palette control */
		/* TODO: Mirrors */
		return palette_ram[address - 0x3F00];
	}
	else if (address < 0x4000) {
		/* TODO: Mirrors */
		return 0;
	}
	else {
		/* TODO: Out-of-range */
		return 0;
	}
}

static void bus_write(uint16_t address, uint8_t value)
{
	if (address < 0x2000) {
		return;
	}
	else if (address < 0x2800) {
		ram[address - 0x2000] = value;
	}
	else if (address < 0x3000) {
		ram[address - 0x2800] = value;
	}
	else if (address < 0x3F00) {
		bus_write(address - 0x1000, value);
	}
	else if (address < 0x3F20) {
		/* TODO: Internal palette control */
		/* TODO: Mirrors */
		palette_ram[address - 0x3F00] = value;
	}
	else if (address < 0x4000) {
		/* TODO: Mirrors */
	}
	else {
		/* TODO: Out-of-range */
		return;
	}
}

void set_chr_rom(uint8_t *data)
{
	chr_rom_data = data;
}

void set_wayland(struct wayland *w)
{
	wayland = w;
}

static void paint_pixel(uint8_t x, uint8_t y, uint8_t c)
{
	static uint32_t palette[64] = {
		[0x00] = 0xFF545454,
		[0x01] = 0xFF001E74,
		[0x02] = 0xFF081090,
		[0x03] = 0xFF300088,
		[0x04] = 0xFF440064,
		[0x05] = 0xFF5C0030,
		[0x06] = 0xFF540400,
		[0x07] = 0xFF3C1800,
		[0x08] = 0xFF202A00,
		[0x09] = 0xFF083A00,
		[0x0A] = 0xFF004000,
		[0x0B] = 0xFF003C00,
		[0x0C] = 0xFF00323C,
		[0x0D] = 0xFF000000,
		[0x0E] = 0xFF000000,
		[0x0F] = 0xFF000000,
		[0x10] = 0xFF989698,
		[0x11] = 0xFF084CC4,
		[0x12] = 0xFF3032EC,
		[0x13] = 0xFF5C1EE4,
		[0x14] = 0xFF8814B0,
		[0x15] = 0xFFA01464,
		[0x16] = 0xFF982220,
		[0x17] = 0xFF783C00,
		[0x18] = 0xFF545A00,
		[0x19] = 0xFF287200,
		[0x1A] = 0xFF087C00,
		[0x1B] = 0xFF007628,
		[0x1C] = 0xFF006678,
		[0x1D] = 0xFF000000,
		[0x1E] = 0xFF000000,
		[0x1F] = 0xFF000000,
		[0x20] = 0xFFECEEEC,
		[0x21] = 0xFF4C9AEC,
		[0x22] = 0xFF787CEC,
		[0x23] = 0xFFB062EC,
		[0x24] = 0xFFE454EC,
		[0x25] = 0xFFEC58B4,
		[0x26] = 0xFFEC6A64,
		[0x27] = 0xFFD48820,
		[0x28] = 0xFFA0AA00,
		[0x29] = 0xFF74C400,
		[0x2A] = 0xFF4CD020,
		[0x2B] = 0xFF38CC6C,
		[0x2C] = 0xFF38B4CC,
		[0x2D] = 0xFF3C3C3C,
		[0x2E] = 0xFF000000,
		[0x2F] = 0xFF000000,
		[0x30] = 0xFFECEEEC,
		[0x31] = 0xFFA8CCEC,
		[0x32] = 0xFFBCBCEC,
		[0x33] = 0xFFD4B2EC,
		[0x34] = 0xFFECAEEC,
		[0x35] = 0xFFECAED4,
		[0x36] = 0xFFECB4B0,
		[0x37] = 0xFFE4C490,
		[0x38] = 0xFFCCD278,
		[0x39] = 0xFFB4DE78,
		[0x3A] = 0xFFA8E290,
		[0x3B] = 0xFF98E2B4,
		[0x3C] = 0xFFA0D6E4,
		[0x3D] = 0xFFA0A2A0,
		[0x3E] = 0xFF000000,
		[0x3F] = 0xFF000000,
	};
	wayland->back_data[x + (y * wayland->width)] = palette[c];
}

#include <stdio.h>

static void debug_tile(uint16_t address)
{
	uint8_t color_index[8][8] = { 0 };
	uint8_t x = 0;
	for (uint16_t i = address; i < address + 8; ++i) {
		uint8_t b = bus_read(i);
		for (uint8_t j = 0; j < 8; ++j) {
			if (b & (1 << j)) {
				color_index[x][7 - j] |= 0x02;
			}
		}
		++x;
	}
	x = 0;
	for (uint16_t i = address + 8; i < address + 16; ++i) {
		uint8_t b = bus_read(i);
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

static void debug_background_pixel(uint8_t x, uint8_t y)
{
	/* Lookup in nametable */
	uint16_t index_address = nametable_address + (y / 8) * 32 + (x / 8);
	uint8_t index = bus_read(index_address);

	/* Lookup referenced tile */
	uint8_t column_offset = y % 8;
	uint8_t high = bus_read(background_address + index * 16
	                        + column_offset);
	uint8_t low = bus_read(background_address + index * 16 + 8
	                       + column_offset);
	uint8_t pixel_value = 0;
	if ((1 << (7 - (x % 8))) & high) {
		pixel_value |= 0x02;
	}
	if ((1 << (7 - (x % 8))) & low) {
		pixel_value |= 0x01;
	}

	/* Lookup attribute */
	uint16_t attribute_address = nametable_address + 960 + (y / 32) * 8
	                             + (x / 32);
	uint8_t attribute = bus_read(attribute_address);

	/* Lookup palette */
}

static uint8_t handle_status_read()
{
	for (int32_t x = 0; x < wayland->width; ++x) {
		for (int32_t y = 0; y < wayland->height; ++y) {
			paint_pixel(x, y, 0);
		}
	}
	wl_display_dispatch(wayland->display);
	return 0x80;
}

static uint8_t handle_oam_data_read()
{
	return 0;
}

static uint8_t handle_data_read()
{
	uint8_t value = bus_read(computed_address);
	computed_address += computed_address_increment;
	return value;
}

uint8_t ppu_read(uint8_t address)
{
	switch (address) {
	case 0:
		return 0;
	case 1:
		return 0;
	case 2:
		return handle_status_read();
	case 3:
		return 0;
	case 4:
		return handle_oam_data_read();
	case 5:
		return 0;
	case 6:
		return 0;
	case 7:
		return handle_data_read();
	}
	return 0;
}

static void handle_ctrl_write(uint8_t value)
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
		computed_address_increment = 1;
		break;
	case 1:
		computed_address_increment = 32;
		break;
	}

	/* Base nametable address
	   (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00) */
	uint8_t n = (value & 0x03);
	switch (n) {
	case 0:
		nametable_address = 0x2000;
		break;
	case 1:
		nametable_address = 0x2400;
		break;
	case 2:
		nametable_address = 0x2800;
		break;
	case 3:
		nametable_address = 0x2C00;
		break;
	}
}

static void handle_mask_write(uint8_t value)
{

}

static void handle_oam_address_write(uint8_t value)
{

}

static void handle_oam_data_write(uint8_t value)
{

}

static void handle_scroll_write(uint8_t value)
{

}

static void handle_address_write(uint8_t value)
{
	if (computed_address_is_high) {
		computed_address &= 0x00FF;
		computed_address |= (value << 8);
		computed_address_is_high = false;
	}
	else {
		computed_address &= 0xFF00;
		computed_address |= value;
		computed_address_is_high = true;
	}
}

static void handle_data_write(uint8_t value)
{
	bus_write(computed_address, value);
	computed_address += computed_address_increment;
}

void ppu_write(uint8_t address, uint8_t value)
{
	switch (address) {
	case 0:
		handle_ctrl_write(value);
		break;
	case 1:
		handle_mask_write(value);
		break;
	case 2:
		break;
	case 3:
		handle_oam_address_write(value);
		break;
	case 4:
		handle_oam_data_write(value);
		break;
	case 5:
		handle_scroll_write(value);
		break;
	case 6:
		handle_address_write(value);
		break;
	case 7:
		handle_data_write(value);
		break;
	}
}
