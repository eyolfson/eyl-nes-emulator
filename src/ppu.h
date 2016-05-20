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

#pragma once

#ifdef __cpluscplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct nes_emulator_console;

#define PPU_RAM_SIZE           0x0800 /*   2 KiB */
#define PPU_PALETTE_SIZE       0x0020 /*  32 B   */
#define PPU_OAM_SIZE           0x0100 /* 256 B   */
#define PPU_SECONDARY_OAM_SIZE 0x0020 /*  32 B   */
#define PPU_BACKENDS_MAX 3

struct nes_emulator_ppu_backend {
	void *pointer;
	void (*render_pixel)(void *, uint8_t, uint8_t, uint8_t);
	void (*vertical_blank)(void *);
};

struct ppu_internal_registers {
	uint16_t v;
	uint16_t t;
	uint8_t x;
	uint8_t w;
};

struct ppu {
	uint8_t ram[PPU_RAM_SIZE];
	uint8_t palette[PPU_PALETTE_SIZE];
	uint8_t oam[PPU_OAM_SIZE];
	uint8_t secondary_oam[PPU_SECONDARY_OAM_SIZE];

	bool computed_address_is_high;
	uint8_t computed_address_increment;
	uint16_t computed_address;

	uint8_t mask;
	uint8_t oam_address;
	uint16_t background_address;
	uint16_t sprite_address;
	uint16_t nametable_address;

	bool is_sprite_0_in_secondary;
	bool is_sprite_0_hit_frame;
	bool is_sprite_0_hit;

	/* TODO: refactor */
	bool is_sprite_overflow;

	bool nmi_output;
	bool nmi_occurred;

	uint16_t cycle;
	int16_t scan_line;

	struct ppu_internal_registers internal_registers;

	struct nes_emulator_ppu_backend *backends[PPU_BACKENDS_MAX];
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

#ifdef __cpluscplus
}
#endif
