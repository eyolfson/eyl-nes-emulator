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

#pragma once

#ifdef __cpluscplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

struct memory_mapping {
	uint8_t *data;
	size_t size;
};

uint8_t init_memory_mapping_from_args(int argc, char** argv,
                                      struct memory_mapping *mm);
uint8_t fini_memory_mapping(struct memory_mapping *mm);

#ifdef __cpluscplus
}
#endif
