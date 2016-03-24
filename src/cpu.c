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
#include "memory_bus.h"

#include <stdbool.h>

void init_registers(struct registers *registers)
{
	registers->a = 0;
	registers->x = 0;
	registers->y = 0;
	registers->p = 0x24;
	registers->s = 0xFD;
	registers->pc = 0xC000;
}

/* Utils */

static uint16_t get_stack_address(struct registers *registers)
{
	return 0x0100 + registers->s;
}

static void push_to_stack(struct registers *registers, uint8_t v)
{
	memory_write(get_stack_address(registers), v);
	registers->s -= 1;
}

static uint8_t pop_from_stack(struct registers *registers)
{
	registers->s += 1;
	return memory_read(get_stack_address(registers));
}

static uint8_t get_byte_operand(struct registers *registers)
{
	return memory_read(registers->pc + 1);
}

static uint16_t get_2_byte_operand(struct registers *registers)
{
	uint8_t low_byte = memory_read(registers->pc + 1);
	uint8_t high_byte = memory_read(registers->pc + 2);
	return (high_byte << 8) + low_byte;
}

/* Addressing modes */

static uint16_t computed_address;

static void compute_immediate_address(struct registers *registers)
{
	computed_address = registers->pc + 1;
}

static void compute_zero_page_address(struct registers *registers)
{
	computed_address = get_byte_operand(registers);
}

static void compute_zero_page_x_address(struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(registers);
	zero_page_address += registers->x;
	computed_address = zero_page_address;
}

static void compute_zero_page_y_address(struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(registers);
	zero_page_address += registers->y;
	computed_address = zero_page_address;
}

static void compute_absolute_address(struct registers *registers)
{
	computed_address = get_2_byte_operand(registers);
}

static void compute_absolute_x_address(struct registers *registers)
{
	computed_address = get_2_byte_operand(registers);
	computed_address += registers->x;
}

static void compute_absolute_y_address(struct registers *registers)
{
	computed_address = get_2_byte_operand(registers);
	computed_address += registers->y;
}

static void compute_indirect_x_address(struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(registers);
	zero_page_address += registers->x;

	/* This has zero page wrap around */
	uint8_t address_low = memory_read(zero_page_address);
	zero_page_address += 1;
	uint8_t address_high = memory_read(zero_page_address);

	computed_address = (address_high << 8) + address_low;
}

static void compute_indirect_y_address(struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(registers);

	/* This has zero page wrap around */
	uint8_t address_low = memory_read(zero_page_address);
	zero_page_address += 1;
	uint8_t address_high = memory_read(zero_page_address);

	computed_address = (address_high << 8) + address_low;
	computed_address += registers->y;
}

/* Carry Flag */
static void clear_carry_flag(struct registers *registers)
{ registers->p &= ~(1 << 0); }
static void set_carry_flag(struct registers *registers)
{ registers->p |= 1 << 0; }
static void assign_carry_flag(struct registers *registers, bool c)
{
	if (c) { set_carry_flag(registers); }
	else { clear_carry_flag(registers); }
}
static bool get_carry_flag(struct registers *registers)
{ return registers->p & (1 << 0); }

/* Zero Flag */
static void clear_zero_flag(struct registers *registers)
{ registers->p &= ~(1 << 1); }
static void set_zero_flag(struct registers *registers)
{ registers->p |= 1 << 1; }
static void assign_zero_flag(struct registers *registers, bool z)
{
	if (z) { set_zero_flag(registers); }
	else { clear_zero_flag(registers); }
}
static bool get_zero_flag(struct registers *registers)
{ return registers->p & 1 << 1; }

/* Interrupt Disable Flag */
static void clear_interrupt_disable_flag(struct registers *registers)
{ registers->p &= ~(1 << 2); }
static void set_interrupt_disable_flag(struct registers *registers)
{ registers->p |= 1 << 2; }
static void assign_interrupt_disable_flag(struct registers *registers, bool i)
{
	if (i) { set_interrupt_disable_flag(registers); }
	else { clear_interrupt_disable_flag(registers); }
}
static bool get_interrupt_disable_flag(struct registers *registers)
{ return registers->p & 1 << 2;}

/* Decimal Mode Flag */
static void clear_decimal_mode_flag(struct registers *registers)
{ registers->p &= ~(1 << 3); }
static void set_decimal_mode_flag(struct registers *registers)
{ registers->p |= 1 << 3; }
static void assign_decimal_mode_flag(struct registers *registers, bool d)
{
	if (d) { set_decimal_mode_flag(registers); }
	else { clear_decimal_mode_flag(registers); }
}
static bool get_decimal_mode_flag(struct registers *registers)
{ return registers->p & 1 << 3; }

/* Break Command Flag */
static void clear_break_command_flag(struct registers *registers)
{ registers->p &= ~(1 << 4); }
static void set_break_command_flag(struct registers *registers)
{ registers->p |= 1 << 4; }
static void assign_break_command_flag(struct registers *registers, bool b)
{
	if (b) { set_break_command_flag(registers); }
	else { clear_break_command_flag(registers); }
}
static bool get_break_command_flag(struct registers *registers)
{ return registers->p & 1 << 4; }

