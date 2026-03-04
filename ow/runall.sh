#!/bin/bash

outputfile="runall_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 \
           tstr fileops ttime tm glob tap tsimplef tphi tf ttt td terrno \
           t_setjmp tex pis lenum \
           trename nqueens fopentst fact triangle mm_old hidave \
           termiosf mandle;
do
    echo $arg
    echo $arg >>$outputfile
    cp ../c_tests/$arg.c .
    ./mn.sh $arg
    ../x32os $arg.elf >>$outputfile
    rm $arg.c > /dev/null 2>&1
    rm $arg.o > /dev/null 2>&1
    rm $arg.map > /dev/null 2>&1
    rm $arg.elf > /dev/null 2>&1
done

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
