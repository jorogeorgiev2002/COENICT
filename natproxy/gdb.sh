#!/bin/bash
# custom GDB start, since it does not work with esp-clang
export TERM=linux
xtensa-esp32s3-elf-gdb -x build/gdbinit/symbols -x build/gdbinit/connect
