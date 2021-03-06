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

	uint16_t computed_address;
	bool nmi_queued;
	bool nmi_delay; /* Allow the CPU to execute it's next instruction */

	bool controller_latch;
	uint8_t controller_shift;
	uint8_t controller_status;
	uint16_t dma_suspend_cycles;
};

void cpu_init(struct nes_emulator_console *console);
void cpu_reset(struct nes_emulator_console *console);
uint8_t cpu_step(struct nes_emulator_console *console);
void cpu_generate_nmi(struct nes_emulator_console *console);

#ifdef __cpluscplus
}
#endif