/* Unused Flag */
static void clear_unused_flag(struct registers *registers)
{ registers->p &= ~(1 << 5); }
static void set_unused_flag(struct registers *registers)
{ registers->p |= 1 << 5; }
static void assign_unused_flag(struct registers *registers, bool v)
{
	if (v) { set_unused_flag(registers); }
	else { clear_unused_flag(registers); }
}
static bool get_unused_flag(struct registers *registers)
{ return registers->p & 1 << 5; }

/* Overflow Flag */
static void clear_overflow_flag(struct registers *registers)
{ registers->p &= ~(1 << 6); }
static void set_overflow_flag(struct registers *registers)
{ registers->p |= 1 << 6; }
static void assign_overflow_flag(struct registers *registers, bool v)
{
	if (v) { set_overflow_flag(registers); }
	else { clear_overflow_flag(registers); }
}
static bool get_overflow_flag(struct registers *registers)
{ return registers->p & 1 << 6; }

/* Negative Flag */
static void clear_negative_flag(struct registers *registers)
{ registers->p &= ~(1 << 7); }
static void set_negative_flag(struct registers *registers)
{ registers->p |= 1 << 7; }
static void assign_negative_flag(struct registers *registers, bool n)
{
	if (n) { set_negative_flag(registers); }
	else { clear_negative_flag(registers); }
}
static bool get_negative_flag(struct registers *registers)
{ return registers->p & 1 << 7; }

/* Execution */

static void assign_negative_and_zero_flags_from_value(
	struct registers *registers,
	uint8_t m)
{
	assign_zero_flag(registers, m == 0);
	assign_negative_flag(registers, m & (1 << 7));
}

static
void
execute_compare(struct registers *registers, uint8_t r)
{
	uint8_t m = memory_read(computed_address);

	uint16_t result = r - m;
	assign_carry_flag(registers, result < 0x100);
	assign_negative_and_zero_flags_from_value(registers, result);
}

static
void
execute_add_with_carry(struct registers *registers)
{
	uint8_t v = memory_read(computed_address);

	/* TODO: clean-up */
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
		set_overflow_flag(registers);
	}
	else if (a < 0 && b < 0 && (int8_t) result > 0) {
		set_overflow_flag(registers);
	}
	else {
		clear_overflow_flag(registers);
	}

	assign_negative_flag(registers, result & 0x80);
	assign_zero_flag(registers, result == 0);
	registers->a = (result & 0xFF);
}

static
void
execute_subtract_with_carry(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);

	int8_t a = (int8_t) registers->a;
	int8_t b = (int8_t) m;

	int16_t result = a - b;
	uint16_t t = registers->a + ((uint8_t) (~m) + (uint8_t) 1);
	if (!get_carry_flag(registers)) {
		result -= 1;
		t += 0xFF;
	}

	assign_carry_flag(registers, t >= 0x100);
	assign_overflow_flag(registers, result < -128 || result > 127);
	assign_negative_flag(registers, result & 0x80);
	assign_zero_flag(registers, result == 0);
	registers->a = (result & 0xFF);
}

