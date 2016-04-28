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

#pragma once

#ifdef __cpluscplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct nes_emulator_console;

#define PPU_RAM_SIZE     0x0800 /*   2 KiB */
#define PPU_PALETTE_SIZE 0x0020 /*  32 B   */
#define PPU_OAM_SIZE     0x0100 /* 256 B   */

struct ppu {
	uint8_t ram[PPU_RAM_SIZE];
	uint8_t palette[PPU_PALETTE_SIZE];
	uint8_t oam[PPU_OAM_SIZE];

	bool computed_address_is_high;
	uint8_t computed_address_increment;
	uint16_t computed_address;

	uint8_t mask;
	uint8_t oam_address;
	uint16_t background_address;
	uint16_t sprite_address;
	uint16_t nametable_address;

	bool scroll_is_x;
	uint8_t scroll_x;
	uint8_t scroll_y;

	bool generate_nmi;
	bool trigger_vblank;

	uint16_t cycle;
	int16_t scan_line;
};

void ppu_init(struct nes_emulator_console *console);
uint8_t ppu_step(struct nes_emulator_console *console);

uint8_t ppu_cpu_bus_read(struct nes_emulator_console *console,
                         uint16_t address);
void ppu_cpu_bus_write(struct nes_emulator_console *console,
                       uint16_t address,
                       uint8_t value);

uint8_t ppu_bus_read(struct nes_emulator_console *console, uint16_t address);
void ppu_bus_write(struct nes_emulator_console *console,
                   uint16_t address,
                   uint8_t value);

/* TODO: Refactor to backend */
struct wayland;
extern struct wayland *wayland_ppu;

#ifdef __cpluscplus
}
#endif
