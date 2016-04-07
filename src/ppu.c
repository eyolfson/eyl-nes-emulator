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

/* TODO: Assume horizontal arrangement / vertical mirroring (64x30 tilemap) */

static uint8_t ctrl = 0;
static uint8_t status = 0;

static uint8_t *chr_rom_data;

#define RAM_SIZE 0x800 /* 2 KiB */
static uint8_t ram[RAM_SIZE];

#define OAM_SIZE 0x100 /* 256 B */
static uint8_t object_attribute_memory[OAM_SIZE];

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

uint8_t ppu_read(uint8_t address)
{
	switch (address) {
	case 0:
		return ctrl;
	case 2:
		return 0xE0;
	}
	return 0;
}

void ppu_write(uint8_t address, uint8_t value)
{
	switch (address) {
	case 0:
		ctrl = value;
		break;
	case 2:
		break;
	}
}
