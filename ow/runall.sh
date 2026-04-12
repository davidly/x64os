#!/bin/bash

outputfile="runall_test.txt"
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
    ../x32os $arg.elf >>$outputfile
    rm $arg.c > /dev/null 2>&1
    rm $arg.o > /dev/null 2>&1
    rm $arg.map > /dev/null 2>&1
    rm $arg.elf > /dev/null 2>&1
done

# ff test
echo ff
echo ff >>$outputfile
cp ../c_tests/ff.c .
./mn.sh ff
../x32os ff.elf . ff.c >>$outputfile
rm ff.c > /dev/null 2>&1
rm ff.o > /dev/null 2>&1
rm ff.map > /dev/null 2>&1
rm ff.elf > /dev/null 2>&1

# targs test
echo targs
echo targs >>$outputfile
cp ../c_tests/targs.c .
./mn.sh targs
../x32os "-e:solo=iu;group=i-dle" targs.elf a bb ccc dddd >>$outputfile
rm targs.c > /dev/null 2>&1
rm targs.o > /dev/null 2>&1
rm targs.map > /dev/null 2>&1
rm targs.elf > /dev/null 2>&1

# tgets test
echo tgets
echo tgets >>$outputfile
cp ../c_tests/tgets.c .
./mn.sh tgets
../x32os tgets.elf <../c_tests/tgets.txt >>$outputfile
rm tgets.c > /dev/null 2>&1
rm tgets.o > /dev/null 2>&1
rm tgets.map > /dev/null 2>&1
rm tgets.elf > /dev/null 2>&1

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
