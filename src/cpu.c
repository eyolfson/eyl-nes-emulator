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

#include "cpu.h"

#include "exit_code.h"

#include <stdbool.h>

uint8_t memory[MEMORY_SIZE];

void init_registers(struct registers *registers)
{
	registers->a = 0;
	registers->x = 0;
	registers->y = 0;
	registers->p = 0x24;
	registers->s = 0xFD;
	registers->pc = 0xC000;
}

/* Carry Flag */

static void clear_carry_flag(struct registers *registers)
{
	registers->p &= ~(1 << 0);
}

static void set_carry_flag(struct registers *registers)
{
	registers->p |= 1 << 0;
}

static void assign_carry_flag(struct registers *registers, bool c)
{
	if (c) { set_carry_flag(registers); }
	else { clear_carry_flag(registers); }
}

static bool get_carry_flag(struct registers *registers)
{
	return registers->p & (1 << 0);
}

/* Zero Flag */

static void clear_zero_flag(struct registers *registers)
{
	registers->p &= ~(1 << 1);
}

static void set_zero_flag(struct registers *registers)
{
	registers->p |= 1 << 1;
}

static void assign_zero_flag(struct registers *registers, bool z)
{
	if (z) { set_zero_flag(registers); }
	else { clear_zero_flag(registers); }
}

static bool get_zero_flag(struct registers *registers)
{
	return registers->p & 1 << 1;
}

static
void
set_interrupt_disable_flag(struct registers *registers, bool i)
{
	if (i) {
		registers->p |= 1 << 2;
	}
	else {
		registers->p &= ~(1 << 2);
	}
}

static
bool
get_interrupt_disable_flag(struct registers *registers)
{
	return registers->p & 1 << 2;
}

static
void
set_decimal_mode_flag(struct registers *registers, bool d)
{
	if (d) {
		registers->p |= 1 << 3;
	}
	else {
		registers->p &= ~(1 << 3);
	}
}

static
bool
get_decimal_mode_flag(struct registers *registers)
{
	return registers->p & 1 << 3;
}

static
void
set_break_command_flag(struct registers *registers, bool b)
{
	if (b) {
		registers->p |= 1 << 4;
	}
	else {
		registers->p &= ~(1 << 4);
	}
}

static
bool
get_break_command_flag(struct registers *registers)
{
	return registers->p & 1 << 4;
}

static
void
set_overflow_flag(struct registers *registers, bool v)
{
	if (v) {
		registers->p |= 1 << 6;
	}
	else {
		registers->p &= ~(1 << 6);
	}
}

static
bool
get_overflow_flag(struct registers *registers)
{
	return registers->p & 1 << 6;
}

static
void
set_negative_flag(struct registers *registers, bool n)
{
	if (n) {
		registers->p |= 1 << 7;
	}
	else {
		registers->p &= ~(1 << 7);
	}
}

static
bool
get_negative_flag(struct registers *registers)
{
	return registers->p & 1 << 7;
}

static
void
push_to_stack(struct registers *registers, uint8_t v)
{
	*(memory + 0x0100 + registers->s) = v;
	if (registers->s == 0x00) {
		registers->s = 0xFF;
	}
	else {
		registers->s -= 1;
	}
}

static
uint8_t
pop_from_stack(struct registers *registers)
{
	if (registers->s == 0xFF) {
		registers->s = 0x00;
	}
	else {
		registers->s += 1;
	}
	return *(memory + 0x0100 + registers->s);
}

static
uint8_t *
pointer_to_zero_page(uint8_t offset)
{
	return memory + offset;
}

static
uint8_t *
pointer_to_memory(uint16_t offset)
{
	return memory + offset;
}

static
uint8_t
get_byte_operand(struct registers *registers)
{
	uint8_t *operands_start = (memory + registers->pc + 1);
	return *operands_start;
}

static
uint16_t
get_2_byte_operand(struct registers *registers)
{
	uint8_t *operands_start = (memory + registers->pc + 1);
	return *operands_start + (*(operands_start + 1) << 8);
}

/* Addressing modes */

static
uint8_t
get_immediate_value(struct registers *registers)
{
	return get_byte_operand(registers);
}

static
uint8_t *
get_zero_page_pointer(struct registers *registers)
{
	return pointer_to_zero_page(get_byte_operand(registers));
}

static
uint8_t
get_zero_page_value(struct registers *registers)
{
	return *get_zero_page_pointer(registers);
}

static
uint8_t *
get_zero_page_x_pointer(struct registers *registers)
{
	return pointer_to_zero_page(get_byte_operand(registers) + registers->x);
}

static
uint8_t
get_zero_page_x_value(struct registers *registers)
{
	return *get_zero_page_x_pointer(registers);
}

static
uint8_t *
get_zero_page_y_pointer(struct registers *registers)
{
	return pointer_to_zero_page(get_byte_operand(registers) + registers->y);
}

static
uint8_t
get_zero_page_y_value(struct registers *registers)
{
	return *get_zero_page_y_pointer(registers);
}

static
uint8_t *
get_absolute_pointer(struct registers *registers)
{
	return pointer_to_memory(get_2_byte_operand(registers));
}

static
uint8_t
get_absolute_value(struct registers *registers)
{
	return *get_absolute_pointer(registers);
}

static
uint8_t *
get_absolute_x_pointer(struct registers *registers)
{
	return pointer_to_memory(get_2_byte_operand(registers) + registers->x);
}

static
uint8_t
get_absolute_x_value(struct registers *registers)
{
	return *get_absolute_x_pointer(registers);
}

static
uint8_t *
get_absolute_y_pointer(struct registers *registers)
{
	return pointer_to_memory(get_2_byte_operand(registers) + registers->y);
}

static
uint8_t
get_absolute_y_value(struct registers *registers)
{
	return *get_absolute_y_pointer(registers);
}

static
uint8_t *
get_indirect_y_pointer(struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(registers);

	/* This has zero page wrap around */
	uint16_t absolute_address = *pointer_to_zero_page(zero_page_address);
	zero_page_address += 1;
	absolute_address += *pointer_to_zero_page(zero_page_address) << 8;

	absolute_address += registers->y;

	return pointer_to_memory(absolute_address);
}

