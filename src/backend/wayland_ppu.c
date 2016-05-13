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

#include <stdint.h>
#include <stdlib.h>

#include "../exit_code.h"
#include "../ppu.h"
#include "wayland_private.h"

static void render_pixel(void *pointer,
                         uint8_t x,
                         uint8_t y,
                         uint8_t c)
{
	struct wayland *wayland = pointer;
	static uint32_t palette[64] = {
		[0x00] = 0xFF545454,
		[0x01] = 0xFF001E74,
		[0x02] = 0xFF081090,
		[0x03] = 0xFF300088,
		[0x04] = 0xFF440064,
		[0x05] = 0xFF5C0030,
		[0x06] = 0xFF540400,
		[0x07] = 0xFF3C1800,
		[0x08] = 0xFF202A00,
		[0x09] = 0xFF083A00,
		[0x0A] = 0xFF004000,
		[0x0B] = 0xFF003C00,
		[0x0C] = 0xFF00323C,
		[0x0D] = 0xFF000000,
		[0x0E] = 0xFF000000,
		[0x0F] = 0xFF000000,
		[0x10] = 0xFF989698,
		[0x11] = 0xFF084CC4,
		[0x12] = 0xFF3032EC,
		[0x13] = 0xFF5C1EE4,
		[0x14] = 0xFF8814B0,
		[0x15] = 0xFFA01464,
		[0x16] = 0xFF982220,
		[0x17] = 0xFF783C00,
		[0x18] = 0xFF545A00,
		[0x19] = 0xFF287200,
		[0x1A] = 0xFF087C00,
		[0x1B] = 0xFF007628,
		[0x1C] = 0xFF006678,
		[0x1D] = 0xFF000000,
		[0x1E] = 0xFF000000,
		[0x1F] = 0xFF000000,
		[0x20] = 0xFFECEEEC,
		[0x21] = 0xFF4C9AEC,
		[0x22] = 0xFF787CEC,
		[0x23] = 0xFFB062EC,
		[0x24] = 0xFFE454EC,
		[0x25] = 0xFFEC58B4,
		[0x26] = 0xFFEC6A64,
		[0x27] = 0xFFD48820,
		[0x28] = 0xFFA0AA00,
		[0x29] = 0xFF74C400,
		[0x2A] = 0xFF4CD020,
		[0x2B] = 0xFF38CC6C,
		[0x2C] = 0xFF38B4CC,
		[0x2D] = 0xFF3C3C3C,
		[0x2E] = 0xFF000000,
		[0x2F] = 0xFF000000,
		[0x30] = 0xFFECEEEC,
		[0x31] = 0xFFA8CCEC,
		[0x32] = 0xFFBCBCEC,
		[0x33] = 0xFFD4B2EC,
		[0x34] = 0xFFECAEEC,
		[0x35] = 0xFFECAED4,
		[0x36] = 0xFFECB4B0,
		[0x37] = 0xFFE4C490,
		[0x38] = 0xFFCCD278,
		[0x39] = 0xFFB4DE78,
		[0x3A] = 0xFFA8E290,
		[0x3B] = 0xFF98E2B4,
		[0x3C] = 0xFFA0D6E4,
		[0x3D] = 0xFFA0A2A0,
		[0x3E] = 0xFF000000,
		[0x3F] = 0xFF000000,
	};
	for (int32_t i = x * 4; i < x * 4 + 4; ++i) {
		for (int32_t j = y * 4; j < y * 4 + 4; ++j) {
			wayland->back_data[i + (j * wayland->width)] =
				palette[c];
		}
        }
}

static void vertical_blank(void *pointer)
{
  struct wayland *wayland = pointer;
	wl_display_roundtrip(wayland->display);
}

uint8_t nes_emulator_ppu_backend_wayland_init(
	struct nes_emulator_ppu_backend **ppu_backend)
{
	if (ppu_backend == NULL) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
	*ppu_backend = NULL;

	struct nes_emulator_ppu_backend *b =
		malloc(sizeof(struct nes_emulator_ppu_backend));
	if (b == NULL) {
		return EXIT_CODE_OS_ERROR_BIT;
	}

	struct wayland *w = malloc(sizeof(struct wayland));
	if (w == NULL) {
		free(b);
		return EXIT_CODE_OS_ERROR_BIT;
	}

	uint8_t exit_code = init_wayland(w);
	if (exit_code != 0) {
		free(w);
		free(b);
		return exit_code;
	}

	b->pointer = w;
	b->render_pixel = render_pixel;
	b->vertical_blank = vertical_blank;
	*ppu_backend = b;
	return 0;
}

uint8_t nes_emulator_ppu_backend_wayland_fini(
	struct nes_emulator_ppu_backend **ppu_backend)
{
	uint8_t exit_code = 0;
	struct wayland *wayland = (struct wayland *) &((*ppu_backend)->pointer);
	exit_code |= fini_wayland(wayland);
	free(wayland);
	free(*ppu_backend);
	*ppu_backend = NULL;
	return exit_code;
}
