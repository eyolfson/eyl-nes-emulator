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

#include "memory_bus.h"

#include "ppu.h"
#include "prg_rom.h"
#include "ram.h"

uint8_t memory_read(uint16_t address)
{
	if (address < 0x0800) {
		return ram_read(address);
	}
	else if (address < 0x1000) {
		return ram_read(address - 0x0800);
	}
	else if (address < 0x1800) {
		return ram_read(address - 0x0100);
	}
	else if (address < 0x2000) {
		return ram_read(address - 0x1800);
	}
	else if (address < 0x4000) {
		return ppu_read(address % 8);
	}
	else if (address < 0x8000) {
		return 0;
	}
	else if (address < 0xC000) {
		return prg_rom_read_bank_1(address - 0x8000);
	}
	else {
		return prg_rom_read_bank_2(address - 0xC000);
	}
}

void memory_write(uint16_t address, uint8_t value)
{
	if (address < 0x0800) {
		ram_write(address, value);
	}
	else if (address < 0x1000) {
		ram_write(address - 0x0800, value);
	}
	else if (address < 0x1800) {
		ram_write(address - 0x1000, value);
	}
	else if (address < 0x2000) {
		ram_write(address - 0x1800, value);
	}
	else if (address < 0x4000) {
		ppu_write(address % 8, value);
	}
}