static
uint8_t
get_indirect_y_value(struct registers *registers)
{
	return *get_indirect_y_pointer(registers);
}

static
uint8_t *
get_indirect_x_pointer(struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(registers);
	zero_page_address += registers->x;

	/* This has zero page wrap around */
	uint16_t absolute_address = *pointer_to_zero_page(zero_page_address);
	zero_page_address += 1;
	absolute_address += *pointer_to_zero_page(zero_page_address) << 8;

	return pointer_to_memory(absolute_address);
}

static
uint8_t
get_indirect_x_value(struct registers *registers)
{
	return *get_indirect_x_pointer(registers);
}

static
uint8_t *
get_accumulator_pointer(struct registers *registers)
{
	return &(registers->a);
}

/* Execution */

static
void
sync_negative_and_zero_flags(struct registers *registers, uint8_t m)
{
	assign_zero_flag(registers, m == 0);
	set_negative_flag(registers, m & (1 << 7));
}

static
void
execute_compare(struct registers *registers, uint8_t r, uint8_t m)
{
	uint16_t result = r - m;
	assign_carry_flag(registers, result < 0x100);
	sync_negative_and_zero_flags(registers, result);
}

static
void
execute_add_with_carry(struct registers *registers, uint8_t v)
{
	int8_t a = (int8_t) registers->a;
	int8_t b = (int8_t) v;

	int16_t result = a + b;
	bool inc_carry = false;
	if (get_carry_flag(registers)) {
		if (result == -1) {
			inc_carry = true;
		}
		result += 1;
	}

	if (inc_carry) {
		set_carry_flag(registers);
	}
	else if (result > 0) {
		assign_carry_flag(registers, result & 0x100);
	}
	else {
		clear_carry_flag(registers);
	}

	/* If the operands have opposite signs, the sum will never overflow */
	if (a >= 0 && b >= 0 && (int8_t) result < 0) {
		set_overflow_flag(registers, true);
	}
	else if (a < 0 && b < 0 && (int8_t) result > 0) {
		set_overflow_flag(registers, true);
	}
	else {
		set_overflow_flag(registers, false);
	}

	set_negative_flag(registers, result & 0x80);
	assign_zero_flag(registers, result == 0);
	registers->a = (result & 0xFF);
}

static
void
execute_subtract_with_carry(struct registers *registers, uint8_t m)
{
	int8_t a = (int8_t) registers->a;
	int8_t b = (int8_t) m;

	int16_t result = a - b;
	uint16_t t = registers->a + ((uint8_t) (~m) + (uint8_t) 1);
	if (!get_carry_flag(registers)) {
		result -= 1;
		t += 0xFF;
	}

	assign_carry_flag(registers, t >= 0x100);
	set_overflow_flag(registers, result < -128 || result > 127);
	set_negative_flag(registers, result & 0x80);
	assign_zero_flag(registers, result == 0);
	registers->a = (result & 0xFF);
}

static
void
execute_logical_and(struct registers *registers, uint8_t m)
{
	registers->a &= m;
	sync_negative_and_zero_flags(registers, registers->a);
}

static
void
execute_logical_exclusive_or(struct registers *registers, uint8_t m)
{
	registers->a ^= m;
	sync_negative_and_zero_flags(registers, registers->a);
}

static
void
execute_logical_inclusive_or(struct registers *registers, uint8_t m)
{
	registers->a |= m;
	sync_negative_and_zero_flags(registers, registers->a);
}

static
void
execute_arithmetic_shift_left(struct registers *registers, uint8_t *pointer)
{
	assign_carry_flag(registers, *pointer & 0x80);
	*pointer <<= 1;
	sync_negative_and_zero_flags(registers, *pointer);
}

static
void
execute_logical_shift_right(struct registers *registers, uint8_t *pointer)
{
	assign_carry_flag(registers, *pointer & 0x01);
	*pointer >>= 1;
	sync_negative_and_zero_flags(registers, *pointer);
}

static
void
execute_rotate_left(struct registers *registers, uint8_t *pointer)
{
	bool current_carry_flag = get_carry_flag(registers);
	assign_carry_flag(registers, *pointer & 0x80);
	*pointer <<= 1;
	if (current_carry_flag) {
		*pointer |= 0x01;
	}
	sync_negative_and_zero_flags(registers, *pointer);
}

static
void
execute_rotate_right(struct registers *registers, uint8_t *pointer)
{
	bool current_carry_flag = get_carry_flag(registers);
	assign_carry_flag(registers, *pointer & 0x01);
	*pointer >>= 1;
	if (current_carry_flag) {
		*pointer |= 0x80;
	}
	sync_negative_and_zero_flags(registers, *pointer);
}

static
void
execute_decrement_memory(struct registers *registers, uint8_t *pointer)
{
	*pointer -= 1;
	sync_negative_and_zero_flags(registers, *pointer);
}

static
void
execute_increment_memory(struct registers *registers, uint8_t *pointer)
{
	*pointer += 1;
	sync_negative_and_zero_flags(registers, *pointer);
}

static
void
execute_bit_test(struct registers *registers, uint8_t m)
{
	set_negative_flag(registers, m & (1 << 7));
	set_overflow_flag(registers, m & (1 << 6));
	assign_zero_flag(registers, (registers->a & m) == 0);
}

static
void
execute_subtract_with_carry_for_isb(struct registers *registers, uint8_t m)
{
	int8_t a = (int8_t) registers->a;
	int8_t b = (int8_t) m;

	int16_t result = a - b;
	if (!get_carry_flag(registers)) {
		result -= 1;
	}

	uint16_t unsigned_result = ~(registers->a) + m;
	if (get_carry_flag(registers)) {
		unsigned_result += 1;
	}

	assign_carry_flag(registers, unsigned_result >= 0x100);
	set_overflow_flag(registers, result < -128 || result > 127);
	set_negative_flag(registers, result & 0x80);
	assign_zero_flag(registers, result == 0);
	registers->a = (result & 0xFF);
}

static
void
execute_isb(struct registers *registers, uint8_t *pointer)
{
	execute_increment_memory(registers, pointer);
	execute_subtract_with_carry_for_isb(registers, *pointer);
}

