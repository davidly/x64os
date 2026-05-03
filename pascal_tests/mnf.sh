#!/bin/bash
# Check if a filename was provided
if [ -z "$1" ]; then
    echo "Usage: $0 <filename_without_extension>"
    exit 1
fi

# 1. Define the backend compiler path (adjust if necessary)
PPC_BIN=$(which ppcx64)

# 2. Compile using internal tools to avoid forking
# -Aelf: Use internal ELF writer (assembler)
# -Xi:   Use internal linker
# -g:    Generate debug info
# -O2:   Level 2 optimization
# -o:    Specify output destination

# /usr/bin/ppcx64
# /etc/fpc.cfg

rm bin/$1 > /dev/null 2>&1
rm bin/$1.o > /dev/null 2>&1
../x64os /usr/bin/ppcx64 -Aelf -g -O2 -obin/$1 $1.pas
rm bin/$1.o > /dev/null 2>&1

#../x64os -t $PPC_BIN -vt -n \
#                        -Fu/usr/lib/fpc/3.2.2/units/x86_64-linux/rtl \
#                        Tlinux -Px86_64 --Aelf -Xi -g -O2 -obin/"$1" "$1"
