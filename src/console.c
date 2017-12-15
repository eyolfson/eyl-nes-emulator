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

#include "console.h"

#include <stdlib.h>

#include "exit_code.h"

uint8_t nes_emulator_console_init(struct nes_emulator_console **console)
{
	struct nes_emulator_console *c;

	c = malloc(sizeof(struct nes_emulator_console));
	if (c == NULL) {
		*console = NULL;
		return EXIT_CODE_OS_ERROR_BIT;
	}

	cpu_init(c);
	ppu_init(c);

	c->controller = NULL;
	c->cartridge = NULL;

	*console = c;
	return 0;
}

void nes_emulator_console_insert_cartridge(
	struct nes_emulator_console *console,
	struct nes_emulator_cartridge *cartridge)
{
	console->cartridge = cartridge;
	cpu_reset(console);
}

uint8_t nes_emulator_console_step(struct nes_emulator_console *console)
{
	uint8_t exit_code;

	exit_code = cpu_step(console);
	if (exit_code != 0) {
		return exit_code;
	}

	exit_code = ppu_step(console);
	if (exit_code != 0) {
		return exit_code;
	}

	return 0;
}

void nes_emulator_console_fini(struct nes_emulator_console **console)
{
	if (*console != NULL) {
		free(*console);
	}
	*console = NULL;
}
