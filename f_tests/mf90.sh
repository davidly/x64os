#!/bin/bash
#set -x

if [ "$1" == "" ]; then
    echo "Usage: m.sh <sourcefile> [optlevel]"
    echo "  optlevel: 0, 1, 2 (default), 3, fast"
    exit 1
fi

if [ "$2" == "" ]; then
    optlevel="2"
else
    optlevel="$2"
fi

gfortran -O$optlevel $1.f90 -o $1.elf -static
objdump -d $1.elf >$1.txt

cp $1.f90 /mnt/c/users/david/onedrive/x64os/f_tests
cp $1.elf /mnt/c/users/david/onedrive/x64os/f_tests
cp $1.txt /mnt/c/users/david/onedrive/x64os/f_tests
