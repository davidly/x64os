#!/bin/bash

outputfile="runall_test.txt"
date_time=$(date)
echo "$date_time" > "$outputfile"

apps=(sieve e ttt mm chk tap tphi nqueens nq1d trw mandle tf tdir tcmp tstr \
      trename sleeptm tmuldiv lenum tpi pis tshift ff)

if [ "$1" = "native" ]; then
    x32cmd=""
    x64cmd=""
else
    x32cmd="../x32os"
    x64cmd="../x64os"
fi

for arg in "${apps[@]}"
do
    echo "$arg"
    echo "$arg" >> "$outputfile"

    for OPT in 1 2 3; do
        if [ "$arg" = "ff" ]; then
            echo test x32bin$OPT/$arg -i . $arg.pas >> "$outputfile" 2>&1
            $x32cmd "x32bin${OPT}/$arg" -i . $arg.pas >> "$outputfile" 2>&1
            echo test bin$OPT/$arg -i . $arg.pas >> "$outputfile" 2>&1
            $x64cmd "bin${OPT}/$arg" -i . $arg.pas >> "$outputfile" 2>&1
        else
            echo test x32bin$OPT/$arg >> "$outputfile" 2>&1
            $x32cmd "x32bin${OPT}/$arg" >> "$outputfile" 2>&1
            echo test bin$OPT/$arg >> "$outputfile" 2>&1
            $x64cmd "bin${OPT}/$arg" >> "$outputfile" 2>&1
        fi
    done
done

date_time=$(date)
echo "$date_time" >> "$outputfile"

diff "baseline_$outputfile" "$outputfile"
