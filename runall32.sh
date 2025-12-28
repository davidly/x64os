#!/bin/bash

_x32oscmd="x32os"

if [ "$1" = "nested" ]; then
    _x32oscmd="x32os -h:200 x32bin/x32os"
elif [ "$1" = "native" ]; then
    _x32oscmd=""
elif [ "$1" = "x32oscl" ]; then
    _x32oscmd="x32oscl -h:200"
elif [ "$1" = "x64os" ]; then
    _x32oscmd="x64os -h:200 bin/x32os -h:100"
elif [ "$1" = "m68" ]; then
    _x32oscmd="../m68/m68 -h:200 ../m68/x32os/x32os -h:100"
elif [ "$1" = "sparcos" ]; then
    _x32oscmd="../sparcos/sparcos -h:200 ../sparcos/bin/x32os-sparc.elf -h:100"
elif [ "$1" = "rvos" ]; then
    _x32oscmd="../rvos/rvos -h:200 ../rvos/bin/x32os -h:100"
elif [ "$1" = "rvoscl" ]; then
    _x32oscmd="../rvos/rvoscl -h:200 ../rvos/bin/x32os -h:100"
fi    

outputfile="runall32_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 \
             tmmap tstr tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno \
             t_setjmp tex mm tao pis ttypes nantst sleeptm tatomic lenum \
             tregex trename nqueens fopentst fact triangle mm_old hidave tscas tpopcnt;

do
    echo $arg
    for opt in 0 1 2 3 fast;
    do
        echo c_tests/x32bin$opt/$arg >>$outputfile
        $_x32oscmd c_tests/x32bin$opt/$arg >>$outputfile
        echo c_tests/x32clangbin$opt/$arg >>$outputfile
        $_x32oscmd c_tests/x32clangbin$opt/$arg >>$outputfile
    done
done

for arg in e_x32 sieve_x32 tttu_x32;
do
    echo $arg
    echo c_tests/$arg>>$outputfile
    $_x32oscmd c_tests/$arg.elf>>$outputfile
done

echo test AN
for opt in 0 1 2 3 fast;
do
    echo c_tests/x32bin$opt/an david lee >>$outputfile
    $_x32oscmd c_tests/x32bin$opt/an david lee >>$outputfile
    echo c_tests/x32clangbin$opt/an david lee >>$outputfile
    $_x32oscmd c_tests/x32clangbin$opt/an david lee >>$outputfile
done

echo test BA
for opt in 0 1 2 3 fast;
do
    echo c_tests/x32bin$opt/ba c_tests/tp.bas >>$outputfile
    $_x32oscmd c_tests/x32bin$opt/ba c_tests/tp.bas >>$outputfile
    for codegen in 6 8 a d 3 i I m o r x;
    do
        $_x32oscmd c_tests/x32bin$opt/ba -a:$codegen -x c_tests/tp.bas >>$outputfile
        $_x32oscmd c_tests/x32clangbin$opt/ba -a:$codegen -x c_tests/tp.bas >>$outputfile
    done
done

echo test ff . ff.c
for opt in 0 1 2 3 fast;
do
    echo test c_tests/x32bin$opt/ff . ff.c >>$outputfile
    $_x32oscmd c_tests/x32bin$opt/ff . ff.c >>$outputfile
    echo test c_tests/x32clangbin$opt/ff . ff.c >>$outputfile
    $_x32oscmd c_tests/x32clangbin$opt/ff . ff.c >>$outputfile
done

echo test tgets
for optflag in 0 1 2 3 fast;
do
    echo test c_tests/x32bin$optflag/tgets >>$outputfile
    $_x32oscmd c_tests/x32bin$optflag/tgets <c_tests/tgets.txt >>$outputfile
    echo test c_tests/x32clangbin$optflag/tgets >>$outputfile
    $_x32oscmd c_tests/x32clangbin$optflag/tgets <c_tests/tgets.txt >>$outputfile
done    

for arg in e sieve ttt primes mm
do
   echo $arg
     echo f_tests/x32bin/$arg >>$outputfile
    $_x32oscmd f_tests/x32bin/$arg >>$outputfile
done

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
