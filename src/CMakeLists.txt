# Copyright 2016 Jonathan Eyolfson
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
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

find_package(PkgConfig)

pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
pkg_check_modules(WAYLAND_SCANNER REQUIRED wayland-scanner)

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h
	COMMAND wayland-scanner
	ARGS client-header
	     ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v5.xml
	     ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h
)

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.c
	COMMAND wayland-scanner
	ARGS code
	     ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v5.xml
	     ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.c
)

include_directories(
	${CMAKE_BINARY_DIR}
	${WAYLAND_CLIENT_INCLUDE_DIRS}
)

add_executable(nes-emulator
	main.c

	args.c
	cartridge.c
	console.c
	cpu.c
	exit_code.c
	ppu.c
	ppu_register.c
	backend/wayland.c
	backend/wayland_buffer.c

	${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.c
	${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h
)

target_link_libraries(nes-emulator
	${WAYLAND_CLIENT_LIBRARIES}
)
