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

#include "cpu.h"

#include <stdbool.h>
#include "apu.h"
#include "cartridge.h"
#include "console.h"
#include "exit_code.h"
#include "ppu.h"

static const uint16_t NMI_HANDLER_ADDRESS = 0xFFFA;
static const uint16_t RESET_HANDLER_ADDRESS = 0xFFFC;
static const uint16_t IRQ_HANDLER_ADDRESS = 0xFFFE;

static uint8_t get_controller_status(struct nes_emulator_console *console)
{
	if (console->cpu.controller_shift > 7) {
		/* Overflowed the shifts */
		return 0x01;
	}

	uint8_t ret = 0x00;
	if ((console->cpu.controller_status << console->cpu.controller_shift)
	    & 0x80) {
		ret = 0x01;
	}

	if (console->cpu.controller_latch == false) {
		++(console->cpu.controller_shift);
	}

	return ret;
}

static uint8_t cpu_bus_read(struct nes_emulator_console *console,
                            uint16_t address)
{
	if (address < 0x0800) {
		return console->cpu.ram[address];
	}
	else if (address < 0x1000) {
		return cpu_bus_read(console, address - 0x0800);
	}
	else if (address < 0x1800) {
		return cpu_bus_read(console, address - 0x1000);
	}
	else if (address < 0x2000) {
		return cpu_bus_read(console, address - 0x1800);
	}
	else if (address < 0x4000) {
		return ppu_cpu_bus_read(console, address);
	}
	else if (address < 0x4020) {
		if (address == 0x4014) {
			return 0x00;
		}
		else if (address == 0x4016) {
			return get_controller_status(console);
		}
		else {
			return apu_cpu_bus_read(console, address);
		}
	}
	else {
		return cartridge_cpu_bus_read(console, address);
	}
}

static void oam_dma(struct nes_emulator_console *console,
                    uint8_t value)
{
	/* TODO: should suspend for 513 cycles? */
	console->cpu.dma_suspend_cycles = 513;
	uint16_t cpu_address = value << 8;
	for (uint8_t offset = 0; ; ++offset) {
		uint8_t cpu_value = cpu_bus_read(console, cpu_address + offset);
		ppu_cpu_bus_write(console, 0x2004, cpu_value);
		if (offset == 0xFF) {
			break;
		}
	}
}

static void cpu_bus_write(struct nes_emulator_console *console,
                          uint16_t address,
                          uint8_t value)
{
	if (address < 0x0800) {
		console->cpu.ram[address] = value;
	}
	else if (address < 0x1000) {
		cpu_bus_write(console, address - 0x0800, value);
	}
	else if (address < 0x1800) {
		cpu_bus_write(console, address - 0x1000, value);
	}
	else if (address < 0x2000) {
		cpu_bus_write(console, address - 0x1800, value);
	}
	else if (address < 0x4000) {
		ppu_cpu_bus_write(console, address, value);
	}
	else if (address < 0x4020) {
		if (address == 0x4014) {
			oam_dma(console, value);
		}
		else if (address == 0x4016) {
			if (console->cpu.controller_latch && ((value & 0x01)
			    == 0)) {
				console->cpu.controller_shift = 0;
				console->cpu.controller_status =
					controller_read(console);
			}
			console->cpu.controller_latch = value & 0x01;
		}
		else {
			apu_cpu_bus_write(console, address, value);
		}
	}
	else {
		cartridge_cpu_bus_write(console, address, value);
	}
}

static void init_registers(struct registers *registers)
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

static void push_to_stack(struct nes_emulator_console *console,
                          struct registers *registers,
                          uint8_t value)
{
	cpu_bus_write(console, get_stack_address(registers), value);
	registers->s -= 1;
}

static uint8_t pop_from_stack(struct nes_emulator_console *console,
                              struct registers *registers)
{
	registers->s += 1;
	return cpu_bus_read(console, get_stack_address(registers));
}

static uint8_t get_byte_operand(struct nes_emulator_console *console,
                                struct registers *registers)
{
	return cpu_bus_read(console, registers->pc + 1);
}

static uint16_t get_2_byte_operand(struct nes_emulator_console *console,
                                   struct registers *registers)
{
	uint8_t low_byte = cpu_bus_read(console, registers->pc + 1);
	uint8_t high_byte = cpu_bus_read(console, registers->pc + 2);
	return (high_byte << 8) + low_byte;
}

static bool is_page_crossed(uint16_t a1, uint16_t a2)
{
	return (a1 & 0xFF00) != (a2 & 0xFF00);
}

/* Addressing modes */
static void compute_immediate_address(struct nes_emulator_console *console,
                                      struct registers *registers)
{
	console->cpu.computed_address = registers->pc + 1;
}

static void compute_zero_page_address(struct nes_emulator_console *console,
                                      struct registers *registers)
{
	console->cpu.computed_address = get_byte_operand(console, registers);
}

static void compute_zero_page_x_address(struct nes_emulator_console *console,
                                        struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(console, registers);
	zero_page_address += registers->x;
	console->cpu.computed_address = zero_page_address;
}

static void compute_zero_page_y_address(struct nes_emulator_console *console,
                                        struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(console, registers);
	zero_page_address += registers->y;
	console->cpu.computed_address = zero_page_address;
}

static void compute_absolute_address(struct nes_emulator_console *console,
                                     struct registers *registers)
{
	console->cpu.computed_address = get_2_byte_operand(console, registers);
}

static void compute_absolute_x_address(struct nes_emulator_console *console,
                                       struct registers *registers)
{
	console->cpu.computed_address = get_2_byte_operand(console, registers);
	console->cpu.computed_address += registers->x;
}

static void compute_absolute_y_address(struct nes_emulator_console *console,
                                       struct registers *registers)
{
	console->cpu.computed_address = get_2_byte_operand(console, registers);
	console->cpu.computed_address += registers->y;
}

