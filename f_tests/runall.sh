#!/bin/bash

if [ "$1" = "nested" ]; then
    _x64osruncmd="../x64os -h:120 ../c_tests/x64os.elf"
fi

if [ "$1" = "armos" ]; then
    _x64osruncmd="../../ArmOS/armos -h:120 ../../ArmOS/bin/x64os"
fi

if [ "$1" = "rvos" ]; then
    _x64osruncmd="../../rvos/rvos -h:120 ../../rvos/bin/x64os.elf"
fi

if [ "$_x64osruncmd" = "" ]; then
    _x64osruncmd="../x64os"
fi

outputfile="test_x64os.txt"
date_time=$(date)
echo "$date_time" >$outputfile

for arg in primes sieve e ttt mm;
do
    echo $arg
    echo test $arg >>$outputfile
    $_x64osruncmd $arg.elf >>$outputfile
done

date_time=$(date)
echo "$date_time" >>$outputfile
unix2dos -f $outputfile

diff --ignore-all-space baseline_$outputfile $outputfile
