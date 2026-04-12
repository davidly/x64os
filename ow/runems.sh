#!/bin/bash

outputfile="runems_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 \
           tstr fileops ttime tm glob tap tsimplef tphi tf ttt td terrno \
           t_setjmp tex pis lenum tdir \
           trename nqueens fopentst fact triangle mm_old hidave \
           termiosf mandle tmmap;
do
    echo $arg
    echo $arg >>$outputfile
    cp ../c_tests/$arg.c .
    ./mn.sh $arg

    echo x64os $arg >>$outputfile
    ../x64os -h:100 ../bin/x32os $arg.elf >>$outputfile

    echo x32os $arg >>$outputfile
    ../x32os -h:100 ../x32bin/x32os $arg.elf >>$outputfile

    echo rvos $arg >>$outputfile
    ../../rvos/rvos -h:100 ../../rvos/bin/x32os $arg.elf >>$outputfile

    echo armos $arg >>$outputfile
    ../../armos/armos -h:100 ../../armos/bin/x32os $arg.elf >>$outputfile

    echo m68 $arg >>$outputfile
    ../../m68/m68 -h:100 ../../m68/bin/x32os $arg.elf >>$outputfile

    echo sparcos $arg >>$outputfile
    ../../sparcos/sparcos -h:100 ../../sparcos/bin/x32os $arg.elf >>$outputfile
    
    rm $arg.c > /dev/null 2>&1
    rm $arg.o > /dev/null 2>&1
    rm $arg.elf > /dev/null 2>&1
done

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