static void compute_indirect_x_address(struct nes_emulator_console *console,
                                       struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(console, registers);
	zero_page_address += registers->x;

	/* This has zero page wrap around */
	uint8_t address_low = cpu_bus_read(console, zero_page_address);
	zero_page_address += 1;
	uint8_t address_high = cpu_bus_read(console, zero_page_address);

	console->cpu.computed_address = (address_high << 8) + address_low;
}

static void compute_indirect_y_address(struct nes_emulator_console *console,
                                       struct registers *registers)
{
	uint8_t zero_page_address = get_byte_operand(console, registers);

	/* This has zero page wrap around */
	uint8_t address_low = cpu_bus_read(console, zero_page_address);
	zero_page_address += 1;
	uint8_t address_high = cpu_bus_read(console, zero_page_address);

	console->cpu.computed_address = (address_high << 8) + address_low;
	console->cpu.computed_address += registers->y;
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
static void assign_unused_flag(struct registers *registers, bool u)
{
	if (u) { set_unused_flag(registers); }
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

static void assign_negative_and_zero_flags_from_value(
	struct registers *registers,
	uint8_t m)
{
	assign_zero_flag(registers, m == 0);
	assign_negative_flag(registers, m & (1 << 7));
}

/* Execution */

static void execute_compare(struct nes_emulator_console *console,
                            struct registers *registers,
                            uint8_t r)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);

	uint16_t result = r - m;
	assign_carry_flag(registers, result < 0x100);
	assign_negative_and_zero_flags_from_value(registers, result);
}

static void execute_add_with_carry(struct nes_emulator_console *console,
                                   struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);

	uint16_t result = registers->a + m;
	if (get_carry_flag(registers) == true) {
		result += 1;
	}

	bool a_is_negative = registers->a & 0x80;
	bool m_is_negative = m & 0x80;
	bool result_is_negative = result & 0x80;
	if ((a_is_negative && m_is_negative && !result_is_negative)
	    || (!a_is_negative && !m_is_negative && result_is_negative)) {
		set_overflow_flag(registers);
	}
	else {
		clear_overflow_flag(registers);
	}

	assign_carry_flag(registers, result & 0x100);

	uint8_t byte_result = result;
	assign_negative_and_zero_flags_from_value(registers, byte_result);
	registers->a = byte_result;
}

static void execute_subtract_with_carry(struct nes_emulator_console *console,
                                        struct registers *registers)
{
	uint8_t a = registers->a;
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);

	uint16_t result = a - m;
	if (!get_carry_flag(registers)) {
		result -= 1;
	}

	assign_overflow_flag(registers, (a ^ result) & (a ^ m) & 0x80);
	assign_carry_flag(registers, !(result & 0x100));

	uint8_t byte_result = result;
	assign_negative_and_zero_flags_from_value(registers, byte_result);
	registers->a = byte_result;
}

