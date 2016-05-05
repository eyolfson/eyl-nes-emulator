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

#include "../../../src/args.h"
#include "../../../src/exit_code.h"
#include "../../../src/console.h"
#include "../../../src/cpu.h"
#include "../../../src/ppu.h"
#include "../../../src/nes_emulator.h"

#include <stdio.h>

static uint64_t frame = 0;
static bool test_running = true;
static bool check_success = true;

#define WIDTH 256
#define HEIGHT 240

//const uint64_t CHECK_FRAME = 300;
extern const uint64_t CHECK_FRAME;
extern const uint8_t CHECK_DATA[HEIGHT][WIDTH];

static void render_pixel(void *pointer, uint8_t x, uint8_t y, uint8_t c)
{
	if (frame == CHECK_FRAME) {
		if (CHECK_DATA[y][x] != c) {
			test_running = false;
			check_success = false;
		}
	}
}

static void vertical_blank(void *pointer)
{
	if (frame == CHECK_FRAME) {
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

	if (check_success) {
		printf("Check SUCCESS\n");
	}
	else {
		printf("Check FAILURE\n");
	}

	nes_emulator_cartridge_fini(&cartridge);
	nes_emulator_console_fini(&console);
	exit_code |= fini_memory_mapping(&mm);
	return exit_code;
}