static
void
execute_dcp(struct registers *registers, uint8_t *pointer)
{
	execute_decrement_memory(registers, pointer);
	execute_compare(registers, registers->a, *pointer);
}

static
void
execute_slo(struct registers *registers, uint8_t *pointer)
{
	execute_arithmetic_shift_left(registers, pointer);
	execute_logical_inclusive_or(registers, *pointer);
}

static
void
execute_rla(struct registers *registers, uint8_t *pointer)
{
	execute_rotate_left(registers, pointer);
	execute_logical_and(registers, *pointer);
}

static
void
execute_sre(struct registers *registers, uint8_t *pointer)
{
	execute_logical_shift_right(registers, pointer);
	execute_logical_exclusive_or(registers, *pointer);
}

static
void
execute_add_with_carry_rra(struct registers *registers, uint8_t v)
{
	int8_t a = (int8_t) registers->a;
	int8_t b = (int8_t) v;

	int16_t result = a + b;
	bool inc_carry = false;
	if (get_carry_flag(registers)) {
		if (result == -1) {
			inc_carry = true;
		}
		result += 1;
	}

	uint16_t unsigned_result = registers->a + v;
	if (get_carry_flag(registers)) {
		unsigned_result += 1;
	}
	assign_carry_flag(registers, unsigned_result & 0x100);

	/* If the operands have opposite signs, the sum will never overflow */
	if (a >= 0 && b >= 0 && (int8_t) result < 0) {
		set_overflow_flag(registers, true);
	}
	else if (a < 0 && b < 0 && (int8_t) result > 0) {
		set_overflow_flag(registers, true);
	}
	else {
		set_overflow_flag(registers, false);
	}

	set_negative_flag(registers, result & 0x80);
	assign_zero_flag(registers, result == 0);
	registers->a = (result & 0xFF);
}

static
void
execute_rra(struct registers *registers, uint8_t *pointer)
{
	execute_rotate_right(registers, pointer);
	execute_add_with_carry_rra(registers, *pointer);
}

