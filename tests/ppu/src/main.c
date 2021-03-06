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

#include "../../../src/args.h"
#include "../../../src/exit_code.h"
#include "../../../src/console.h"
#include "../../../src/cpu.h"
#include "../../../src/ppu.h"
#include "../../../src/nes_emulator.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define WIDTH 256
#define HEIGHT 240
#define SIZE WIDTH * HEIGHT

static int fd;
static long long check_frame;
static uint8_t buffer[SIZE];

static long long frame = 0;
static bool test_running = true;
static bool test_success = true;

static void render_pixel(void *pointer, uint8_t x, uint8_t y, uint8_t c)
{
	if (frame == check_frame) {
		if (buffer[y * WIDTH + x] != c) {
			test_running = false;
			test_success = false;
		}
	}
}

static void vertical_blank(void *pointer)
{
	if (frame == check_frame) {
		test_running = false;
	}
	else {
		++frame;
	}
}

int main(int argc, char **argv)
{
	struct memory_mapping mm;
	uint8_t exit_code;

	exit_code = init_memory_mapping_from_args(argc, argv, &mm);
	if (exit_code != 0) {
		return exit_code;
	}

	if (argc != 4) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	fd = open(argv[2], O_RDONLY);
	if (fd == -1) {
		exit_code = EXIT_CODE_OS_ERROR_BIT;
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}

	if (read(fd, buffer, SIZE) == -1) {
		exit_code = EXIT_CODE_OS_ERROR_BIT;
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}

	check_frame = atoll(argv[3]);

	struct nes_emulator_console *console;
	exit_code = nes_emulator_console_init(&console);
	if (exit_code != 0) {
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}

	struct nes_emulator_cartridge *cartridge;
	exit_code = nes_emulator_cartridge_init(&cartridge, mm.data, mm.size);
	if (exit_code != 0) {
		nes_emulator_console_fini(&console);
		exit_code |= fini_memory_mapping(&mm);
		return exit_code;
	}

	nes_emulator_console_insert_cartridge(console, cartridge);

	struct nes_emulator_ppu_backend ppu_backend = {
		.pointer = NULL,
		.render_pixel = render_pixel,
		.vertical_blank = vertical_blank,
	};
	nes_emulator_console_add_ppu_backend(console, &ppu_backend);

	while (exit_code == 0 && test_running) {
		exit_code = nes_emulator_console_step(console);
	}

	if (test_success) {
		printf("Test SUCCESS\n");
	}
	else {
		printf("Test FAILURE\n");
	}

	nes_emulator_cartridge_fini(&cartridge);
	nes_emulator_console_fini(&console);
	exit_code |= fini_memory_mapping(&mm);
	return exit_code;
}