static
void
execute_logical_and(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	registers->a &= m;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static
void
execute_logical_exclusive_or(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	registers->a ^= m;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static
void
execute_logical_inclusive_or(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	registers->a |= m;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static
void
execute_arithmetic_shift_left_accumulator(struct registers *registers)
{
	assign_carry_flag(registers, registers->a & 0x80);
	registers->a <<= 1;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static
void
execute_arithmetic_shift_left(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	assign_carry_flag(registers, m & 0x80);
	m <<= 1;
	assign_negative_and_zero_flags_from_value(registers, m);
	memory_write(computed_address, m);
}

static void execute_logical_shift_right_accumulator(struct registers *registers)
{
	assign_carry_flag(registers, registers->a & 0x01);
	registers->a >>= 1;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static void execute_logical_shift_right(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	assign_carry_flag(registers, m & 0x01);
	m >>= 1;
	assign_negative_and_zero_flags_from_value(registers, m);
	memory_write(computed_address, m);
}

static void execute_rotate_left_accumulator(struct registers *registers)
{
	bool current_carry_flag = get_carry_flag(registers);
	assign_carry_flag(registers, registers->a & 0x80);
	registers->a <<= 1;
	if (current_carry_flag) {
		registers->a |= 0x01;
	}
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static void execute_rotate_left(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	bool current_carry_flag = get_carry_flag(registers);
	assign_carry_flag(registers, m & 0x80);
	m <<= 1;
	if (current_carry_flag) {
		m |= 0x01;
	}
	assign_negative_and_zero_flags_from_value(registers, m);
	memory_write(computed_address, m);
}

static void execute_rotate_right_accumulator(struct registers *registers)
{
	bool current_carry_flag = get_carry_flag(registers);
	assign_carry_flag(registers, registers->a & 0x01);
	registers->a >>= 1;
	if (current_carry_flag) {
		registers->a |= 0x80;
	}
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static void execute_rotate_right(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	bool current_carry_flag = get_carry_flag(registers);
	assign_carry_flag(registers, m & 0x01);
	m >>= 1;
	if (current_carry_flag) {
		m |= 0x80;
	}
	assign_negative_and_zero_flags_from_value(registers, m);
	memory_write(computed_address, m);
}

static
void
execute_decrement_memory(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	m -= 1;
	assign_negative_and_zero_flags_from_value(registers, m);
	memory_write(computed_address, m);
}

static
void
execute_increment_memory(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);
	m += 1;
	assign_negative_and_zero_flags_from_value(registers, m);
	memory_write(computed_address, m);
}

static
void
execute_bit_test(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);

	assign_negative_flag(registers, m & (1 << 7));
	assign_overflow_flag(registers, m & (1 << 6));
	assign_zero_flag(registers, (registers->a & m) == 0);
}

static void execute_branch(struct registers *registers,
                           bool (*get_flag)(struct registers *registers),
                           bool condition)
{
	uint8_t relative = memory_read(registers->pc + 1);
	registers->pc += 2;
	if (get_flag(registers) == condition) {
		/* TODO: Don't depend on cast */
		registers->pc += (int8_t) relative;
	}
}

static void execute_jump_absolute(struct registers *registers)
{
	uint16_t absolute_address = get_2_byte_operand(registers);
	registers->pc = absolute_address;
}

static void execute_jump_indirect(struct registers *registers)
{
	uint8_t indirect_address_low = memory_read(registers->pc + 1);
	uint8_t indirect_address_high = memory_read(registers->pc + 2);
	uint16_t indirect_address = (indirect_address_high << 8)
	                            + indirect_address_low;

	uint8_t absolute_address_low = memory_read(indirect_address);
	/* If the address is 0x02FF, read low byte from 0x02FF
                                and high byte from 0x0200 */
	indirect_address_low += 1;
	indirect_address = (indirect_address_high << 8)
	                   + (indirect_address_low);
	uint8_t absolute_address_high = memory_read(indirect_address);
	uint16_t absolute_address = (absolute_address_high << 8)
	                            + absolute_address_low;

	registers->pc = absolute_address;
}

static void execute_jump_to_subroutine(struct registers *registers)
{
	uint8_t address_low = memory_read(registers->pc + 1);
	uint8_t address_high = memory_read(registers->pc + 2);
	uint16_t target_address = (address_high << 8) + address_low;

	uint16_t return_address = registers->pc + 3;
	/* Hardware subtracts one from the correct return address */
	return_address -= 1;

	uint8_t return_address_high = (return_address & 0xFF00) >> 8;
	uint8_t return_address_low = return_address & 0x00FF;
	push_to_stack(registers, return_address_high);
	push_to_stack(registers, return_address_low);

	registers->pc = target_address;
}

static void execute_return_from_subroutine(struct registers *registers)
{
	uint8_t address_low = pop_from_stack(registers);
	uint8_t address_high = pop_from_stack(registers);;
	uint16_t address = (address_high << 8) + address_low;
	/* Hardware adds one to get the correct return address */
	address += 1;
	registers->pc = address;
}

static void execute_pull_processor_status(struct registers *registers)
{
	bool current_break_command_flag= get_break_command_flag(registers);
	registers->p = pop_from_stack(registers);
	set_unused_flag(registers);
	assign_break_command_flag(registers, current_break_command_flag);
}

static void execute_return_from_interrupt(struct registers *registers)
{
	execute_pull_processor_status(registers);
	uint8_t address_low = pop_from_stack(registers);
	uint8_t address_high = pop_from_stack(registers);;
	uint16_t address = (address_high << 8) + address_low;
	registers->pc = address;
}

static
void
execute_subtract_with_carry_for_isb(struct registers *registers)
{
	uint8_t m = memory_read(computed_address);

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
	assign_overflow_flag(registers, result < -128 || result > 127);
	assign_negative_flag(registers, result & 0x80);
	assign_zero_flag(registers, result == 0);
	registers->a = (result & 0xFF);
}

static
void
execute_isb(struct registers *registers)
{
	execute_increment_memory(registers);
	execute_subtract_with_carry_for_isb(registers);
}

static
void
execute_dcp(struct registers *registers)
{
	execute_decrement_memory(registers);
	execute_compare(registers, registers->a);
}

static
void
execute_slo(struct registers *registers)
{
	execute_arithmetic_shift_left(registers);
	execute_logical_inclusive_or(registers);
}

static
void
execute_rla(struct registers *registers)
{
	execute_rotate_left(registers);
	execute_logical_and(registers);
}

static
void
execute_sre(struct registers *registers)
{
	execute_logical_shift_right(registers);
	execute_logical_exclusive_or(registers);
}

static
void
execute_add_with_carry_rra(struct registers *registers)
{
	uint8_t v = memory_read(computed_address);

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
		set_overflow_flag(registers);
	}
	else if (a < 0 && b < 0 && (int8_t) result > 0) {
		set_overflow_flag(registers);
	}
	else {
		clear_overflow_flag(registers);
	}

	assign_negative_flag(registers, result & 0x80);
	assign_zero_flag(registers, result == 0);
	registers->a = (result & 0xFF);
}

static
void
execute_rra(struct registers *registers)
{
	execute_rotate_right(registers);
	execute_add_with_carry_rra(registers);
}

uint8_t execute_instruction(struct registers *registers)
{
	uint8_t opcode = memory_read(registers->pc);

	/* Processor is little-endian */
	switch (opcode) {
	case 0x01:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		execute_logical_inclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x03:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_x_address(registers);
		execute_slo(registers);
		registers->pc += 2;
		break;
	case 0x04:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		registers->pc += 2;
		break;
	case 0x05:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_logical_inclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x06:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_arithmetic_shift_left(registers);
		registers->pc += 2;
		break;
	case 0x07:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_slo(registers);
		registers->pc += 2;
		break;
	case 0x08:
		/* PHP - Push Processor Status */
		/* Cycles: 3 */
		push_to_stack(registers, registers->p | 0x10);
		registers->pc += 1;
		break;
	case 0x09:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		execute_logical_inclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x0A:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 2 */
		execute_arithmetic_shift_left_accumulator(registers);
		registers->pc += 1;
		break;
	case 0x0C:
		/* TOP */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		registers->pc += 3;
		break;
	case 0x0D:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_logical_inclusive_or(registers);
		registers->pc += 3;
		break;
	case 0x0E:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_arithmetic_shift_left(registers);
		registers->pc += 3;
		break;
	case 0x0F:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_slo(registers);
		registers->pc += 3;
		break;
	case 0x10:
		/* BPL - Branch if Positive */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		execute_branch(registers, get_negative_flag, false);
		break;
	case 0x11:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 5 (+1 if page crossed) */
		compute_indirect_y_address(registers);
		execute_logical_inclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x13:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_y_address(registers);
		execute_slo(registers);
		registers->pc += 2;
		break;
	case 0x14:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		registers->pc += 2;
		break;
	case 0x15:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		execute_logical_inclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x16:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_arithmetic_shift_left(registers);
		registers->pc += 2;
		break;
	case 0x17:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_slo(registers);
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
		compute_absolute_y_address(registers);
		execute_logical_inclusive_or(registers);
		registers->pc += 3;
		break;
	case 0x1A:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: 2 */
		registers->pc += 1;
		break;
	case 0x1B:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_y_address(registers);
		execute_slo(registers);
		registers->pc += 3;
		break;
	case 0x1C:
		/* TOP */
		/* Illegal Opcode */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		registers->pc += 3;
		break;
	case 0x1D:
		/* ORA - Logical Inclusive OR */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		execute_logical_inclusive_or(registers);
		registers->pc += 3;
		break;
	case 0x1E:
		/* ASL - Arithmetic Shift Left */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_arithmetic_shift_left(registers);
		registers->pc += 3;
		break;
	case 0x1F:
		/* SLO */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_slo(registers);
		registers->pc += 3;
		break;
	case 0x20:
		/* JSR - Jump to Subroutine */
		/* Cycles: 6 */
		execute_jump_to_subroutine(registers);
		break;
	case 0x21:
		/* AND - Logical AND */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		execute_logical_and(registers);
		registers->pc += 2;
		break;
	case 0x23:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_x_address(registers);
		execute_rla(registers);
		registers->pc += 2;
		break;
	case 0x24:
		/* BIT - Bit Test */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_bit_test(registers);
		registers->pc += 2;
		break;
	case 0x25:
		/* AND - Logical AND */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_logical_and(registers);
		registers->pc += 2;
		break;
	case 0x26:
		/* ROL - Rotate Left */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_rotate_left(registers);
		registers->pc += 2;
		break;
	case 0x27:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_rla(registers);
		registers->pc += 2;
		break;
	case 0x28:
		/* PLP - Pull Processor Status */
		/* Cycles: 4 */
		execute_pull_processor_status(registers);
		registers->pc += 1;
		break;
	case 0x29:
		/* AND - Logical AND */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		execute_logical_and(registers);
		registers->pc += 2;
		break;
	case 0x2A:
		/* ROL - Rotate Left */
		/* Cycles: 2 */
		execute_rotate_left_accumulator(registers);
		registers->pc += 1;
		break;
	case 0x2C:
		/* BIT - Bit Test */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_bit_test(registers);
		registers->pc += 3;
		break;
	case 0x2D:
		/* AND - Logical AND */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_logical_and(registers);
		registers->pc += 3;
		break;
	case 0x2E:
		/* ROL - Rotate Left */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_rotate_left(registers);
		registers->pc += 3;
		break;
	case 0x2F:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_rla(registers);
		registers->pc += 3;
		break;
	case 0x30:
		/* BMI - Branch if Minus */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		execute_branch(registers, get_negative_flag, true);
		break;
	case 0x31:
		/* AND - Logical AND */
		/* Cycles: 5 (+1 if page crossed) */
		compute_indirect_y_address(registers);
		execute_logical_and(registers);
		registers->pc += 2;
		break;
	case 0x33:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_y_address(registers);
		execute_rla(registers);
		registers->pc += 2;
		break;
	case 0x34:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		registers->pc += 2;
		break;
	case 0x35:
		/* AND - Logical AND */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		execute_logical_and(registers);
		registers->pc += 2;
		break;
	case 0x36:
		/* ROL - Rotate Left */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_rotate_left(registers);
		registers->pc += 2;
		break;
	case 0x37:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_rla(registers);
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
		compute_absolute_y_address(registers);
		execute_logical_and(registers);
		registers->pc += 3;
		break;
	case 0x3A:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: 2 */
		registers->pc += 1;
		break;
	case 0x3B:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_y_address(registers);
		execute_rla(registers);
		registers->pc += 3;
		break;
	case 0x3C:
		/* TOP */
		/* Illegal Opcode */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		registers->pc += 3;
		break;
	case 0x3D:
		/* AND - Logical AND */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		execute_logical_and(registers);
		registers->pc += 3;
		break;
	case 0x3E:
		/* ROL - Rotate Left */
		/* Cycles: 7 */
		// get_absolute_x_pointer(registers));
		execute_rotate_left(registers);
		registers->pc += 3;
		break;
	case 0x3F:
		/* RLA */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_rla(registers);
		registers->pc += 3;
		break;
	case 0x40:
		/* RTI - Return from Interrupt */
		/* Cycles: 6 */
		execute_return_from_interrupt(registers);
		break;
	case 0x41:
		/* EOR - Exclusive OR */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		execute_logical_exclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x43:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_x_address(registers);
		execute_sre(registers);
		registers->pc += 2;
		break;
	case 0x44:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		registers->pc += 2;
		break;
	case 0x45:
		/* EOR - Exclusive OR */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_logical_exclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x46:
		/* LSR - Logical Shift Right */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_logical_shift_right(registers);
		registers->pc += 2;
		break;
	case 0x47:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_sre(registers);
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
		compute_immediate_address(registers);
		execute_logical_exclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x4A:
		/* LSR - Logical Shift Right */
		/* Cycles: 2 */
		execute_logical_shift_right_accumulator(registers);
		registers->pc += 1;
		break;
	case 0x4C:
		/* JMP - Jump */
		/* Cycles: 3 */
		execute_jump_absolute(registers);
		break;
	case 0x4D:
		/* EOR - Exclusive OR */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_logical_exclusive_or(registers);
		registers->pc += 3;
		break;
	case 0x4E:
		/* LSR - Logical Shift Right */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_logical_shift_right(registers);
		registers->pc += 3;
		break;
	case 0x4F:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_sre(registers);
		registers->pc += 3;
		break;
	case 0x50:
		/* BVC - Branch if Overflow Clear */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		execute_branch(registers, get_overflow_flag, false);
		break;
	case 0x51:
		/* EOR - Exclusive OR */
		/* Cycles: 5 (+1 if page crossed) */
		compute_indirect_y_address(registers);
		execute_logical_exclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x53:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_y_address(registers);
		execute_sre(registers);
		registers->pc += 2;
		break;
	case 0x54:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		registers->pc += 2;
		break;
	case 0x55:
		/* EOR - Exclusive OR */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		execute_logical_exclusive_or(registers);
		registers->pc += 2;
		break;
	case 0x56:
		/* LSR - Logical Shift Right */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_logical_shift_right(registers);
		registers->pc += 2;
		break;
	case 0x57:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_sre(registers);
		registers->pc += 2;
		break;
	case 0x59:
		/* EOR - Exclusive OR */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_y_address(registers);
		execute_logical_exclusive_or(registers);
		registers->pc += 3;
		break;
	case 0x5A:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: 2 */
		registers->pc += 1;
		break;
	case 0x5B:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_sre(registers);
		registers->pc += 3;
		break;
	case 0x5C:
		/* TOP */
		/* Illegal Opcode */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		registers->pc += 3;
		break;
	case 0x5D:
		/* EOR - Exclusive OR */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		execute_logical_exclusive_or(registers);
		registers->pc += 3;
		break;
	case 0x5E:
		/* LSR - Logical Shift Right */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_logical_shift_right(registers);
		registers->pc += 3;
		break;
	case 0x5F:
		/* SRE */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_sre(registers);
		registers->pc += 3;
		break;
	case 0x60:
		/* RTS - Return from Subroutine */
		/* Cycles: 6 */
		execute_return_from_subroutine(registers);
		break;
	case 0x61:
		/* ADC - Add with Carry */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		compute_indirect_x_address(registers);
		execute_add_with_carry(registers);
		registers->pc += 2;
		break;
	case 0x63:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_x_address(registers);
		execute_rra(registers);
		registers->pc += 2;
		break;
	case 0x64:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		registers->pc += 2;
		break;
	case 0x65:
		/* ADC - Add with Carry */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_add_with_carry(registers);
		registers->pc += 2;
		break;
	case 0x66:
		/* ROR - Rotate Right */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_rotate_right(registers);
		registers->pc += 2;
		break;
	case 0x67:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_rra(registers);
		registers->pc += 2;
		break;
	case 0x68:
		/* PLA - Pull Accumulator */
		/* Cycles: 4 */
		registers->a = pop_from_stack(registers);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 1;
		break;
	case 0x69:
		/* ADC - Add with Carry */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		execute_add_with_carry(registers);
		registers->pc += 2;
		break;
	case 0x6A:
		/* ROR - Rotate Right */
		/* Cycles: 2 */
		execute_rotate_right_accumulator(registers);
		registers->pc += 1;
		break;
	case 0x6C:
		/* JMP - Jump */
		/* Cycles: 5 */
		execute_jump_indirect(registers);
		break;
	case 0x6D:
		/* ADC - Add with Carry */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_add_with_carry(registers);
		registers->pc += 3;
		break;
	case 0x6E:
		/* ROR - Rotate Right */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_rotate_right(registers);
		registers->pc += 3;
		break;
	case 0x6F:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_rra(registers);
		registers->pc += 3;
		break;
	case 0x70:
		/* BVS - Branch if Overflow Set */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		execute_branch(registers, get_overflow_flag, true);
		break;
	case 0x71:
		/* ADC - Add with Carry */
		/* Cycles: 5 (+1 if page crossed) */
		compute_indirect_y_address(registers);
		execute_add_with_carry(registers);
		registers->pc += 2;
		break;
	case 0x73:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_y_address(registers);
		execute_rra(registers);
		registers->pc += 2;
		break;
	case 0x74:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		registers->pc += 2;
		break;
	case 0x75:
		/* ADC - Add with Carry */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		execute_add_with_carry(registers);
		registers->pc += 2;
		break;
	case 0x76:
		/* ROR - Rotate Right */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_rotate_right(registers);
		registers->pc += 2;
		break;
	case 0x77:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_rra(registers);
		registers->pc += 2;
		break;
	case 0x78:
		/* SEI - Set Interrupt Disable */
		/* Cycles: 2 */
		set_interrupt_disable_flag(registers);
		registers->pc += 1;
		break;
	case 0x79:
		/* ADC - Add with Carry */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_y_address(registers);
		execute_add_with_carry(registers);
		registers->pc += 3;
		break;
	case 0x7A:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: 2 */
		registers->pc += 1;
		break;
	case 0x7B:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_y_address(registers);
		execute_rra(registers);
		registers->pc += 3;
		break;
	case 0x7C:
		/* TOP */
		/* Illegal Opcode */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		registers->pc += 3;
		break;
	case 0x7D:
		/* ADC - Add with Carry */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		execute_add_with_carry(registers);
		registers->pc += 3;
		break;
	case 0x7E:
		/* ROR - Rotate Right */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_rotate_right(registers);
		registers->pc += 3;
		break;
	case 0x7F:
		/* RRA */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_rra(registers);
		registers->pc += 3;
		break;
	case 0x80:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		registers->pc += 2;
		break;
	case 0x81:
		/* STA - Store Accumulator */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		memory_write(computed_address, registers->a);
		registers->pc += 2;
		break;
	case 0x82:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		registers->pc += 2;
		break;
	case 0x83:
		/* SAX */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		memory_write(computed_address, registers->a & registers->x);
		registers->pc += 2;
		break;
	case 0x84:
		/* STY - Store Y Register */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		memory_write(computed_address, registers->y);
		registers->pc += 2;
		break;
	case 0x85:
		/* STA - Store Accumulator */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		memory_write(computed_address, registers->a);
		registers->pc += 2;
		break;
	case 0x86:
		/* STX - Store X Register */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		memory_write(computed_address, registers->x);
		registers->pc += 2;
		break;
	case 0x87:
		/* SAX */
		/* Illegal Opcode */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		memory_write(computed_address, registers->a & registers->x);
		registers->pc += 2;
		break;
	case 0x88:
		/* DEY - Decrement Y Register */
		/* Cycles: 2 */
		registers->y -= 1;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 1;
		break;
	case 0x89:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		registers->pc += 2;
		break;
	case 0x8A:
		/* TXA - Transfer X to Accumulator */
		/* Cycles: 2 */
		registers->a = registers->x;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 1;
		break;
	case 0x8C:
		/* STY - Store Y Register */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		memory_write(computed_address, registers->y);
		registers->pc += 3;
		break;
	case 0x8D:
		/* STA - Store Accumulator */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		memory_write(computed_address, registers->a);
		registers->pc += 3;
		break;
	case 0x8E:
		/* STX - Store X Register */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		memory_write(computed_address, registers->x);
		registers->pc += 3;
		break;
	case 0x8F:
		/* SAX */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		memory_write(computed_address, registers->a & registers->x);
		registers->pc += 3;
		break;
	case 0x90:
		/* BCC - Branch if Carry Clear */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		execute_branch(registers, get_carry_flag, false);
		break;
	case 0x91:
		/* STA - Store Accumulator */
		/* Cycles: 6 */
		compute_indirect_y_address(registers);
		memory_write(computed_address, registers->a);
		registers->pc += 2;
		break;
	case 0x94:
		/* STY - Store Y Register */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		memory_write(computed_address, registers->y);
		registers->pc += 2;
		break;
	case 0x95:
		/* STA - Store Accumulator */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		memory_write(computed_address, registers->a);
		registers->pc += 2;
		break;
	case 0x96:
		/* STX - Store X Register */
		/* Cycles: 4 */
		compute_zero_page_y_address(registers);
		memory_write(computed_address, registers->x);
		registers->pc += 2;
		break;
	case 0x97:
		/* SAX */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_zero_page_y_address(registers);
		memory_write(computed_address, registers->a & registers->x);
		registers->pc += 2;
		break;
	case 0x98:
		/* TYA - Transfer Y to Accumulator */
		/* Cycles: 2 */
		registers->a = registers->y;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 1;
		break;
	case 0x99:
		/* STA - Store Accumulator */
		/* Cycles: 5 */
		compute_absolute_y_address(registers);
		memory_write(computed_address, registers->a);
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
		compute_absolute_x_address(registers);
		memory_write(computed_address, registers->a);
		registers->pc += 3;
		break;
	case 0xA0:
		/* LDY - Load Y Register */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		registers->y = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 2;
		break;
	case 0xA1:
		/* LDA - Load Accumlator */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		registers->a = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xA2:
		/* LDX - Load X Register */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 2;
		break;
	case 0xA3:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		registers->a = memory_read(computed_address);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xA4:
		/* LDY - Load Y Register */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		registers->y = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 2;
		break;
	case 0xA5:
		/* LDA - Load Accumlator */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		registers->a = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xA6:
		/* LDX - Load X Register */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 2;
		break;
	case 0xA7:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		registers->a = memory_read(computed_address);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xA8:
		/* TAY - Transfer Accumulator to Y */
		/* Cycles: 2 */
		registers->y = registers->a;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 1;
		break;
	case 0xA9:
		/* LDA - Load Accumlator */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		registers->a = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xAA:
		/* TAY - Transfer Accumulator to X */
		/* Cycles: 2 */
		registers->x = registers->a;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 1;
		break;
	case 0xAC:
		/* LDY - Load Y Register */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		registers->y = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 3;
		break;
	case 0xAD:
		/* LDA - Load Acuumulator */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		registers->a = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		break;
	case 0xAE:
		/* LDX - Load X Register */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 3;
		break;
	case 0xAF:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		registers->a = memory_read(computed_address);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		break;
	case 0xB0:
		/* BCS - Branch if Carry Set */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		execute_branch(registers, get_carry_flag, true);
		break;
	case 0xB1:
		/* LDA - Load Accumlator */
		/* Cycles: 5 (+1 if page crossed) */
		compute_indirect_y_address(registers);
		registers->a = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xB3:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 5 (+1 if page crossed) */
		compute_indirect_y_address(registers);
		registers->a = memory_read(computed_address);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xB4:
		/* LDY - Load Y Register */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		registers->y = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 2;
		break;
	case 0xB5:
		/* LDA - Load Accumlator */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		registers->a = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xB6:
		/* LDX - Load X Register */
		/* Cycles: 4 */
		compute_zero_page_y_address(registers);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 2;
		break;
	case 0xB7:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_zero_page_y_address(registers);
		registers->a = memory_read(computed_address);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		break;
	case 0xB8:
		/* CLV - Clear Overflow Flag */
		/* Cycles: 2 */
		clear_overflow_flag(registers);
		registers->pc += 1;
		break;
	case 0xB9:
		/* LDA - Load Acuumulator */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_y_address(registers);
		registers->a = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		break;
	case 0xBA:
		/* TSX - Transfer Stack Pointer to X */
		/* Cycles: 2 */
		registers->x = registers->s;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 1;
		break;
	case 0xBC:
		/* LDY - Load Y Register */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		registers->y = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 3;
		break;
	case 0xBD:
		/* LDA - Load Acuumulator */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		registers->a = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		break;
	case 0xBE:
		/* LDX - Load X Register */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_y_address(registers);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 3;
		break;
	case 0xBF:
		/* LAX - Load Accumulator and X Register */
		/* Illegal Opcode */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_y_address(registers);
		registers->a = memory_read(computed_address);
		registers->x = memory_read(computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		break;
	case 0xC0:
		/* CPY - Compare Y Register */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		execute_compare(registers, registers->y);
		registers->pc += 2;
		break;
	case 0xC1:
		/* CMP - Compare */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		execute_compare(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xC2:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		registers->pc += 2;
		break;
	case 0xC3:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_x_address(registers);
		execute_dcp(registers);
		registers->pc += 2;
		break;
	case 0xC4:
		/* CPY - Compare Y Register */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_compare(registers, registers->y);
		registers->pc += 2;
		break;
	case 0xC5:
		/* CMP - Compare */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_compare(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xC6:
		/* DEC - Decrement Memory */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_decrement_memory(registers);
		registers->pc += 2;
		break;
	case 0xC7:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_dcp(registers);
		registers->pc += 2;
		break;
	case 0xC8:
		/* INY - Increment Y Register */
		/* Cycles: 2 */
		registers->y += 1;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 1;
		break;
	case 0xC9:
		/* CMP - Compare */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		execute_compare(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xCA:
		/* DEX - Decrement X Register */
		/* Cycles: 2 */
		registers->x -= 1;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 1;
		break;
	case 0xCC:
		/* CPY - Compare Y Register */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_compare(registers, registers->y);
		registers->pc += 3;
		break;
	case 0xCD:
		/* CMP - Compare */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_compare(registers, registers->a);
		registers->pc += 3;
		break;
	case 0xCE:
		/* DEC - Decrement Memory */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_decrement_memory(registers);
		registers->pc += 3;
		break;
	case 0xCF:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_dcp(registers);
		registers->pc += 3;
		break;
	case 0xD0:
		/* BNE - Branch if Not Equal */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		execute_branch(registers, get_zero_flag, false);
		break;
	case 0xD1:
		/* CMP - Compare */
		/* Cycles: 5 (+1 if page crossed) */
		compute_indirect_y_address(registers);
		execute_compare(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xD3:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_y_address(registers);
		execute_dcp(registers);
		registers->pc += 2;
		break;
	case 0xD4:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		registers->pc += 2;
		break;
	case 0xD5:
		/* CMP - Compare */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		execute_compare(registers, registers->a);
		registers->pc += 2;
		break;
	case 0xD6:
		/* DEC - Decrement Memory */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_decrement_memory(registers);
		registers->pc += 2;
		break;
	case 0xD7:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_dcp(registers);
		registers->pc += 2;
		break;
	case 0xD8:
		/* CLD - Clear Decimal Mode */
		/* Cycles: 2 */
		clear_decimal_mode_flag(registers);
		registers->pc += 1;
		break;
	case 0xD9:
		/* CMP - Compare */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_y_address(registers);
		execute_compare(registers, registers->a);
		registers->pc += 3;
		break;
	case 0xDA:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: 2 */
		registers->pc += 1;
		break;
	case 0xDB:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_y_address(registers);
		execute_dcp(registers);
		registers->pc += 3;
		break;
	case 0xDC:
		/* TOP */
		/* Illegal Opcode */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		registers->pc += 3;
		break;
	case 0xDD:
		/* CMP - Compare */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		execute_compare(registers, registers->a);
		registers->pc += 3;
		break;
	case 0xDE:
		/* DEC - Decrement Memory */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_decrement_memory(registers);
		registers->pc += 3;
		break;
	case 0xDF:
		/* DCP */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_dcp(registers);
		registers->pc += 3;
		break;
	case 0xE0:
		/* CPX - Compare X Register */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		execute_compare(registers, registers->x);
		registers->pc += 2;
		break;
	case 0xE1:
		/* SBC - Subtract with Carry */
		/* Cycles: 6 */
		compute_indirect_x_address(registers);
		execute_subtract_with_carry(registers);
		registers->pc += 2;
		break;
	case 0xE2:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		registers->pc += 2;
		break;
	case 0xE3:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_x_address(registers);
		execute_isb(registers);
		registers->pc += 2;
		break;
	case 0xE4:
		/* CPX - Compare X Register */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_compare(registers, registers->x);
		registers->pc += 2;
		break;
	case 0xE5:
		/* SBC - Subtract with Carry */
		/* Cycles: 3 */
		compute_zero_page_address(registers);
		execute_subtract_with_carry(registers);
		registers->pc += 2;
		break;
	case 0xE6:
		/* INC - Increment Memory */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_increment_memory(registers);
		registers->pc += 2;
		break;
	case 0xE7:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 5 */
		compute_zero_page_address(registers);
		execute_isb(registers);
		registers->pc += 2;
		break;
	case 0xE8:
		/* INX - Increment X Register */
		/* Cycles: 2 */
		registers->x += 1;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 1;
		break;
	case 0xE9:
		/* SBC - Subtract with Carry */
		/* Cycles: 2 */
		compute_immediate_address(registers);
		execute_subtract_with_carry(registers);
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
		compute_immediate_address(registers);
		execute_subtract_with_carry(registers);
		registers->pc += 2;
		break;
	case 0xEC:
		/* CPX - Compare X Register */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_compare(registers, registers->x);
		registers->pc += 3;
		break;
	case 0xED:
		/* SBC - Subtract with Carry */
		/* Cycles: 4 */
		compute_absolute_address(registers);
		execute_subtract_with_carry(registers);
		registers->pc += 3;
		break;
	case 0xEE:
		/* INC - Increment Memory */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_increment_memory(registers);
		registers->pc += 3;
		break;
	case 0xEF:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_absolute_address(registers);
		execute_isb(registers);
		registers->pc += 3;
		break;
	case 0xF0:
		/* BEQ - Branch if Equal */
		/* Cycles: 2 (+1 if branch succeeds, +2 if to a new page) */
		execute_branch(registers, get_zero_flag, true);
		break;
	case 0xF1:
		/* SBC - Subtract with Carry */
		/* Cycles: 5 (+1 if page crossed) */
		compute_indirect_y_address(registers);
		execute_subtract_with_carry(registers);
		registers->pc += 2;
		break;
	case 0xF3:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 8 */
		compute_indirect_y_address(registers);
		execute_isb(registers);
		registers->pc += 2;
		break;
	case 0xF4:
		/* DOP */
		/* Illegal Opcode */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		registers->pc += 2;
		break;
	case 0xF5:
		/* SBC - Subtract with Carry */
		/* Cycles: 4 */
		compute_zero_page_x_address(registers);
		execute_subtract_with_carry(registers);
		registers->pc += 2;
		break;
	case 0xF6:
		/* INC - Increment Memory */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_increment_memory(registers);
		registers->pc += 2;
		break;
	case 0xF7:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 6 */
		compute_zero_page_x_address(registers);
		execute_isb(registers);
		registers->pc += 2;
		break;
	case 0xF8:
		/* SED - Set Decimal Flag */
		/* Cycles: 2 */
		set_decimal_mode_flag(registers);
		registers->pc += 1;
		break;
	case 0xF9:
		/* SBC - Subtract with Carry */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_y_address(registers);
		execute_subtract_with_carry(registers);
		registers->pc += 3;
		break;
	case 0xFA:
		/* NOP - No Operation */
		/* Illegal Opcode */
		/* Cycles: 2 */
		registers->pc += 1;
		break;
	case 0xFB:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_y_address(registers);
		execute_isb(registers);
		registers->pc += 3;
		break;
	case 0xFC:
		/* TOP */
		/* Illegal Opcode */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		registers->pc += 3;
		break;
	case 0xFD:
		/* SBC - Subtract with Carry */
		/* Cycles: 4 (+1 if page crossed) */
		compute_absolute_x_address(registers);
		execute_subtract_with_carry(registers);
		registers->pc += 3;
		break;
	case 0xFE:
		/* INC - Increment Memory */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_increment_memory(registers);
		registers->pc += 3;
		break;
	case 0xFF:
		/* ISB */
		/* Illegal Opcode */
		/* Cycles: 7 */
		compute_absolute_x_address(registers);
		execute_isb(registers);
		registers->pc += 3;
		break;
	default:
		return EXIT_CODE_UNIMPLEMENTED_BIT;
	}

	return 0;
}
