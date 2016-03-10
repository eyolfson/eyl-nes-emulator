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

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MEMORY_SIZE 0x10000

uint8_t memory[MEMORY_SIZE];

struct registers {
	uint8_t a;   /* Accumulator */
	uint8_t x;   /* Index Register 0 */
	uint8_t y;   /* Index Register 1 */
	uint8_t p;   /* Processor Status Flag Bits */
	uint8_t s;   /* Stack Pointer */
	uint16_t pc; /* Program Counter */
};

void init_registers(struct registers *registers)
{
	registers->a = 0;
	registers->x = 0;
	registers->y = 0;
	registers->p = 0;
	registers->s = 0xFF;
	registers->pc = 0xC000;
}

void init()
{
	struct registers registers;
	init_registers(&registers);
}

void set_carry_flag(struct registers *registers, bool c)
{
	registers->p |= c << 0;
}

bool get_carry_flag(struct registers *registers)
{
	return registers->p & 1 << 0;
}

void set_zero_flag(struct registers *registers, bool z)
{
	registers->p |= z << 1;
}

bool get_zero_flag(struct registers *registers)
{
	return registers->p & 1 << 1;
}

void set_interrupt_disable_flag(struct registers *registers, bool i)
{
	registers->p |= i << 2;
}

bool get_interrupt_disable_flag(struct registers *registers)
{
	return registers->p & 1 << 2;
}

void set_decimal_mode_flag(struct registers *registers, bool d)
{
	registers->p |= d << 3;
}

bool get_decimal_mode_flag(struct registers *registers)
{
	return registers->p & 1 << 3;
}

void set_break_command_flag(struct registers *registers, bool b)
{
	registers->p |= b << 4;
}

bool get_break_command_flag(struct registers *registers)
{
	return registers->p & 1 << 4;
}

void set_overflow_flag(struct registers *registers, bool v)
{
	registers->p |= v << 6;
}

bool get_overflow_flag(struct registers *registers)
{
	return registers->p & 1 << 6;
}

void set_negative_flag(struct registers *registers, bool n)
{
	registers->p |= n << 7;
}

bool get_negative_flag(struct registers *registers)
{
	return registers->p & 1 << 7;
}

static const uint16_t HEADER_SIZE = 16;               /* 16 B */
static const uint16_t PRG_ROM_SIZE_PER_UNIT = 0x4000; /* 16 KiB */
static const uint16_t CHR_ROM_SIZE_PER_UNIT = 0x2000; /*  8 KiB */

static const uint8_t EXIT_CODE_ARG_ERROR_BIT = 1 << 0;
static const uint8_t EXIT_CODE_OS_ERROR_BIT = 1 << 1;
static const uint8_t EXIT_CODE_UNIMPLEMENTED_BIT = 1 << 2;


/* PPU: Picture Processing Unit */

static
uint8_t
check_rom_size_raw(uint8_t *data, size_t size)
{
	if (data[0] != 'N') {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
	if (data[1] != 'E') {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
	if (data[2] != 'S') {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
	if (data[3] != 0x1A) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	uint8_t prg_rom_units = data[4];
	if (prg_rom_units > 2) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	uint8_t chr_rom_units = data[5];
	if (chr_rom_units > 1) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	uint16_t expected_size = HEADER_SIZE;
	expected_size += prg_rom_units * PRG_ROM_SIZE_PER_UNIT;
	expected_size += chr_rom_units * CHR_ROM_SIZE_PER_UNIT;

	if (size == expected_size) {
		return 0;
	}
	else {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
}

static
void
load_rom_into_memory(uint8_t *data, size_t size)
{
	uint8_t prg_rom_units = data[4];
	memcpy(memory + 0xC000, data + HEADER_SIZE, PRG_ROM_SIZE_PER_UNIT);
}

static
uint8_t
execute_instruction(struct registers *registers)
{
	uint8_t *memory_pc_offset = memory + registers->pc;
	uint8_t opcode = *(memory_pc_offset);

	uint16_t t1;
	uint16_t t2;

		/* Processor is little-endian */
	switch (opcode) {
	case 0x4C:
		/* JMP - Jump */
		/* Address is absolute */
		/* Bytes: 3 */
		/* Cycles: 3 */
		t1 = *(memory_pc_offset + 1) + (*(memory_pc_offset + 2) << 8);
		registers->pc = t1;
		break;
	case 0x6C:
		/* JMP - Jump */
		/* Address is indirect */
		/* Bytes: 3 */
		/* Cycles: 5 */
		t1 = *(memory_pc_offset + 1) + (*(memory_pc_offset + 2) << 8);
		t2 = *(memory + t1) + (*(memory + t1 + 1) << 8);
		registers->pc = t2;
		break;
	default:
		return EXIT_CODE_UNIMPLEMENTED_BIT;
	}

	return 0;
}

static
uint8_t
get_rom_raw(const char *path, uint8_t **data, size_t *size)
{
	int32_t fd = open(path, O_RDONLY);
	if (fd < 0) {
		return EXIT_CODE_OS_ERROR_BIT;
	}

	uint8_t exit_code = 0;

	struct stat stat;
	if (fstat(fd, &stat) < 0) {
		exit_code |= EXIT_CODE_OS_ERROR_BIT;
	}

	size_t local_size = stat.st_size;
	if (exit_code == 0) {
		/* File size is too big for a ROM file */
		if (local_size > UINT16_MAX) {
			exit_code |= EXIT_CODE_ARG_ERROR_BIT;
		}
	}

	if (exit_code == 0) {
		uint8_t *p = mmap(NULL, local_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (p != MAP_FAILED) {
			*data = p;
		}
		else {
			exit_code |= EXIT_CODE_OS_ERROR_BIT;
		}
	}

	if (exit_code == 0) {
		*size = local_size;
	}

	if (close(fd) >= 0) {
		return exit_code;
	}
	else {
		return exit_code | EXIT_CODE_OS_ERROR_BIT;
	}
}

uint8_t main(int argc, char **argv)
{
	if (argc != 2) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	uint8_t *rom_data;
	size_t rom_size;
	uint8_t exit_code = get_rom_raw(argv[1], &rom_data, &rom_size);
	if (exit_code != 0) {
		return exit_code;
	}
	exit_code |= check_rom_size_raw(rom_data, rom_size);
	load_rom_into_memory(rom_data, rom_size);

	if (exit_code == 0) {
		struct registers registers;
		init_registers(&registers);
		while (exit_code == 0) {
			exit_code = execute_instruction(&registers);
		}
	}

	if (munmap(rom_data, rom_size) >= 0) {
		return exit_code;
	}
	else {
		return exit_code | EXIT_CODE_OS_ERROR_BIT;
	}
}
