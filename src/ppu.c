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

/* TODO: Assume horizontal arrangement / vertical mirroring (64x30 tilemap) */

static uint8_t *chr_rom_data;

static uint8_t ram[PPU_RAM_SIZE];

static uint8_t palette_ram[PPU_PALETTE_SIZE];

static uint8_t object_attribute_memory[PPU_OAM_SIZE];

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

static uint8_t debug_background_pixel(uint8_t x, uint8_t y)
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

	if (pixel_value == 0) {
		return bus_read(0x3F00);
	}

	/* Lookup attribute */
	uint16_t attribute_address = nametable_address + 960 + (y / 32) * 8
	                             + (x / 32);
	uint8_t attribute = bus_read(attribute_address);

	/* Lookup palette */
	uint8_t palette_index = 0;
	if ((x / 16) % 2) {
		palette_index |= 0x01;
	}
	if ((y / 16) % 2) {
		palette_index |= 0x02;
	}

	uint16_t palette_offset = 0x3F00 + 4 * palette_index;
	return bus_read(palette_offset + pixel_value);
}

static uint8_t handle_status_read()
{
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

static uint8_t ppu_register_status_read(struct nes_emulator_console *console)
{
	/* Vertical blank has started */
	/* Sprite 0 Hit */
	/* Sprite overflow */
}

uint8_t ppu_cpu_bus_read(struct nes_emulator_console *console,
                         uint16_t address)
{
	switch (address % 8) {
	case 0:
		return ppu_register_status_read(console);
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
	default:
		break;
	}
}

void ppu_init(struct nes_emulator_console *console)
{
	console->ppu.computed_address_is_high = true;
	console->ppu.computed_address_increment = 1;
	console->ppu.computed_address = 0;
	console->ppu.background_address = 0x1000;
	console->ppu.nametable_address = 0x2000;
	console->ppu.cycle = 0;
	console->ppu.scan_line = 241;
}

uint8_t ppu_step(struct nes_emulator_console *console)
{
	uint16_t cycle = console->ppu.cycle;
	int16_t scan_line = console->ppu.scan_line;
	for (uint8_t i = 0; i < console->cpu_step_cycles * 3; ++i) {

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
