#!/bin/bash

apps=("tcmp" "t" "e" "printint" "sieve" "simple" "tmuldiv" "tpi" "ts" "tarray" "tbits" "trw" "trw2" "tmmap" "tstr" \
      "tdir" "fileops" "ttime" "tm" "glob" "tap" "tsimplef" "tphi" "tf" "ttt" "td" "terrno" "t_setjmp" "tex" \
      "tprintf" "pis" "mm" "tao" "ttypes" "nantst" "sleeptm" "tatomic" "lenum" "tregex" "trename" \
      "nqueens" "ff" "an" "ba fopentst fact triangle mm_old hidave targs tgets tscas")

for arg in ${apps[@]}
do
    echo $arg
    for optflag in 0 1 2 3 fast;
    do
        mkdir x32bin"$optflag" 2>/dev/null
        mkdir x32clangbin"$optflag" 2>/dev/null
        mkdir /mnt/c/users/david/onedrive/x64os/c_tests/x32bin"$optflag" 2>/dev/null
        mkdir /mnt/c/users/david/onedrive/x64os/c_tests/x32clangbin"$optflag" 2>/dev/null

        _clangbuild="clang-18 -m32 -x c++ "$arg".c -o x32clangbin"$optflag"/"$arg" -O"$optflag" -static -Wno-implicit-const-int-float-conversion -fsigned-char -Wno-format -Wno-format-security -std=c++14 -lm -lstdc++"
        _gnubuild="g++ "$arg".c -m32 -o x32bin"$optflag"/"$arg" -O"$optflag" -static -fsigned-char -Wno-format -Wno-format-security"

        if [ "$optflag" != "fast" ]; then
            $_clangbuild &
            $_gnubuild &
        else    
            $_clangbuild
            $_gnubuild
        fi
    done
done

echo "Waiting for compilation to complete..."
wait

echo "Generating assembly listings..."
for arg in ${apps[@]}
do
    for optflag in 0 1 2 3 fast;
    do
        objdump -d x32bin"$optflag"/"$arg" > x32bin"$optflag"/"$arg".txt &
        objdump -d x32clangbin"$optflag"/"$arg" > x32clangbin"$optflag"/"$arg".txt &
    done
done

echo "Waiting for assembly listings to complete..."
wait
