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

#include <stdint.h>

#include "nes_emulator.h"

#define CPU_RAM_SIZE 0x800 /* 2 KiB */

struct registers {
	uint8_t a;    /* Accumulator */
	uint8_t x;    /* Index Register 0 */
	uint8_t y;    /* Index Register 1 */
	uint8_t p;    /* Processor Status Flag Bits */
	uint8_t s;    /* Stack Pointer */
	uint16_t pc;  /* Program Counter */
};

struct cpu {
	struct registers registers;
	uint8_t ram[CPU_RAM_SIZE];
};

void cpu_init(struct nes_emulator_console *console);
void cpu_reset(struct nes_emulator_console *console);
uint8_t cpu_step(struct nes_emulator_console *console);

#ifdef __cpluscplus
}
#endif
