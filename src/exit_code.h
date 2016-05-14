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

#include <stdint.h>

extern const uint8_t EXIT_CODE_ARG_ERROR_BIT;
extern const uint8_t EXIT_CODE_OS_ERROR_BIT;
extern const uint8_t EXIT_CODE_WAYLAND_BIT;
extern const uint8_t EXIT_CODE_UNIMPLEMENTED_BIT;

#ifdef __cpluscplus
}
#endif
