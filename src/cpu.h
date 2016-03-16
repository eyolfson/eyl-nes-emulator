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

#ifndef NES_EMULATOR_CPU_H
#define NES_EMULATOR_CPU_H

#ifdef __cpluscplus
extern "C" {
#endif

#include <stdint.h>

#define MEMORY_SIZE 0x10000

extern uint8_t memory[MEMORY_SIZE];

struct registers {
	uint8_t a;    /* Accumulator */
	uint8_t x;    /* Index Register 0 */
	uint8_t y;    /* Index Register 1 */
	uint8_t p;    /* Processor Status Flag Bits */
	uint8_t s;    /* Stack Pointer */
	uint16_t pc;  /* Program Counter */
};

void init_registers(struct registers *registers);

uint8_t execute_instruction(struct registers *registers);

#ifdef __cpluscplus
}
#endif

#endif