uint8_t execute_instruction(struct registers *registers)
{
	uint8_t *memory_pc_offset = memory + registers->pc;
	uint8_t opcode = *(memory_pc_offset);

	uint16_t t1 = 0;
	uint16_t t2 = 0;

	/* Processor is little-endian */
	switch (opcode) {
	case 0x01:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 6 */
		execute_logical_inclusive_or(registers,
			get_indirect_x_value(registers));
		registers->pc += 2;
		break;
	case 0x03:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_slo(registers,
			get_indirect_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x04:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0x05:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 3 */
		execute_logical_inclusive_or(registers,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0x06:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 5 */
		execute_arithmetic_shift_left(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0x07:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 5 */
		execute_slo(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0x08:
		/* PHP - Push Processor Status */
		/* Cycles: 3 */
		push_to_stack(registers, registers->p);
		registers->pc += 1;
		break;
	case 0x09:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 2 */
		execute_logical_inclusive_or(registers,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0x0A:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 2 */
		execute_arithmetic_shift_left(registers,
			get_accumulator_pointer(registers));
		registers->pc += 1;
		break;
	case 0x0C:
		/* NOP - No Operation */
		/* Illegal Opcode (Absolute) */
		/* Cycles: TODO */
		registers->pc += 3;
		break;
	case 0x0D:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 4 */
		execute_logical_inclusive_or(registers,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0x0E:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 6 */
		execute_arithmetic_shift_left(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0x0F:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_slo(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0x10:
		/* BPL - Branch if Positive */
		/* Addressing is relative */
		/* Bytes: 2 */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		registers->pc += 2;
		if (!get_negative_flag(registers)) {
			t1 = *(memory_pc_offset + 1);
			registers->pc += t1;
		}
		break;
	case 0x11:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 5 (+1 if page crossed) */
		execute_logical_inclusive_or(registers,
			get_indirect_y_value(registers));
		registers->pc += 2;
		break;
	case 0x13:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_slo(registers,
			get_indirect_y_pointer(registers));
		registers->pc += 2;
		break;
	case 0x14:
		/* NOP - No Operation */
		/* Illegal Opcode (Zero Page X) */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0x15:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 4 */
		execute_logical_inclusive_or(registers,
			get_zero_page_x_value(registers));
		registers->pc += 2;
		break;
	case 0x16:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 6 */
		execute_arithmetic_shift_left(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x17:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_slo(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x18:
		/* CLC - Clear Carry Flag */
		/* Cycles: 2 */
		clear_carry_flag(registers);
		registers->pc += 1;
		break;
	case 0x19:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 4 (+1 if page crossed) */
		execute_logical_inclusive_or(registers,
			get_absolute_y_value(registers));
		registers->pc += 3;
		break;
	case 0x1A:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 1;
		break;
	case 0x1B:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_slo(registers,
			get_absolute_y_pointer(registers));
		registers->pc += 3;
		break;
	case 0x1C:
		/* NOP - No Operation */
		/* Illegal Opcode (Absolute X) */
		/* Cycles: TODO */
		registers->pc += 3;
		break;
	case 0x1D:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 4 (+1 if page crossed) */
		execute_logical_inclusive_or(registers,
			get_absolute_x_value(registers));
		registers->pc += 3;
		break;
	case 0x1E:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 7 */
		execute_arithmetic_shift_left(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x1F:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_slo(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x20:
		/* JSR - Jump to Subroutine */
		/* Address is absolute */
		/* Bytes: 3 */
		/* Cycles: 6 */
		t1 = *(memory_pc_offset + 1) + (*(memory_pc_offset + 2) << 8);

		t2 = registers->pc + 3 - 1;
		push_to_stack(registers, (t2 & 0xFF00) >> 8);
		push_to_stack(registers, t2 & 0x00FF);

		registers->pc = t1;
		break;
	case 0x21:
		/* AND - Logical AND */
		/* Cycles: 6 */
		execute_logical_and(registers,
			get_indirect_x_value(registers));
		registers->pc += 2;
		break;
	case 0x23:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_rla(registers,
			get_indirect_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x24:
		/* BIT - Bit Test */
		/* Cycles: 3 */
		execute_bit_test(registers,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0x25:
		/* AND - Logical AND */
		/* Cycles: 3 */
		execute_logical_and(registers,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0x26:
		/* ROL - Rotate Left */
		/* Cycles: 5 */
		execute_rotate_left(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0x27:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 5 */
		execute_rla(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0x28:
		/* PLP - Pull Processor Status */
		/* Bytes: 1 */
		/* Cycles: 4 */

		/* TODO: perserve break command flag? */
		t1 = get_break_command_flag(registers);
		registers->p = pop_from_stack(registers);
		/* TODO: flag seems to always be set? */
		registers->p |= 0x20;
		if (t1) {
			set_break_command_flag(registers, true);
		}
		else {
			set_break_command_flag(registers, false);
		}
		registers->pc += 1;
		break;
	case 0x29:
		/* AND - Logical AND */
		/* Cycles: 2 */
		execute_logical_and(registers,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0x2A:
		/* ROL - Rotate Left */
		/* Cycles: 2 */
		execute_rotate_left(registers,
			get_accumulator_pointer(registers));
		registers->pc += 1;
		break;
	case 0x2C:
		/* BIT - Bit Test */
		/* Cycles: 4 */
		execute_bit_test(registers,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0x2D:
		/* AND - Logical AND */
		/* Cycles: 4 */
		execute_logical_and(registers,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0x2E:
		/* ROL - Rotate Left */
		/* Cycles: 6 */
		execute_rotate_left(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0x2F:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_rla(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0x30:
		/* BMI - Branch if Minus */
		/* Addressing is relative */
		/* Bytes: 2 */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		registers->pc += 2;
		if (get_negative_flag(registers)) {
			t1 = *(memory_pc_offset + 1);
			registers->pc += t1;
		}
		break;
	case 0x31:
		/* AND - Logical AND */
		/* Cycles: 5 (+1 if page crossed) */
		execute_logical_and(registers,
			get_indirect_y_value(registers));
		registers->pc += 2;
		break;
	case 0x33:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_rla(registers,
			get_indirect_y_pointer(registers));
		registers->pc += 2;
		break;
	case 0x34:
		/* NOP - No Operation */
		/* Illegal Opcode (Zero Page Y) */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0x35:
		/* AND - Logical AND */
		/* Cycles: 4 */
		execute_logical_and(registers,
			get_zero_page_x_value(registers));
		registers->pc += 2;
		break;
	case 0x36:
		/* ROL - Rotate Left */
		/* Cycles: 6 */
		execute_rotate_left(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x37:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_rla(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x38:
		/* SEC - Set Carry Flag */
		/* Cycles: 2 */
		set_carry_flag(registers);
		registers->pc += 1;
		break;
	case 0x39:
		/* AND - Logical AND */
		/* Cycles: 4 (+1 if page crossed) */
		execute_logical_and(registers,
			get_absolute_y_value(registers));
		registers->pc += 3;
		break;
	case 0x3A:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 1;
		break;
	case 0x3B:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_rla(registers,
			get_absolute_y_pointer(registers));
		registers->pc += 3;
		break;
	case 0x3C:
		/* NOP - No Operation */
		/* Illegal Opcode (Absolute X) */
		/* Cycles: TODO */
		registers->pc += 3;
		break;
	case 0x3D:
		/* AND - Logical AND */
		/* Cycles: 4 (+1 if page crossed) */
		execute_logical_and(registers,
			get_absolute_x_value(registers));
		registers->pc += 3;
		break;
	case 0x3E:
		/* ROL - Rotate Left */
		/* Cycles: 7 */
		execute_rotate_left(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x3F:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_rla(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x40:
		/* RTI - Return from Interrupt */
		/* Bytes: 1 */
		/* Cycles: 6 */
		t1 = get_break_command_flag(registers);
		registers->p = pop_from_stack(registers);
		/* TODO: flag seems to always be set? */
		registers->p |= 0x20;
		if (t1) {
			set_break_command_flag(registers, true);
		}
		else {
			set_break_command_flag(registers, false);
		}
		t2 = pop_from_stack(registers);
		t2 += pop_from_stack(registers) << 8;
		registers->pc = t2;
		break;
	case 0x41:
		/* EOR - Exclusive OR */
		/* Cycles: 6 */
		execute_logical_exclusive_or(registers,
			get_indirect_x_value(registers));
		registers->pc += 2;
		break;
	case 0x43:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_sre(registers,
			get_indirect_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x44:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0x45:
		/* EOR - Exclusive OR */
		/* Cycles: 3 */
		execute_logical_exclusive_or(registers,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0x46:
		/* LSR - Logical Shift Right */
		/* Cycles: 5 */
		execute_logical_shift_right(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0x47:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 5 */
		execute_sre(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0x48:
		/* PHA - Push Accumulator */
		/* Cycles: 3 */
		push_to_stack(registers, registers->a);
		registers->pc += 1;
		break;
	case 0x49:
		/* EOR - Exclusive OR */
		/* Cycles: 2 */
		execute_logical_exclusive_or(registers,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0x4A:
		/* LSR - Logical Shift Right */
		/* Cycles: 2 */
		execute_logical_shift_right(registers,
			get_accumulator_pointer(registers));
		registers->pc += 1;
		break;
	case 0x4C:
		/* JMP - Jump */
		/* Address is absolute */
		/* Bytes: 3 */
		/* Cycles: 3 */
		registers->pc = get_2_byte_operand(registers);
		break;
	case 0x4D:
		/* EOR - Exclusive OR */
		/* Cycles: 4 */
		execute_logical_exclusive_or(registers,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0x4E:
		/* LSR - Logical Shift Right */
		/* Cycles: 6 */
		execute_logical_shift_right(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0x4F:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_sre(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0x50:
		/* BVC - Branch if Overflow Clear */
		/* Addressing is relative */
		/* Bytes: 2 */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		registers->pc += 2;
		if (!get_overflow_flag(registers)) {
			t1 = *(memory_pc_offset + 1);
			registers->pc += t1;
		}
		break;
	case 0x51:
		/* EOR - Exclusive OR */
		/* Cycles: 5 (+1 if page crossed) */
		execute_logical_exclusive_or(registers,
			get_indirect_y_value(registers));
		registers->pc += 2;
		break;
	case 0x53:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_sre(registers,
			get_indirect_y_pointer(registers));
		registers->pc += 2;
		break;
	case 0x54:
		/* NOP - No Operation */
		/* Illegal Opcode (Zero Page X) */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0x55:
		/* EOR - Exclusive OR */
		/* Cycles: 4 */
		execute_logical_exclusive_or(registers,
			get_zero_page_x_value(registers));
		registers->pc += 2;
		break;
	case 0x56:
		/* LSR - Logical Shift Right */
		/* Cycles: 6 */
		execute_logical_shift_right(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x57:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_sre(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x59:
		/* EOR - Exclusive OR */
		/* Cycles: 4 (+1 if page crossed) */
		execute_logical_exclusive_or(registers,
			get_absolute_y_value(registers));
		registers->pc += 3;
		break;
	case 0x5A:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 1;
		break;
	case 0x5B:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_sre(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x5C:
		/* NOP - No Operation */
		/* Illegal Opcode (Absolute X) */
		/* Cycles: TODO */
		registers->pc += 3;
		break;
	case 0x5D:
		/* EOR - Exclusive OR */
		/* Cycles: 4 (+1 if page crossed) */
		execute_logical_exclusive_or(registers,
			get_absolute_x_value(registers));
		registers->pc += 3;
		break;
	case 0x5E:
		/* LSR - Logical Shift Right */
		/* Cycles: 7 */
		execute_logical_shift_right(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x5F:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_sre(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x60:
		/* RTS - Return from Subroutine */
		/* Bytes: 1 */
		/* Cycles: 6 */
		t1 = pop_from_stack(registers);
		t1 += pop_from_stack(registers) << 8;
		t1 += 1;
		registers->pc = t1;
		break;
	case 0x61:
		/* ADC - Add with Carry */
		/* Cycles: 6 */
		execute_add_with_carry(registers,
			get_indirect_x_value(registers));
		registers->pc += 2;
		break;
	case 0x63:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_rra(registers,
			get_indirect_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x64:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0x65:
		/* ADC - Add with Carry */
		/* Cycles: 3 */
		execute_add_with_carry(registers,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0x66:
		/* ROR - Rotate Right */
		/* Cycles: 5 */
		execute_rotate_right(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0x67:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 5 */
		execute_rra(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0x68:
		/* PLA - Pull Accumulator */
		/* Cycles: 4 */
		registers->a = pop_from_stack(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 1;
		break;
	case 0x69:
		/* ADC - Add with Carry */
		/* Cycles: 2 */
		execute_add_with_carry(registers,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0x6A:
		/* ROR - Rotate Right */
		/* Cycles: 2 */
		execute_rotate_right(registers,
			get_accumulator_pointer(registers));
		registers->pc += 1;
		break;
	case 0x6C:
		/* JMP - Jump */
		/* Address is indirect */
		/* Bytes: 3 */
		/* Cycles: 5 */
		t1 = *(memory_pc_offset + 1) + (*(memory_pc_offset + 2) << 8);
		t2 = *(memory + t1);
		/* If the address is 0x02FF, the address of the low byte,
		   the high byte is in 0x0200, not 0x0300 */
		if ((t1 & 0xFF) == 0xFF) {
			t2 += (*(memory + (t1 & 0xFF00)) << 8);
		}
		else {
			t2 += (*(memory + t1 + 1) << 8);
		}
		registers->pc = t2;
		break;
	case 0x6D:
		/* ADC - Add with Carry */
		/* Cycles: 4 */
		execute_add_with_carry(registers,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0x6E:
		/* ROR - Rotate Right */
		/* Cycles: 6 */
		execute_rotate_right(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0x6F:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_rra(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0x70:
		/* BVS - Branch if Overflow Set */
		/* Addressing is relative */
		/* Bytes: 2 */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		registers->pc += 2;
		if (get_overflow_flag(registers)) {
			t1 = *(memory_pc_offset + 1);
			registers->pc += t1;
		}
		break;
	case 0x71:
		/* ADC - Add with Carry */
		/* Cycles: 5 (+1 if page crossed) */
		execute_add_with_carry(registers,
			get_indirect_y_value(registers));
		registers->pc += 2;
		break;
	case 0x73:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_rra(registers,
			get_indirect_y_pointer(registers));
		registers->pc += 2;
		break;
	case 0x74:
		/* NOP - No Operation */
		/* Illegal Opcode (Zero Page X) */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0x75:
		/* ADC - Add with Carry */
		/* Cycles: 4 */
		execute_add_with_carry(registers,
			get_zero_page_x_value(registers));
		registers->pc += 2;
		break;
	case 0x76:
		/* ROR - Rotate Right */
		/* Cycles: 6 */
		execute_rotate_right(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x77:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_rra(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0x78:
		/* SEI - Set Interrupt Disable */
		/* Cycles: 2 */
		set_interrupt_disable_flag(registers, true);
		registers->pc += 1;
		break;
	case 0x79:
		/* ADC - Add with Carry */
		/* Cycles: 4 (+1 if page crossed) */
		execute_add_with_carry(registers,
			get_absolute_y_value(registers));
		registers->pc += 3;
		break;
	case 0x7A:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 1;
		break;
	case 0x7B:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_rra(registers,
			get_absolute_y_pointer(registers));
		registers->pc += 3;
		break;
	case 0x7C:
		/* NOP - No Operation */
		/* Illegal Opcode (Absolute X) */
		/* Cycles: TODO */
		registers->pc += 3;
		break;
	case 0x7D:
		/* ADC - Add with Carry */
		/* Cycles: 4 (+1 if page crossed) */
		execute_add_with_carry(registers,
			get_absolute_x_value(registers));
		registers->pc += 3;
		break;
	case 0x7E:
		/* ROR - Rotate Right */
		/* Cycles: 7 */
		execute_rotate_right(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x7F:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_rra(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0x80:
		/* NOP - No Operation */
		/* Illegal Opcode (Immediate) */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0x81:
		/* STA - Store Accumulator */
		/* Cycles: 6 */
		*get_indirect_x_pointer(registers) = registers->a;
		registers->pc += 2;
		break;
	case 0x83:
		/* SAX */
		/* Illegal Opcode */
		/* Cycles: 6 */
		*get_indirect_x_pointer(registers) = registers->a
		                                     & registers->x;
		registers->pc += 2;
		break;
	case 0x84:
		/* STY - Store Y Register */
		/* Cycles: 3 */
		*get_zero_page_pointer(registers) = registers->y;
		registers->pc += 2;
		break;
	case 0x85:
		/* STA - Store Accumulator */
		/* Cycles: 3 */
		*get_zero_page_pointer(registers) = registers->a;
		registers->pc += 2;
		break;
	case 0x86:
		/* STX - Store X Register */
		/* Cycles: 3 */
		*get_zero_page_pointer(registers) = registers->x;
		registers->pc += 2;
		break;
	case 0x87:
		/* SAX */
		/* Illegal Opcode */
		/* Cycles: 3 */
		*get_zero_page_pointer(registers) = registers->a
		                                    & registers->x;
		registers->pc += 2;
		break;
	case 0x88:
		/* DEY - Decrement Y Register */
		/* Cycles: 2 */
		registers->y -= 1;
		sync_negative_and_zero_flags(registers, registers->y);
		registers->pc += 1;
		break;
	case 0x8A:
		/* TXA - Transfer X to Accumulator */
		/* Cycles: 2 */
		registers->a = registers->x;
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 1;
		break;
	case 0x8C:
		/* STY - Store Y Register */
		/* Cycles: 4 */
		*get_absolute_pointer(registers) = registers->y;
		registers->pc += 3;
		break;
	case 0x8D:
		/* STA - Store Accumulator */
		/* Cycles: 4 */
		*get_absolute_pointer(registers) = registers->a;
		registers->pc += 3;
		break;
	case 0x8E:
		/* STX - Store X Register */
		/* Cycles: 4 */
		*get_absolute_pointer(registers) = registers->x;
		registers->pc += 3;
		break;
	case 0x8F:
		/* SAX */
		/* Illegal Opcode */
		/* Cycles: 4 */
		*get_absolute_pointer(registers) = registers->a
		                                   & registers->x;
		registers->pc += 3;
		break;
	case 0x90:
		/* BCC - Branch if Carry Clear */
		/* Addressing is relative */
		/* Bytes: 2 */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		registers->pc += 2;
		if (!get_carry_flag(registers)) {
			t1 = *(memory_pc_offset + 1);
			registers->pc += t1;
		}
		break;
	case 0x91:
		/* STA - Store Accumulator */
		/* Cycles: 6 */
		*get_indirect_y_pointer(registers) = registers->a;
		registers->pc += 2;
		break;
	case 0x94:
		/* STY - Store Y Register */
		/* Cycles: 4 */
		*get_zero_page_x_pointer(registers) = registers->y;
		registers->pc += 2;
		break;
	case 0x95:
		/* STA - Store Accumulator */
		/* Cycles: 4 */
		*get_zero_page_x_pointer(registers) = registers->a;
		registers->pc += 2;
		break;
	case 0x96:
		/* STX - Store X Register */
		/* Cycles: 4 */
		*get_zero_page_y_pointer(registers) = registers->x;
		registers->pc += 2;
		break;
	case 0x97:
		/* SAX */
		/* Illegal Opcode */
		/* Cycles: 4 */
		*get_zero_page_y_pointer(registers) = registers->a
		                                     & registers->x;
		registers->pc += 2;
		break;
	case 0x98:
		/* TYA - Transfer Y to Accumulator */
		/* Cycles: 2 */
		registers->a = registers->y;
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 1;
		break;
	case 0x99:
		/* STA - Store Accumulator */
		/* Cycles: 5 */
		*get_absolute_y_pointer(registers) = registers->a;
		registers->pc += 3;
		break;
	case 0x9A:
		/* TXS - Transfer X to Stack Pointer */
		/* Cycles: 2 */
		registers->s = registers->x;
		registers->pc += 1;
		break;
	case 0x9D:
		/* STA - Store Accumulator */
		/* Cycles: 5 */
		*get_absolute_x_pointer(registers) = registers->a;
		registers->pc += 3;
		break;
	case 0xA0:
		/* LDY - Load Y Register */
		/* Cycles: 2 */
		registers->y = get_immediate_value(registers);
		sync_negative_and_zero_flags(registers, registers->y);
		registers->pc += 2;
		break;
	case 0xA1:
		/* LDA - Load Accumlator */
		/* Cycles: 6 */
		registers->a = get_indirect_x_value(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xA2:
		/* LDX - Load X Register */
		/* Cycles: 2 */
		registers->x = get_immediate_value(registers);
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 2;
		break;
	case 0xA3:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 6 */
		t1 = get_indirect_x_value(registers);
		registers->a = t1;
		registers->x = t1;
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xA4:
		/* LDY - Load Y Register */
		/* Cycles: 3 */
		registers->y = get_zero_page_value(registers);
		sync_negative_and_zero_flags(registers, registers->y);
		registers->pc += 2;
		break;
	case 0xA5:
		/* LDA - Load Accumlator */
		/* Cycles: 3 */
		registers->a = get_zero_page_value(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xA6:
		/* LDX - Load X Register */
		/* Cycles: 3 */
		registers->x = get_zero_page_value(registers);
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 2;
		break;
	case 0xA7:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 3 */
		t1 = get_zero_page_value(registers);
		registers->a = t1;
		registers->x = t1;
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xA8:
		/* TAY - Transfer Accumulator to Y */
		/* Cycles: 2 */
		registers->y = registers->a;
		sync_negative_and_zero_flags(registers, registers->y);
		registers->pc += 1;
		break;
	case 0xA9:
		/* LDA - Load Accumlator */
		/* Cycles: 2 */
		registers->a = get_immediate_value(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xAA:
		/* TAY - Transfer Accumulator to X */
		/* Cycles: 2 */
		registers->x = registers->a;
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 1;
		break;
	case 0xAC:
		/* LDY - Load Y Register */
		/* Cycles: 4 */
		registers->y = get_absolute_value(registers);
		sync_negative_and_zero_flags(registers, registers->y);
		registers->pc += 3;
		break;
	case 0xAD:
		/* LDA - Load Acuumulator */
		/* Cycles: 4 */
		registers->a = get_absolute_value(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 3;
		break;
	case 0xAE:
		/* LDX - Load X Register */
		/* Cycles: 4 */
		registers->x = get_absolute_value(registers);
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 3;
		break;
	case 0xAF:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 4 */
		t1 = get_absolute_value(registers);
		registers->a = t1;
		registers->x = t1;
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 3;
		break;
	case 0xB0:
		/* BCS - Branch if Carry Set */
		/* Addressing is relative */
		/* Bytes: 2 */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		registers->pc += 2;
		if (get_carry_flag(registers)) {
			t1 = *(memory_pc_offset + 1);
			registers->pc += t1;
		}
		break;
	case 0xB1:
		/* LDA - Load Accumlator */
		/* Cycles: 5 (+1 if page crossed) */
		registers->a = get_indirect_y_value(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xB3:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 5 (+1 if page crossed) */
		t1 = get_indirect_y_value(registers);
		registers->a = t1;
		registers->x = t1;
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xB4:
		/* LDY - Load Y Register */
		/* Cycles: 4 */
		registers->y = get_zero_page_x_value(registers);
		sync_negative_and_zero_flags(registers, registers->y);
		registers->pc += 2;
		break;
	case 0xB5:
		/* LDA - Load Accumlator */
		/* Cycles: 4 */
		registers->a = get_zero_page_x_value(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xB6:
		/* LDX - Load X Register */
		/* Cycles: 4 */
		registers->x = get_zero_page_y_value(registers);
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 2;
		break;
	case 0xB7:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 4 */
		t1 = get_zero_page_y_value(registers);
		registers->a = t1;
		registers->x = t1;
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xB8:
		/* CLV - Clear Overflow Flag */
		/* Cycles: 2 */
		set_overflow_flag(registers, false);
		registers->pc += 1;
		break;
	case 0xB9:
		/* LDA - Load Acuumulator */
		/* Cycles: 4 (+1 if page crossed) */
		registers->a = get_absolute_y_value(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 3;
		break;
	case 0xBA:
		/* TSX - Transfer Stack Pointer to X */
		/* Cycles: 2 */
		registers->x = registers->s;
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 1;
		break;
	case 0xBC:
		/* LDY - Load Y Register */
		/* Cycles: 4 (+1 if page crossed) */
		registers->y = get_absolute_x_value(registers);
		sync_negative_and_zero_flags(registers, registers->y);
		registers->pc += 3;
		break;
	case 0xBD:
		/* LDA - Load Acuumulator */
		/* Cycles: 4 (+1 if page crossed) */
		registers->a = get_absolute_x_value(registers);
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 3;
		break;
	case 0xBE:
		/* LDX - Load X Register */
		/* Cycles: 4 (+1 if page crossed) */
		registers->x = get_absolute_y_value(registers);
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 3;
		break;
	case 0xBF:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 4 (+1 if page crossed) */
		t1 = get_absolute_y_value(registers);
		registers->a = t1;
		registers->x = t1;
		sync_negative_and_zero_flags(registers, registers->a);
		registers->pc += 3;
		break;
	case 0xC0:
		/* CPY - Compare Y Register */
		/* Cycles: 2 */
		execute_compare(registers, registers->y,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0xC1:
		/* CMP - Compare */
		/* Cycles: 6 */
		execute_compare(registers, registers->a,
			get_indirect_x_value(registers));
		registers->pc += 2;
		break;
	case 0xC3:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_dcp(registers,
			get_indirect_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0xC4:
		/* CPY - Compare Y Register */
		/* Cycles: 3 */
		execute_compare(registers, registers->y,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0xC5:
		/* CMP - Compare */
		/* Cycles: 3 */
		execute_compare(registers, registers->a,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0xC6:
		/* DEC - Decrement Memory */
		/* Cycles: 5 */
		execute_decrement_memory(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0xC7:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 5 */
		execute_dcp(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0xC8:
		/* INY - Increment Y Register */
		/* Cycles: 2 */
		registers->y += 1;
		sync_negative_and_zero_flags(registers, registers->y);
		registers->pc += 1;
		break;
	case 0xC9:
		/* CMP - Compare */
		/* Cycles: 2 */
		execute_compare(registers, registers->a,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0xCA:
		/* DEX - Decrement X Register */
		/* Cycles: 2 */
		registers->x -= 1;
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 1;
		break;
	case 0xCC:
		/* CPY - Compare Y Register */
		/* Cycles: 4 */
		execute_compare(registers, registers->y,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0xCD:
		/* CMP - Compare */
		/* Cycles: 4 */
		execute_compare(registers, registers->a,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0xCE:
		/* DEC - Decrement Memory */
		/* Cycles: 6 */
		execute_decrement_memory(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0xCF:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_dcp(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0xD0:
		/* BNE - Branch if Not Equal */
		/* Addressing is relative */
		/* Bytes: 2 */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		registers->pc += 2;
		if (!get_zero_flag(registers)) {
			t1 = *(memory_pc_offset + 1);
			/* TODO: the relative can be negative */
			registers->pc += (int8_t) t1;
		}
		break;
	case 0xD1:
		/* CMP - Compare */
		/* Cycles: 5 (+1 if page crossed) */
		execute_compare(registers, registers->a,
			get_indirect_y_value(registers));
		registers->pc += 2;
		break;
	case 0xD3:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_dcp(registers,
			get_indirect_y_pointer(registers));
		registers->pc += 2;
		break;
	case 0xD4:
		/* NOP - No Operation */
		/* Illegal Opcode (Zero Page X) */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0xD5:
		/* CMP - Compare */
		/* Cycles: 4 */
		execute_compare(registers, registers->a,
			get_zero_page_x_value(registers));
		registers->pc += 2;
		break;
	case 0xD6:
		/* DEC - Decrement Memory */
		/* Cycles: 6 */
		execute_decrement_memory(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0xD7:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_dcp(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0xD8:
		/* CLD - Clear Decimal Mode */
		/* Cycles: 2 */
		set_decimal_mode_flag(registers, false);
		registers->pc += 1;
		break;
	case 0xD9:
		/* CMP - Compare */
		/* Cycles: 4 (+1 if page crossed) */
		execute_compare(registers, registers->a,
			get_absolute_y_value(registers));
		registers->pc += 3;
		break;
	case 0xDA:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 1;
		break;
	case 0xDB:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_dcp(registers,
			get_absolute_y_pointer(registers));
		registers->pc += 3;
		break;
	case 0xDC:
		/* NOP - No Operation */
		/* Illegal Opcode (Absolute X) */
		/* Cycles: TODO */
		registers->pc += 3;
		break;
	case 0xDD:
		/* CMP - Compare */
		/* Cycles: 4 (+1 if page crossed) */
		execute_compare(registers, registers->a,
			get_absolute_x_value(registers));
		registers->pc += 3;
		break;
	case 0xDE:
		/* DEC - Decrement Memory */
		/* Cycles: 7 */
		execute_decrement_memory(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0xDF:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_dcp(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0xE0:
		/* CPX - Compare X Register */
		/* Cycles: 2 */
		execute_compare(registers, registers->x,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0xE1:
		/* SBC - Subtract with Carry */
		/* Cycles: 6 */
		execute_subtract_with_carry(registers,
			get_indirect_x_value(registers));
		registers->pc += 2;
		break;
	case 0xE3:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_isb(registers,
			get_indirect_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0xE4:
		/* CPX - Compare X Register */
		/* Cycles: 3 */
		execute_compare(registers, registers->x,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0xE5:
		/* SBC - Subtract with Carry */
		/* Cycles: 3 */
		execute_subtract_with_carry(registers,
			get_zero_page_value(registers));
		registers->pc += 2;
		break;
	case 0xE6:
		/* INC - Increment Memory */
		/* Cycles: 5 */
		execute_increment_memory(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0xE7:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 5 */
		execute_isb(registers,
			get_zero_page_pointer(registers));
		registers->pc += 2;
		break;
	case 0xE8:
		/* INX - Increment X Register */
		/* Cycles: 2 */
		registers->x += 1;
		sync_negative_and_zero_flags(registers, registers->x);
		registers->pc += 1;
		break;
	case 0xE9:
		/* SBC - Subtract with Carry */
		/* Cycles: 2 */
		execute_subtract_with_carry(registers,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0xEA:
		/* NOP - No Operation */
		/* Cycles: 2 */
		registers->pc += 1;
		break;
	case 0xEB:
		/* SBC - Subtract with Carry */
		/* Illegal Opcode */
		/* Cycles: 2 */
		execute_subtract_with_carry(registers,
			get_immediate_value(registers));
		registers->pc += 2;
		break;
	case 0xEC:
		/* CPX - Compare X Register */
		/* Cycles: 4 */
		execute_compare(registers, registers->x,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0xED:
		/* SBC - Subtract with Carry */
		/* Cycles: 4 */
		execute_subtract_with_carry(registers,
			get_absolute_value(registers));
		registers->pc += 3;
		break;
	case 0xEE:
		/* INC - Increment Memory */
		/* Cycles: 6 */
		execute_increment_memory(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0xEF:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_isb(registers,
			get_absolute_pointer(registers));
		registers->pc += 3;
		break;
	case 0xF0:
		/* BEQ - Branch if Equal */
		/* Addressing is relative */
		/* Bytes: 2 */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		registers->pc += 2;
		if (get_zero_flag(registers)) {
			t1 = *(memory_pc_offset + 1);
			registers->pc += t1;
		}
		break;
	case 0xF1:
		/* SBC - Subtract with Carry */
		/* Cycles: 5 (+1 if page crossed) */
		execute_subtract_with_carry(registers,
			get_indirect_y_value(registers));
		registers->pc += 2;
		break;
	case 0xF3:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 8 */
		execute_isb(registers,
			get_indirect_y_pointer(registers));
		registers->pc += 2;
		break;
	case 0xF4:
		/* NOP - No Operation */
		/* Illegal Opcode (Zero Page X) */
		/* Cycles: TODO */
		registers->pc += 2;
		break;
	case 0xF5:
		/* SBC - Subtract with Carry */
		/* Cycles: 4 */
		execute_subtract_with_carry(registers,
			get_zero_page_x_value(registers));
		registers->pc += 2;
		break;
	case 0xF6:
		/* INC - Increment Memory */
		/* Cycles: 6 */
		execute_increment_memory(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0xF7:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 6 */
		execute_isb(registers,
			get_zero_page_x_pointer(registers));
		registers->pc += 2;
		break;
	case 0xF8:
		/* SED - Set Decimal Flag */
		/* Cycles: 2 */
		set_decimal_mode_flag(registers, true);
		registers->pc += 1;
		break;
	case 0xF9:
		/* SBC - Subtract with Carry */
		/* Cycles: 4 (+1 if page crossed) */
		execute_subtract_with_carry(registers,
			get_absolute_y_value(registers));
		registers->pc += 3;
		break;
	case 0xFA:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: TODO */
		registers->pc += 1;
		break;
	case 0xFB:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_isb(registers,
			get_absolute_y_pointer(registers));
		registers->pc += 3;
		break;
	case 0xFC:
		/* NOP - No Operation */
		/* Illegal Opcode (Absolute X) */
		/* Cycles: TODO */
		registers->pc += 3;
		break;
	case 0xFD:
		/* SBC - Subtract with Carry */
		/* Cycles: 4 (+1 if page crossed) */
		execute_subtract_with_carry(registers,
			get_absolute_x_value(registers));
		registers->pc += 3;
		break;
	case 0xFE:
		/* INC - Increment Memory */
		/* Cycles: 7 */
		execute_increment_memory(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	case 0xFF:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 7 */
		execute_isb(registers,
			get_absolute_x_pointer(registers));
		registers->pc += 3;
		break;
	default:
		return EXIT_CODE_UNIMPLEMENTED_BIT;
	}

	return 0;
}
