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

#include "apu.h"

#include "console.h"

void apu_init(struct nes_emulator_console *console)
{
	console->apu.status = 0;
}

uint8_t apu_cpu_bus_read(struct nes_emulator_console *console,
                         uint16_t address)
{
	switch (address) {
	case 0x4015:
		return console->apu.status;
	default:
		return 0;
	}
}

void apu_cpu_bus_write(struct nes_emulator_console *console,
                       uint16_t address,
                       uint8_t value)
{
	switch (address) {
	case 0x4015:
		console->apu.status = value;
		break;
	default:
		break;
	}
	/* Pulse 1 */
	/* Pulse 2 */
	/* Triangle */
	/* Noise */
	/* DMC */
}
