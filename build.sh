#!/bin/bash
set -e

mkdir -p bin
gcc $PROGRAM_COMPILER_FLAGS -Iinclude src/terminal.c -o bin/terminal.o
nm bin/terminal.o > bin/terminal.sym
ld $PROGRAM_LINKER_FLAGS bin/terminal.o -o bin/terminal.nxe
