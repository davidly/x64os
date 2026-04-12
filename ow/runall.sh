#!/bin/bash

outputfile="runall_test.txt"
date_time=$(date)
echo "$date_time" >"$outputfile"

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 \
           tstr fileops ttime tm glob tap tsimplef tphi tf ttt td terrno \
           t_setjmp tex pis lenum tdir \
           trename nqueens fopentst fact triangle mm_old hidave \
           termiosf mandle tmmap ff targs tgets
do
    echo "$arg"
    echo "$arg" >>"$outputfile"
    cp "../c_tests/$arg.c" .
    ./mn.sh "$arg"

    if [ "$arg" = "ff" ]; then
        ../x32os "$arg.elf" . "$arg.c" >>"$outputfile"
    elif [ "$arg" = "targs" ]; then
        ../x32os "-e:solo=iu;group=i-dle" "$arg.elf" a bb ccc dddd >>"$outputfile"
    elif [ "$arg" = "tgets" ]; then
        ../x32os "$arg.elf" <../c_tests/tgets.txt >>"$outputfile"
    else
        ../x32os "$arg.elf" >>"$outputfile"
    fi

    rm -f "$arg.c" "$arg.o" "$arg.map" "$arg.elf"
done

date_time=$(date)
echo "$date_time" >>"$outputfile"
diff "baseline_$outputfile" "$outputfile"
