# Copyright 2016 Jonathan Eyolfson

cmake_minimum_required(VERSION 3.1.3)

project(NES_EMULATOR C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wextra)

add_executable(nes-emulator
	main.c
)
