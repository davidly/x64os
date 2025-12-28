#!/bin/bash
#set -x

if [ "$1" == "" ]; then
    echo "Usage: m.sh <sourcefile>"
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

for optflag in 0 1 2 3 fast;
do
    mkdir x32bin"$optflag" 2>/dev/null
    mkdir x32clangbin"$optflag" 2>/dev/null

    _clangbuild="clang-18 -x c++ "$1".c -m32 -mpopcnt -msse2 -o x32clangbin"$optflag"/"$1" -O"$optflag" -static -Wno-implicit-const-int-float-conversion -fsigned-char -Wno-format -Wno-format-security -std=c++14 -lm -lstdc++"
    _gnubuild="g++ "$1".c -m32 -mpopcnt -msse2 -U_FORTIFY_SOURCE -o x32bin"$optflag"/"$1" -O"$optflag" -static -fsigned-char -Wno-format -Wno-format-security"

    if [ "$optflag" != "fast" ]; then
        $_clangbuild &
        $_gnubuild &
    else    
        $_clangbuild
        $_gnubuild
    fi
done

echo "Waiting for compilation to complete..."
wait

for optflag in 0 1 2 3 fast;
do
    objdump -d x32bin"$optflag"/"$1" > x32bin"$optflag"/"$1".txt &
    objdump -d x32clangbin"$optflag"/"$1" > x32clangbin"$optflag"/"$1".txt &
done

echo "Waiting for assembly listings to complete..."
wait
