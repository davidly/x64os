#!/bin/bash

_x64oscmd="x64os"

if [ "$1" = "nested" ]; then
    _x64oscmd="x64os -h:200 bin/x64os"
elif [ "$1" = "native" ]; then
    _x64oscmd=""
elif [ "$1" = "x64oscl" ]; then
    _x64oscmd="x64oscl -h:200"
elif [ "$1" = "m68" ]; then
    _x64oscmd="../m68/m68 -h:200 ../m68/x64os/x64os -h:100"
elif [ "$1" = "sparcos" ]; then
    _x64oscmd="../sparcos/sparcos -h:200 ../sparcos/bin/x64os-sparc.elf -h:100"
elif [ "$1" = "rvos" ]; then
    _x64oscmd="../rvos/rvos -h:200 ../rvos/bin/x64os -h:100"
elif [ "$1" = "rvoscl" ]; then
    _x64oscmd="../rvos/rvoscl -h:200 ../rvos/bin/x64os -h:100"
fi    

outputfile="runall_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 \
             tmmap tstr tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno \
             t_setjmp tex mm tao pis ttypes nantst sleeptm tatomic lenum \
             tregex trename nqueens fopentst fact triangle mm_old hidave;

do
    echo $arg
    for opt in 0 1 2 3 fast;
    do
        echo c_tests/bin$opt/$arg >>$outputfile
        $_x64oscmd c_tests/bin$opt/$arg >>$outputfile
        echo c_tests/clangbin$opt/$arg >>$outputfile
        $_x64oscmd c_tests/clangbin$opt/$arg >>$outputfile
    done
done

for arg in e_x64 sieve_x64 tttu_x64;
do
    echo $arg
    echo c_tests/$arg>>$outputfile
    $_x64oscmd c_tests/$arg.elf>>$outputfile
done

echo test AN
for opt in 0 1 2 3 fast;
do
    echo c_tests/bin$opt/an david lee >>$outputfile
    $_x64oscmd c_tests/bin$opt/an david lee >>$outputfile
    echo c_tests/clangbin$opt/an david lee >>$outputfile
    $_x64oscmd c_tests/clangbin$opt/an david lee >>$outputfile
done

echo test BA
for opt in 0 1 2 3 fast;
do
    echo c_tests/bin$opt/ba c_tests/tp.bas >>$outputfile
    $_x64oscmd c_tests/bin$opt/ba c_tests/tp.bas >>$outputfile
    for codegen in 6 8 a d 3 i I m o r x;
    do
        $_x64oscmd c_tests/bin$opt/ba -a:$codegen -x c_tests/tp.bas >>$outputfile
        $_x64oscmd c_tests/clangbin$opt/ba -a:$codegen -x c_tests/tp.bas >>$outputfile
    done
done

echo test ff . ff.c
for opt in 0 1 2 3 fast;
do
    echo test c_tests/bin$opt/ff . ff.c >>$outputfile
    $_x64oscmd c_tests/bin$opt/ff . ff.c >>$outputfile
    echo test c_tests/clangbin$opt/ff . ff.c >>$outputfile
    $_x64oscmd c_tests/clangbin$opt/ff . ff.c >>$outputfile
done

for arg in e td ttt fileops ato tap real tphi mysort tmm;
do
    echo $arg
    for opt in 0 1 2 3;
    do
        echo rust_tests/bin$opt/$arg >>$outputfile
        $_x64oscmd rust_tests/bin$opt/$arg >>$outputfile
    done
done

for arg in e sieve ttt primes mm
do
	echo $arg
        echo f_tests/$arg >>$outputfile
   $_x64oscmd f_tests/$arg >>$outputfile
done

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
