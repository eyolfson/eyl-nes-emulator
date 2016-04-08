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

#define RAM_SIZE 0x800 /* 2 KiB */
static uint8_t ram[RAM_SIZE];

#define OAM_SIZE 0x100 /* 256 B */
static uint8_t object_attribute_memory[OAM_SIZE];

static bool computed_address_is_high = true;
static uint16_t computed_address = 0;
static uint8_t computed_address_increment = 1;

static uint8_t bus_read(uint16_t address)
{
	if (address < 0x2000) {
		return *chr_rom_data;
	}
	else if (address < 0x2800) {
		return ram[address - 0x2000];
	}
	else if (address < 0x3000) {
		return ram[address - 0x2800];
	}
	else if (address < 0x3F00) {
		return bus_read(address - 0x1000);
	}
	else if (address < 0x4000) {
		/* TODO: Internal palette control */
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
	else if (address < 0x4000) {
		/* TODO: Internal palette control */
		return;
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

uint8_t handle_status_read()
{
	return 0x80;
}

uint8_t handle_oam_data_read()
{
	return 0;
}

uint8_t handle_data_read()
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
