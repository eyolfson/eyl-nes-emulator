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

#include "apu.h"
#include "cpu.h"
#include "ppu.h"
#include "controller.h"

struct nes_emulator_console {
	struct cpu cpu;
	uint16_t cpu_step_cycles;

	struct ppu ppu;
	struct apu apu;

	struct nes_emulator_controller_backend *controller;

	struct nes_emulator_cartridge *cartridge;
};

#ifdef __cpluscplus
}
#endif
