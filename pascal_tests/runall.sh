#!/bin/bash

outputfile="runall_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

apps=(sieve e ttt mm chk tap tphi nqueens nq1d)

for arg in ${apps[@]}
do
    echo $arg
    echo $arg>>$outputfile
    for OPT in 1 2 3; do
        ../x32os x32bin${OPT}/$arg >>$outputfile 2>&1
        ../x64os bin${OPT}/$arg >>$outputfile 2>&1
    done
done

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
