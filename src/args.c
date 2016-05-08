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

#include "args.h"

#include "exit_code.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static uint8_t memory_map_from_path(const char *path, struct memory_mapping *mm)
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

	size_t size = stat.st_size;
	if (exit_code == 0) {
		/* File size is too big for a ROM file */
		if (size > UINT16_MAX) {
			exit_code |= EXIT_CODE_ARG_ERROR_BIT;
		}
	}

	uint8_t *data;
	if (exit_code == 0) {
		data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
		if (data == MAP_FAILED) {
			exit_code |= EXIT_CODE_OS_ERROR_BIT;
		}
	}

	if (exit_code == 0) {
		mm->data = data;
		mm->size = size;
	}

	if (close(fd) >= 0) {
		return exit_code;
	}
	else {
		return exit_code | EXIT_CODE_OS_ERROR_BIT;
	}
}

uint8_t fini_memory_mapping(struct memory_mapping *mm)
{
	if (munmap(mm->data, mm->size) >= 0) {
		return 0;
	}
	else {
		return EXIT_CODE_OS_ERROR_BIT;
	}
}

uint8_t init_memory_mapping_from_args(int argc, char** argv,
                                      struct memory_mapping *mm)
{
	if (argc != 2) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}

	return memory_map_from_path(argv[1], mm);
}
