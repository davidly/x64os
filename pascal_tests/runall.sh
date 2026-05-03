#!/bin/bash

outputfile="runall_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

apps=(sieve e ttt mm chk)

for arg in ${apps[@]}
do
    echo $arg
    echo $arg>>$outputfile
    ../x32os x32bin/$arg >>$outputfile 2>&1
    ../x64os bin/$arg >>$outputfile 2>&1
done

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