static void execute_logical_and(struct nes_emulator_console *console,
                                struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	registers->a &= m;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static void execute_logical_exclusive_or(struct nes_emulator_console *console,
                                         struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	registers->a ^= m;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static void execute_logical_inclusive_or(struct nes_emulator_console *console,
                                         struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	registers->a |= m;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static void execute_arithmetic_shift_left_accumulator(
	struct registers *registers)
{
	assign_carry_flag(registers, registers->a & 0x80);
	registers->a <<= 1;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static void execute_arithmetic_shift_left(struct nes_emulator_console *console,
                                          struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	assign_carry_flag(registers, m & 0x80);
	m <<= 1;
	assign_negative_and_zero_flags_from_value(registers, m);
	cpu_bus_write(console, console->cpu.computed_address, m);
}

static void execute_logical_shift_right_accumulator(struct registers *registers)
{
	assign_carry_flag(registers, registers->a & 0x01);
	registers->a >>= 1;
	assign_negative_and_zero_flags_from_value(registers, registers->a);
}

static void execute_logical_shift_right(struct nes_emulator_console *console,
                                        struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	assign_carry_flag(registers, m & 0x01);
	m >>= 1;
	assign_negative_and_zero_flags_from_value(registers, m);
	cpu_bus_write(console, console->cpu.computed_address, m);
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

static void execute_rotate_left(struct nes_emulator_console *console,
                                struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	bool current_carry_flag = get_carry_flag(registers);
	assign_carry_flag(registers, m & 0x80);
	m <<= 1;
	if (current_carry_flag) {
		m |= 0x01;
	}
	assign_negative_and_zero_flags_from_value(registers, m);
	cpu_bus_write(console, console->cpu.computed_address, m);
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

static void execute_rotate_right(struct nes_emulator_console *console,
                                 struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	bool current_carry_flag = get_carry_flag(registers);
	assign_carry_flag(registers, m & 0x01);
	m >>= 1;
	if (current_carry_flag) {
		m |= 0x80;
	}
	assign_negative_and_zero_flags_from_value(registers, m);
	cpu_bus_write(console, console->cpu.computed_address, m);
}

static void execute_decrement_memory(struct nes_emulator_console *console,
                                     struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	m -= 1;
	assign_negative_and_zero_flags_from_value(registers, m);
	cpu_bus_write(console, console->cpu.computed_address, m);
}

static void execute_increment_memory(struct nes_emulator_console *console,
                                     struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);
	m += 1;
	assign_negative_and_zero_flags_from_value(registers, m);
	cpu_bus_write(console, console->cpu.computed_address, m);
}

static void execute_bit_test(struct nes_emulator_console *console,
                             struct registers *registers)
{
	uint8_t m = cpu_bus_read(console, console->cpu.computed_address);

	assign_negative_flag(registers, m & (1 << 7));
	assign_overflow_flag(registers, m & (1 << 6));
	assign_zero_flag(registers, (registers->a & m) == 0);
}

static void execute_branch(struct nes_emulator_console *console,
                           struct registers *registers,
                           bool (*get_flag)(struct registers *registers),
                           bool condition,
                           uint16_t *step_cycles)
{
	uint8_t relative = cpu_bus_read(console, registers->pc + 1);
	registers->pc += 2;
	*step_cycles = 2;
	if (get_flag(registers) == condition) {
		*step_cycles += 1;
		uint8_t next_page = ((registers->pc) >> 8);
		/* TODO: Don't depend on cast */
		registers->pc += (int8_t) relative;
		uint8_t dest_page = ((registers->pc) >> 8);
		if (next_page != dest_page) {
			*step_cycles += 1;
		}
	}
}

static void execute_jump_absolute(struct nes_emulator_console *console,
                                  struct registers *registers)
{
	uint16_t absolute_address = get_2_byte_operand(console, registers);
	registers->pc = absolute_address;
}

static void execute_jump_indirect(struct nes_emulator_console *console,
                                  struct registers *registers)
{
	uint8_t indirect_address_low = cpu_bus_read(console, registers->pc + 1);
	uint8_t indirect_address_high = cpu_bus_read(console,
	                                             registers->pc + 2);
	uint16_t indirect_address = (indirect_address_high << 8)
	                            + indirect_address_low;

	uint8_t absolute_address_low = cpu_bus_read(console, indirect_address);
	/* If the address is 0x02FF, read low byte from 0x02FF
                                and high byte from 0x0200 */
	indirect_address_low += 1;
	indirect_address = (indirect_address_high << 8)
	                   + (indirect_address_low);
	uint8_t absolute_address_high = cpu_bus_read(console, indirect_address);
	uint16_t absolute_address = (absolute_address_high << 8)
	                            + absolute_address_low;

	registers->pc = absolute_address;
}

static void execute_jump_to_subroutine(struct nes_emulator_console *console,
                                       struct registers *registers)
{
	uint8_t address_low = cpu_bus_read(console, registers->pc + 1);
	uint8_t address_high = cpu_bus_read(console, registers->pc + 2);
	uint16_t target_address = (address_high << 8) + address_low;

	uint16_t return_address = registers->pc + 3;
	/* Hardware subtracts one from the correct return address */
	return_address -= 1;

	uint8_t return_address_high = (return_address & 0xFF00) >> 8;
	uint8_t return_address_low = return_address & 0x00FF;
	push_to_stack(console, registers, return_address_high);
	push_to_stack(console, registers, return_address_low);

	registers->pc = target_address;
}

static void execute_return_from_subroutine(struct nes_emulator_console *console,
                                           struct registers *registers,
                                           uint16_t *step_cycles)
{
	uint8_t address_low = pop_from_stack(console, registers);
	uint8_t address_high = pop_from_stack(console, registers);;
	uint16_t address = (address_high << 8) + address_low;
	/* Hardware adds one to get the correct return address */
	address += 1;
	registers->pc = address;
	*step_cycles = 6;
}

static void execute_pull_processor_status(struct nes_emulator_console *console,
                                          struct registers *registers)
{
	bool current_break_command_flag= get_break_command_flag(registers);
	registers->p = pop_from_stack(console, registers);
	set_unused_flag(registers);
	assign_break_command_flag(registers, current_break_command_flag);
}

static void execute_return_from_interrupt(struct nes_emulator_console *console,
                                          struct registers *registers,
                                          uint16_t *step_cycles)
{
	execute_pull_processor_status(console, registers);
	uint8_t address_low = pop_from_stack(console, registers);
	uint8_t address_high = pop_from_stack(console, registers);;
	uint16_t address = (address_high << 8) + address_low;
	registers->pc = address;
	*step_cycles = 6;
}

static void execute_interrupt(struct nes_emulator_console *console,
                              uint16_t handler_address,
                              uint16_t *step_cycles)
{
	struct registers *registers = &console->cpu.registers;
	uint16_t return_address = registers->pc;
	uint8_t return_address_high = (return_address & 0xFF00) >> 8;
	uint8_t return_address_low = return_address & 0x00FF;
	push_to_stack(console, registers, return_address_high);
	push_to_stack(console, registers, return_address_low);

	push_to_stack(console, registers, registers->p);
	uint8_t address_low = cpu_bus_read(console, handler_address);
	uint8_t address_high = cpu_bus_read(console, handler_address + 1);
	uint16_t address = (address_high << 8) + address_low;
	registers->pc = address;

	*step_cycles = 7;
}

static void execute_force_interrupt(struct nes_emulator_console *console,
                                    struct registers *registers,
                                    uint16_t *step_cycles)
{
	registers->pc += 2;
	execute_interrupt(console, IRQ_HANDLER_ADDRESS, step_cycles);
	set_break_command_flag(registers);
}

static void execute_isb(struct nes_emulator_console *console,
                        struct registers *registers)
{
	execute_increment_memory(console, registers);
	execute_subtract_with_carry(console, registers);
}

static void execute_dcp(struct nes_emulator_console *console,
                        struct registers *registers)
{
	execute_decrement_memory(console, registers);
	execute_compare(console, registers, registers->a);
}

static void execute_slo(struct nes_emulator_console *console,
                        struct registers *registers)
{
	execute_arithmetic_shift_left(console, registers);
	execute_logical_inclusive_or(console, registers);
}

static void execute_rla(struct nes_emulator_console *console,
                        struct registers *registers)
{
	execute_rotate_left(console, registers);
	execute_logical_and(console, registers);
}

static void execute_sre(struct nes_emulator_console *console,
                        struct registers *registers)
{
	execute_logical_shift_right(console, registers);
	execute_logical_exclusive_or(console, registers);
}

static void execute_rra(struct nes_emulator_console *console,
                        struct registers *registers)
{
	execute_rotate_right(console, registers);
	execute_add_with_carry(console, registers);
}

static uint8_t execute_instruction(struct nes_emulator_console *console,
                                   uint16_t *step_cycles)
{
	struct registers *registers = &console->cpu.registers;

	if (console->cpu.dma_suspend_cycles > 0) {
		*step_cycles = console->cpu.dma_suspend_cycles;
		console->cpu.dma_suspend_cycles = 0;
		return 0;
	}

	if (console->cpu.nmi_queued) {
		if (console->cpu.nmi_delay) {
			console->cpu.nmi_delay = false;
		}
		else {
			execute_interrupt(console, NMI_HANDLER_ADDRESS, step_cycles);
			console->cpu.nmi_queued = false;
			return 0;
		}
	}

	uint8_t opcode = cpu_bus_read(console, registers->pc);

	/* Processor is little-endian */
	switch (opcode) {
	case 0x00:
		/* BRK - Force Interrupt */
		execute_force_interrupt(console, registers, step_cycles);
		break;
	case 0x01:
		/* ORA - Logical Inclusive OR */
		compute_indirect_x_address(console, registers);
		execute_logical_inclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x03:
		/* SLO (Illegal Opcode) */
		compute_indirect_x_address(console, registers);
		execute_slo(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0x04:
		/* DOP (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x05:
		/* ORA - Logical Inclusive OR */
		compute_zero_page_address(console, registers);
		execute_logical_inclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x06:
		/* ASL - Arithmetic Shift Left */
		compute_zero_page_address(console, registers);
		execute_arithmetic_shift_left(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0x07:
		/* SLO (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		execute_slo(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0x08:
		/* PHP - Push Processor Status */
		push_to_stack(console, registers, registers->p | 0x10);
		registers->pc += 1;
		*step_cycles = 3;
		break;
	case 0x09:
		/* ORA - Logical Inclusive OR */
		compute_immediate_address(console, registers);
		execute_logical_inclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x0A:
		/* ASL - Arithmetic Shift Left */
		execute_arithmetic_shift_left_accumulator(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x0B:
		/* AAC (ANC) (Illegal Opcode) */
		compute_immediate_address(console, registers);
		execute_logical_and(console, registers);
		assign_carry_flag(console, get_negative_flag(console));
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x0C:
		/* TOP (Illegal Opcode) */
		compute_absolute_address(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x0D:
		/* ORA - Logical Inclusive OR */
		compute_absolute_address(console, registers);
		execute_logical_inclusive_or(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x0E:
		/* ASL - Arithmetic Shift Left */
		compute_absolute_address(console, registers);
		execute_arithmetic_shift_left(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0x0F:
		/* SLO (Illegal Opcode) */
		compute_absolute_address(console, registers);
		execute_slo(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0x10:
		/* BPL - Branch if Positive */
		execute_branch(console, registers, get_negative_flag, false,
		               step_cycles);
		break;
	case 0x11:
		/* ORA - Logical Inclusive OR */
		compute_indirect_y_address(console, registers);
		execute_logical_inclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x13:
		/* SLO (Illegal Opcode) */
		compute_indirect_y_address(console, registers);
		execute_slo(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0x14:
		/* DOP (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x15:
		/* ORA - Logical Inclusive OR */
		compute_zero_page_x_address(console, registers);
		execute_logical_inclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x16:
		/* ASL - Arithmetic Shift Left */
		compute_zero_page_x_address(console, registers);
		execute_arithmetic_shift_left(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x17:
		/* SLO (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		execute_slo(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x18:
		/* CLC - Clear Carry Flag */
		clear_carry_flag(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x19:
		/* ORA - Logical Inclusive OR */
		compute_absolute_y_address(console, registers);
		execute_logical_inclusive_or(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x1A:
		/* NOP - No Operation (Illegal Opcode) */
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x1B:
		/* SLO (Illegal Opcode) */
		compute_absolute_y_address(console, registers);
		execute_slo(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x1C:
		/* TOP (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x1D:
		/* ORA - Logical Inclusive OR */
		compute_absolute_x_address(console, registers);
		execute_logical_inclusive_or(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x1E:
		/* ASL - Arithmetic Shift Left */
		compute_absolute_x_address(console, registers);
		execute_arithmetic_shift_left(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x1F:
		/* SLO (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		execute_slo(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x20:
		/* JSR - Jump to Subroutine */
		execute_jump_to_subroutine(console, registers);
		*step_cycles = 6;
		break;
	case 0x21:
		/* AND - Logical AND */
		compute_indirect_x_address(console, registers);
		execute_logical_and(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x23:
		/* RLA (Illegal Opcode) */
		compute_indirect_x_address(console, registers);
		execute_rla(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0x24:
		/* BIT - Bit Test */
		compute_zero_page_address(console, registers);
		execute_bit_test(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x25:
		/* AND - Logical AND */
		compute_zero_page_address(console, registers);
		execute_logical_and(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x26:
		/* ROL - Rotate Left */
		compute_zero_page_address(console, registers);
		execute_rotate_left(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0x27:
		/* RLA (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		execute_rla(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0x28:
		/* PLP - Pull Processor Status */
		execute_pull_processor_status(console, registers);
		registers->pc += 1;
		*step_cycles = 4;
		break;
	case 0x29:
		/* AND - Logical AND */
		compute_immediate_address(console, registers);
		execute_logical_and(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x2A:
		/* ROL - Rotate Left */
		execute_rotate_left_accumulator(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x2B:
		/* AAC (ANC) (Illegal Opcode) */
		compute_immediate_address(console, registers);
		execute_logical_and(console, registers);
		assign_carry_flag(console, get_negative_flag(console));
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x2C:
		/* BIT - Bit Test */
		compute_absolute_address(console, registers);
		execute_bit_test(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x2D:
		/* AND - Logical AND */
		compute_absolute_address(console, registers);
		execute_logical_and(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x2E:
		/* ROL - Rotate Left */
		compute_absolute_address(console, registers);
		execute_rotate_left(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0x2F:
		/* RLA (Illegal Opcode) */
		compute_absolute_address(console, registers);
		execute_rla(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0x30:
		/* BMI - Branch if Minus */
		execute_branch(console, registers, get_negative_flag, true, step_cycles);
		break;
	case 0x31:
		/* AND - Logical AND */
		compute_indirect_y_address(console, registers);
		execute_logical_and(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x33:
		/* RLA (Illegal Opcode) */
		compute_indirect_y_address(console, registers);
		execute_rla(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0x34:
		/* DOP (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x35:
		/* AND - Logical AND */
		compute_zero_page_x_address(console, registers);
		execute_logical_and(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x36:
		/* ROL - Rotate Left */
		compute_zero_page_x_address(console, registers);
		execute_rotate_left(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x37:
		/* RLA (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		execute_rla(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x38:
		/* SEC - Set Carry Flag */
		set_carry_flag(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x39:
		/* AND - Logical AND */
		compute_absolute_y_address(console, registers);
		execute_logical_and(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x3A:
		/* NOP - No Operation (Illegal Opcode) */
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x3B:
		/* RLA (Illegal Opcode) */
		compute_absolute_y_address(console, registers);
		execute_rla(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x3C:
		/* TOP (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x3D:
		/* AND - Logical AND */
		compute_absolute_x_address(console, registers);
		execute_logical_and(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x3E:
		/* ROL - Rotate Left */
		compute_absolute_x_address(console, registers);
		execute_rotate_left(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x3F:
		/* RLA (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		execute_rla(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x40:
		/* RTI - Return from Interrupt */
		execute_return_from_interrupt(console, registers, step_cycles);
		break;
	case 0x41:
		/* EOR - Exclusive OR */
		compute_indirect_x_address(console, registers);
		execute_logical_exclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x43:
		/* SRE (Illegal Opcode) */
		compute_indirect_x_address(console, registers);
		execute_sre(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0x44:
		/* DOP (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x45:
		/* EOR - Exclusive OR */
		compute_zero_page_address(console, registers);
		execute_logical_exclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x46:
		/* LSR - Logical Shift Right */
		compute_zero_page_address(console, registers);
		execute_logical_shift_right(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0x47:
		/* SRE (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		execute_sre(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0x48:
		/* PHA - Push Accumulator */
		push_to_stack(console, registers, registers->a);
		registers->pc += 1;
		*step_cycles = 3;
		break;
	case 0x49:
		/* EOR - Exclusive OR */
		compute_immediate_address(console, registers);
		execute_logical_exclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x4A:
		/* LSR - Logical Shift Right */
		execute_logical_shift_right_accumulator(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x4B:
		/* ASR (Illegal Opcode) */
		compute_immediate_address(console, registers);
		execute_logical_and(console, registers);
		execute_logical_shift_right_accumulator(console);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x4C:
		/* JMP - Jump */
		execute_jump_absolute(console, registers);
		*step_cycles = 3;
		break;
	case 0x4D:
		/* EOR - Exclusive OR */
		compute_absolute_address(console, registers);
		execute_logical_exclusive_or(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x4E:
		/* LSR - Logical Shift Right */
		compute_absolute_address(console, registers);
		execute_logical_shift_right(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0x4F:
		/* SRE (Illegal Opcode) */
		compute_absolute_address(console, registers);
		execute_sre(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0x50:
		/* BVC - Branch if Overflow Clear */
		execute_branch(console, registers, get_overflow_flag, false,
		               step_cycles);
		break;
	case 0x51:
		/* EOR - Exclusive OR */
		compute_indirect_y_address(console, registers);
		execute_logical_exclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x53:
		/* SRE (Illegal Opcode) */
		compute_indirect_y_address(console, registers);
		execute_sre(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0x54:
		/* DOP (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x55:
		/* EOR - Exclusive OR */
		compute_zero_page_x_address(console, registers);
		execute_logical_exclusive_or(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x56:
		/* LSR - Logical Shift Right */
		compute_zero_page_x_address(console, registers);
		execute_logical_shift_right(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x57:
		/* SRE (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		execute_sre(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x58:
		/* CLI - Clear Interrupt Disable Flag */
		clear_interrupt_disable_flag(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x59:
		/* EOR - Exclusive OR */
		compute_absolute_y_address(console, registers);
		execute_logical_exclusive_or(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x5A:
		/* NOP - No Operation (Illegal Opcode) */
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x5B:
		/* SRE (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		execute_sre(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x5C:
		/* TOP (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x5D:
		/* EOR - Exclusive OR */
		compute_absolute_x_address(console, registers);
		execute_logical_exclusive_or(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x5E:
		/* LSR - Logical Shift Right */
		compute_absolute_x_address(console, registers);
		execute_logical_shift_right(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x5F:
		/* SRE (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		execute_sre(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x60:
		/* RTS - Return from Subroutine */
		execute_return_from_subroutine(console, registers, step_cycles);
		break;
	case 0x61:
		/* ADC - Add with Carry */
		compute_indirect_x_address(console, registers);
		compute_indirect_x_address(console, registers);
		execute_add_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x63:
		/* RRA (Illegal Opcode) */
		compute_indirect_x_address(console, registers);
		execute_rra(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0x64:
		/* DOP (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x65:
		/* ADC - Add with Carry */
		compute_zero_page_address(console, registers);
		execute_add_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x66:
		/* ROR - Rotate Right */
		compute_zero_page_address(console, registers);
		execute_rotate_right(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0x67:
		/* RRA (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		execute_rra(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0x68:
		/* PLA - Pull Accumulator */
		registers->a = pop_from_stack(console, registers);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 1;
		*step_cycles = 4;
		break;
	case 0x69:
		/* ADC - Add with Carry */
		compute_immediate_address(console, registers);
		execute_add_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x6A:
		/* ROR - Rotate Right */
		execute_rotate_right_accumulator(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x6B:
		/* ARR (Illegal Opcode) */
		compute_immediate_address(console, registers);
		execute_logical_and(console, registers);
		execute_rotate_right_accumulator(registers);
		{
			bool bit6 = registers->a & (1 << 6);
			bool bit5 = registers->a & (1 << 5);
			assign_carry_flag(console, bit6);
			assign_overflow_flag(console, bit6 ^ bit5);
		}
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x6C:
		/* JMP - Jump */
		execute_jump_indirect(console, registers);
		*step_cycles = 5;
		break;
	case 0x6D:
		/* ADC - Add with Carry */
		compute_absolute_address(console, registers);
		execute_add_with_carry(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x6E:
		/* ROR - Rotate Right */
		compute_absolute_address(console, registers);
		execute_rotate_right(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0x6F:
		/* RRA (Illegal Opcode) */
		compute_absolute_address(console, registers);
		execute_rra(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0x70:
		/* BVS - Branch if Overflow Set */
		execute_branch(console, registers, get_overflow_flag, true, step_cycles);
		break;
	case 0x71:
		/* ADC - Add with Carry */
		compute_indirect_y_address(console, registers);
		execute_add_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x73:
		/* RRA (Illegal Opcode) */
		compute_indirect_y_address(console, registers);
		execute_rra(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0x74:
		/* DOP (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x75:
		/* ADC - Add with Carry */
		compute_zero_page_x_address(console, registers);
		execute_add_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x76:
		/* ROR - Rotate Right */
		compute_zero_page_x_address(console, registers);
		execute_rotate_right(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x77:
		/* RRA (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		execute_rra(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x78:
		/* SEI - Set Interrupt Disable */
		set_interrupt_disable_flag(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x79:
		/* ADC - Add with Carry */
		compute_absolute_y_address(console, registers);
		execute_add_with_carry(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x7A:
		/* NOP - No Operation (Illegal Opcode) */
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x7B:
		/* RRA (Illegal Opcode) */
		compute_absolute_y_address(console, registers);
		execute_rra(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x7C:
		/* TOP (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x7D:
		/* ADC - Add with Carry */
		compute_absolute_x_address(console, registers);
		execute_add_with_carry(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0x7E:
		/* ROR - Rotate Right */
		compute_absolute_x_address(console, registers);
		execute_rotate_right(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x7F:
		/* RRA (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		execute_rra(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0x80:
		/* DOP (Illegal Opcode) */
		compute_immediate_address(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x81:
		/* STA - Store Accumulator */
		compute_indirect_x_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x82:
		/* DOP (Illegal Opcode) */
		compute_immediate_address(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x83:
		/* SAX (Illegal Opcode) */
		compute_indirect_x_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a & registers->x);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x84:
		/* STY - Store Y Register */
		compute_zero_page_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->y);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x85:
		/* STA - Store Accumulator */
		compute_zero_page_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x86:
		/* STX - Store X Register */
		compute_zero_page_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->x);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x87:
		/* SAX (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a & registers->x);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0x88:
		/* DEY - Decrement Y Register */
		registers->y -= 1;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x89:
		/* DOP (Illegal Opcode) */
		compute_immediate_address(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0x8A:
		/* TXA - Transfer X to Accumulator */
		registers->a = registers->x;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x8C:
		/* STY - Store Y Register */
		compute_absolute_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->y);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x8D:
		/* STA - Store Accumulator */
		compute_absolute_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x8E:
		/* STX - Store X Register */
		compute_absolute_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->x);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x8F:
		/* SAX (Illegal Opcode) */
		compute_absolute_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a & registers->x);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0x90:
		/* BCC - Branch if Carry Clear */
		execute_branch(console, registers, get_carry_flag, false, step_cycles);
		break;
	case 0x91:
		/* STA - Store Accumulator */
		compute_indirect_y_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0x94:
		/* STY - Store Y Register */
		compute_zero_page_x_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->y);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x95:
		/* STA - Store Accumulator */
		compute_zero_page_x_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x96:
		/* STX - Store X Register */
		compute_zero_page_y_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->x);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x97:
		/* SAX (Illegal Opcode) */
		compute_zero_page_y_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a & registers->x);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0x98:
		/* TYA - Transfer Y to Accumulator */
		registers->a = registers->y;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x99:
		/* STA - Store Accumulator */
		compute_absolute_y_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a);
		registers->pc += 3;
		*step_cycles = 5;
		break;
	case 0x9A:
		/* TXS - Transfer X to Stack Pointer */
		registers->s = registers->x;
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0x9D:
		/* STA - Store Accumulator */
		compute_absolute_x_address(console, registers);
		cpu_bus_write(console, console->cpu.computed_address, registers->a);
		registers->pc += 3;
		*step_cycles = 5;
		break;
	case 0xA0:
		/* LDY - Load Y Register */
		compute_immediate_address(console, registers);
		registers->y = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xA1:
		/* LDA - Load Accumlator */
		compute_indirect_x_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0xA2:
		/* LDX - Load X Register */
		compute_immediate_address(console, registers);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xA3:
		/* LAX - Load Accumulator and X Register (Illegal Opcode) */
		compute_indirect_x_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0xA4:
		/* LDY - Load Y Register */
		compute_zero_page_address(console, registers);
		registers->y = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0xA5:
		/* LDA - Load Accumlator */
		compute_zero_page_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0xA6:
		/* LDX - Load X Register */
		compute_zero_page_address(console, registers);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0xA7:
		/* LAX - Load Accumulator and X Register (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0xA8:
		/* TAY - Transfer Accumulator to Y */
		registers->y = registers->a;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xA9:
		/* LDA - Load Accumlator */
		compute_immediate_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xAA:
		/* TAY - Transfer Accumulator to X */
		registers->x = registers->a;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xAB:
		/* ATX (Illegal Opcode) */
		compute_immediate_address(console, registers);
		execute_logical_and(console, registers);
		registers->x = registers->a;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xAC:
		/* LDY - Load Y Register */
		compute_absolute_address(console, registers);
		registers->y = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0xAD:
		/* LDA - Load Acuumulator */
		compute_absolute_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0xAE:
		/* LDX - Load X Register */
		compute_absolute_address(console, registers);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0xAF:
		/* LAX - Load Accumulator and X Register (Illegal Opcode) */
		compute_absolute_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0xB0:
		/* BCS - Branch if Carry Set */
		execute_branch(console, registers, get_carry_flag, true, step_cycles);
		break;
	case 0xB1:
		/* LDA - Load Accumlator */
		compute_indirect_y_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		*step_cycles = 5;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		registers->pc += 2;
		break;
	case 0xB3:
		/* LAX - Load Accumulator and X Register (Illegal Opcode) */
		compute_indirect_y_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		*step_cycles = 5;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xB4:
		/* LDY - Load Y Register */
		compute_zero_page_x_address(console, registers);
		registers->y = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0xB5:
		/* LDA - Load Accumlator */
		compute_zero_page_x_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0xB6:
		/* LDX - Load X Register */
		compute_zero_page_y_address(console, registers);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0xB7:
		/* LAX - Load Accumulator and X Register (Illegal Opcode) */
		compute_zero_page_y_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0xB8:
		/* CLV - Clear Overflow Flag */
		clear_overflow_flag(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xB9:
		/* LDA - Load Acuumulator */
		compute_absolute_y_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xBA:
		/* TSX - Transfer Stack Pointer to X */
		registers->x = registers->s;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xBC:
		/* LDY - Load Y Register */
		compute_absolute_x_address(console, registers);
		registers->y = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xBD:
		/* LDA - Load Acuumulator */
		compute_absolute_x_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xBE:
		/* LDX - Load X Register */
		compute_absolute_y_address(console, registers);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xBF:
		/* LAX - Load Accumulator and X Register (Illegal Opcode) */
		compute_absolute_y_address(console, registers);
		registers->a = cpu_bus_read(console, console->cpu.computed_address);
		registers->x = cpu_bus_read(console, console->cpu.computed_address);
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xC0:
		/* CPY - Compare Y Register */
		compute_immediate_address(console, registers);
		execute_compare(console, registers, registers->y);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xC1:
		/* CMP - Compare */
		compute_indirect_x_address(console, registers);
		execute_compare(console, registers, registers->a);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0xC2:
		/* DOP (Illegal Opcode) */
		compute_immediate_address(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xC3:
		/* DCP (Illegal Opcode) */
		compute_indirect_x_address(console, registers);
		execute_dcp(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0xC4:
		/* CPY - Compare Y Register */
		compute_zero_page_address(console, registers);
		execute_compare(console, registers, registers->y);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0xC5:
		/* CMP - Compare */
		compute_zero_page_address(console, registers);
		execute_compare(console, registers, registers->a);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0xC6:
		/* DEC - Decrement Memory */
		compute_zero_page_address(console, registers);
		execute_decrement_memory(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0xC7:
		/* DCP (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		execute_dcp(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0xC8:
		/* INY - Increment Y Register */
		registers->y += 1;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->y);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xC9:
		/* CMP - Compare */
		compute_immediate_address(console, registers);
		execute_compare(console, registers, registers->a);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xCA:
		/* DEX - Decrement X Register */
		registers->x -= 1;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xCB:
		/* AXS (Illegal Opcode) */
		compute_immediate_address(console, registers);
		registers->x &= registers->a;
		execute_subtract_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xCC:
		/* CPY - Compare Y Register */
		compute_absolute_address(console, registers);
		execute_compare(console, registers, registers->y);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0xCD:
		/* CMP - Compare */
		compute_absolute_address(console, registers);
		execute_compare(console, registers, registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0xCE:
		/* DEC - Decrement Memory */
		compute_absolute_address(console, registers);
		execute_decrement_memory(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0xCF:
		/* DCP (Illegal Opcode) */
		compute_absolute_address(console, registers);
		execute_dcp(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0xD0:
		/* BNE - Branch if Not Equal */
		execute_branch(console, registers, get_zero_flag, false, step_cycles);
		break;
	case 0xD1:
		/* CMP - Compare */
		compute_indirect_y_address(console, registers);
		execute_compare(console, registers, registers->a);
		registers->pc += 2;
		*step_cycles = 5;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xD3:
		/* DCP (Illegal Opcode) */
		compute_indirect_y_address(console, registers);
		execute_dcp(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0xD4:
		/* DOP (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0xD5:
		/* CMP - Compare */
		compute_zero_page_x_address(console, registers);
		execute_compare(console, registers, registers->a);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0xD6:
		/* DEC - Decrement Memory */
		compute_zero_page_x_address(console, registers);
		execute_decrement_memory(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0xD7:
		/* DCP (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		execute_dcp(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0xD8:
		/* CLD - Clear Decimal Mode */
		clear_decimal_mode_flag(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xD9:
		/* CMP - Compare */
		compute_absolute_y_address(console, registers);
		execute_compare(console, registers, registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xDA:
		/* NOP - No Operation (Illegal Opcode) */
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xDB:
		/* DCP (Illegal Opcode) */
		compute_absolute_y_address(console, registers);
		execute_dcp(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0xDC:
		/* TOP (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xDD:
		/* CMP - Compare */
		compute_absolute_x_address(console, registers);
		execute_compare(console, registers, registers->a);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xDE:
		/* DEC - Decrement Memory */
		compute_absolute_x_address(console, registers);
		execute_decrement_memory(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0xDF:
		/* DCP (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		execute_dcp(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0xE0:
		/* CPX - Compare X Register */
		compute_immediate_address(console, registers);
		execute_compare(console, registers, registers->x);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xE1:
		/* SBC - Subtract with Carry */
		compute_indirect_x_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0xE2:
		/* DOP (Illegal Opcode) */
		compute_immediate_address(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xE3:
		/* ISB (Illegal Opcode) */
		compute_indirect_x_address(console, registers);
		execute_isb(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0xE4:
		/* CPX - Compare X Register */
		compute_zero_page_address(console, registers);
		execute_compare(console, registers, registers->x);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0xE5:
		/* SBC - Subtract with Carry */
		compute_zero_page_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 3;
		break;
	case 0xE6:
		/* INC - Increment Memory */
		compute_zero_page_address(console, registers);
		execute_increment_memory(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0xE7:
		/* ISB (Illegal Opcode) */
		compute_zero_page_address(console, registers);
		execute_isb(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		break;
	case 0xE8:
		/* INX - Increment X Register */
		registers->x += 1;
		assign_negative_and_zero_flags_from_value(registers,
		                                          registers->x);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xE9:
		/* SBC - Subtract with Carry */
		compute_immediate_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xEA:
		/* NOP - No Operation */
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xEB:
		/* SBC - Subtract with Carry (Illegal Opcode) */
		compute_immediate_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 2;
		break;
	case 0xEC:
		/* CPX - Compare X Register */
		compute_absolute_address(console, registers);
		execute_compare(console, registers, registers->x);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0xED:
		/* SBC - Subtract with Carry */
		compute_absolute_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		break;
	case 0xEE:
		/* INC - Increment Memory */
		compute_absolute_address(console, registers);
		execute_increment_memory(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0xEF:
		/* ISB (Illegal Opcode) */
		compute_absolute_address(console, registers);
		execute_isb(console, registers);
		registers->pc += 3;
		*step_cycles = 6;
		break;
	case 0xF0:
		/* BEQ - Branch if Equal */
		execute_branch(console, registers, get_zero_flag, true, step_cycles);
		break;
	case 0xF1:
		/* SBC - Subtract with Carry */
		compute_indirect_y_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 5;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xF3:
		/* ISB (Illegal Opcode) */
		compute_indirect_y_address(console, registers);
		execute_isb(console, registers);
		registers->pc += 2;
		*step_cycles = 8;
		break;
	case 0xF4:
		/* DOP (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0xF5:
		/* SBC - Subtract with Carry */
		compute_zero_page_x_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 2;
		*step_cycles = 4;
		break;
	case 0xF6:
		/* INC - Increment Memory */
		compute_zero_page_x_address(console, registers);
		execute_increment_memory(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0xF7:
		/* ISB (Illegal Opcode) */
		compute_zero_page_x_address(console, registers);
		execute_isb(console, registers);
		registers->pc += 2;
		*step_cycles = 6;
		break;
	case 0xF8:
		/* SED - Set Decimal Flag */
		set_decimal_mode_flag(registers);
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xF9:
		/* SBC - Subtract with Carry */
		compute_absolute_y_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->y,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xFA:
		/* NOP - No Operation (Illegal Opcode) */
		registers->pc += 1;
		*step_cycles = 2;
		break;
	case 0xFB:
		/* ISB (Illegal Opcode) */
		compute_absolute_y_address(console, registers);
		execute_isb(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0xFC:
		/* TOP */
		/* Illegal Opcode */
		compute_absolute_x_address(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xFD:
		/* SBC - Subtract with Carry */
		compute_absolute_x_address(console, registers);
		execute_subtract_with_carry(console, registers);
		registers->pc += 3;
		*step_cycles = 4;
		if (is_page_crossed(console->cpu.computed_address - registers->x,
		                    console->cpu.computed_address)) {
			*step_cycles += 1;
		}
		break;
	case 0xFE:
		/* INC - Increment Memory */
		compute_absolute_x_address(console, registers);
		execute_increment_memory(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	case 0xFF:
		/* ISB (Illegal Opcode) */
		compute_absolute_x_address(console, registers);
		execute_isb(console, registers);
		registers->pc += 3;
		*step_cycles = 7;
		break;
	default:
printf("It's %02X\n", opcode);
		return EXIT_CODE_UNIMPLEMENTED_BIT;
	}

	return 0;
}

void cpu_init(struct nes_emulator_console *console)
{
	init_registers(&console->cpu.registers);
	for (int i = 0; i < CPU_RAM_SIZE; ++i) {
		console->cpu.ram[i] = 0;
	}
	console->cpu.computed_address = 0x0000;
	console->cpu.nmi_queued = false;
	console->cpu.nmi_delay = false;
	console->cpu.controller_latch = false;
	console->cpu.controller_shift = 0;
	console->cpu.controller_status = 0;
	console->cpu.dma_suspend_cycles = 0;
}

void cpu_reset(struct nes_emulator_console *console)
{
	struct registers *registers = &console->cpu.registers;
	registers->pc = cpu_bus_read(console, RESET_HANDLER_ADDRESS)
	             + (cpu_bus_read(console, RESET_HANDLER_ADDRESS + 1) << 8);
	console->cpu.nmi_queued = false;
	console->cpu.nmi_delay = false;
	console->cpu.controller_latch = false;
	console->cpu.controller_shift = 0;
	console->cpu.controller_status = 0;
	console->cpu.dma_suspend_cycles = 0;
}

uint8_t cpu_step(struct nes_emulator_console *console)
{
	return execute_instruction(console, &console->cpu_step_cycles);
}

void cpu_generate_nmi(struct nes_emulator_console *console)
{
	console->cpu.nmi_queued = true;
}
