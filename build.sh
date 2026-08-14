#!/bin/bash
set -e

mkdir -p bin
x86_64-linux-gnu-gcc $PROGRAM_COMPILER_FLAGS -isystem sys -g -fno-omit-frame-pointer -O2 -fvect-cost-model=dynamic -Wall -Wextra -Werror src/terminal.c -o bin/terminal.o
x86_64-linux-gnu-nm bin/terminal.o > bin/terminal.sym
x86_64-linux-gnu-ld $PROGRAM_LINKER_FLAGS bin/terminal.o -o bin/terminal.nxe
