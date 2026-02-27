#!/bin/bash

outputfile="runems_test.txt"
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
    ./m.sh $arg

    ../x64os -h:100 ../bin/x32os $arg.elf >>$outputfile
    ../x32os -h:100 ../x32bin/x32os $arg.elf >>$outputfile
    ../../rvos/rvos -h:100 ../../rvos/bin/x32os $arg.elf >>$outputfile
    ../../armos/armos -h:100 ../../armos/bin/x32os $arg.elf >>$outputfile
    ../../m68/m68 -h:100 ../../m68/bin/x32os $arg.elf >>$outputfile
    ../../sparcos/sparcos -h:100 ../../sparcos/bin/x32os $arg.elf >>$outputfile
    
    rm $arg.c > /dev/null 2>&1
    rm $arg.o > /dev/null 2>&1
    rm $arg.elf > /dev/null 2>&1
done

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
