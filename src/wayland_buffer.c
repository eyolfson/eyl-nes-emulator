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

#include "wayland_buffer.h"

#include "wayland.h"

#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "exit_code.h"

uint8_t init_wayland_buffer(struct wayland *wayland)
{
	wayland->fd = syscall(SYS_memfd_create, "nes-emulator",
	                      MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (wayland->fd < 0) {
		return EXIT_CODE_OS_ERROR_BIT;
	}

	int32_t stride = wayland->width * sizeof(uint32_t);
	wayland->capacity = stride * wayland->height;

	if (ftruncate(wayland->fd, wayland->capacity) < 0) {
		uint8_t exit_code = EXIT_CODE_OS_ERROR_BIT;
		if (close(wayland->fd) < 0) {
			exit_code |= EXIT_CODE_OS_ERROR_BIT;
		}
		return exit_code;
	}

	wayland->data = mmap(NULL, wayland->capacity, PROT_WRITE | PROT_READ,
	                    MAP_SHARED, wayland->fd, 0);
	if (wayland->data == MAP_FAILED) {
		uint8_t exit_code = EXIT_CODE_OS_ERROR_BIT;
		if (close(wayland->fd) < 0) {
			exit_code |= EXIT_CODE_OS_ERROR_BIT;
		}
		return exit_code;
	}

	wayland->shm_pool = wl_shm_create_pool(wayland->shm,
	                                       wayland->fd, wayland->capacity);
	if (wayland->shm_pool == NULL) {
		uint8_t exit_code = EXIT_CODE_WAYLAND_BIT;
		if (munmap(wayland->data, wayland->capacity) < 0) {
			exit_code |= EXIT_CODE_OS_ERROR_BIT;
		}
		if (close(wayland->fd) < 0) {
			exit_code |= EXIT_CODE_OS_ERROR_BIT;
		}
		return exit_code;
	}

	wayland->buffer = wl_shm_pool_create_buffer(
		wayland->shm_pool, 0, wayland->width, wayland->height,
		stride, WL_SHM_FORMAT_ARGB8888);
	if (wayland->buffer == NULL) {
		uint8_t exit_code = EXIT_CODE_WAYLAND_BIT;
		wl_shm_pool_destroy(wayland->shm_pool);
		if (munmap(wayland->data, wayland->capacity) < 0) {
			exit_code |= EXIT_CODE_OS_ERROR_BIT;
		}
		if (close(wayland->fd) < 0) {
			exit_code |= EXIT_CODE_OS_ERROR_BIT;
		}
		return exit_code;
	}

	return 0;
}

uint8_t fini_wayland_buffer(struct wayland *wayland)
{
	uint8_t exit_code = 0;
	wl_buffer_destroy(wayland->buffer);
	wl_shm_pool_destroy(wayland->shm_pool);
	if (munmap(wayland->data, wayland->capacity) < 0) {
		exit_code |= EXIT_CODE_OS_ERROR_BIT;
	}
	if (close(wayland->fd) < 0) {
		exit_code |= EXIT_CODE_OS_ERROR_BIT;
	}
	return exit_code;
}
