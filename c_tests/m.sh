#!/bin/bash
#set -x

if [ "$1" == "" ]; then
    echo "Usage: m.sh <sourcefile> [optlevel]"
    echo "  optlevel: 0, 1, 2 (default), 3, fast"
    exit 1
fi

if ! test -f "$1.c"; then
    echo "File $1.c not found!"
    exit 1
fi

if [ "$2" == "" ]; then
    optlevel="3"
else
    optlevel="$2"
fi

mkdir bin"$optlevel" 2>/dev/null

gcc -x c++ -fcf-protection=none -U_FORTIFY_SOURCE -O$optlevel $1.c -o bin$optlevel/$1.elf -lm -l:libstdc++.a -static -fsigned-char -Wno-format -Wno-format-security

# generate s file for reference
#gcc -x c++ -fcf-protection=none -U_FORTIFY_SOURCE -O$optlevel -S -fverbose-asm $1.c -o bin$optlevel/$1.s -fsigned-char -Wno-format -Wno-format-security

# generate disassembly
objdump -d bin$optlevel/$1.elf >bin$optlevel/$1.txt
cp $1.c /mnt/c/users/david/onedrive/x64os/c_tests
cp bin$optlevel/$1.* /mnt/c/users/david/onedrive/x64os/c_tests/bin$optlevel


