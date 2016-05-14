# Copyright 2016 Jonathan Eyolfson
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 3 as published by the Free
# Software Foundation.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.1.3)

project(NES_EMULATOR C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wextra)

set(COMMON_SRC
	../../../src/args.c
	../../../src/cartridge.c
	../../../src/console.c
	../../../src/cpu.c
	../../../src/exit_code.c
	../../../src/ppu.c
	../../../src/ppu_register.c
)

add_executable(nes-emulator-ppu_vbl_nmi-01
	main.c
	01.c
	${COMMON_SRC}
)
