#!/bin/bash

outputfile="runems_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile


echo ====== ntvao test >>$outputfile
x64os bin/ntvao -a:0x1030 -c -p bin/tttaztec.hex >>$outputfile
# x64os bin/ntvao -a:400 -p -c -h bin/6502_functional_test.hex >>$outputfile

echo ====== ntvcm test >>$outputfile
x64os bin/ntvcm -p -c bin/tttcpm.com 10 >>$outputfile

echo ====== ntvdm test >>$outputfile
x64os bin/ntvdm -p -c bin/ttt8086.com 10 >>$outputfile

echo ====== rvos test >>$outputfile
x64os -h:100 bin/rvos -p bin/ttt_rvu.elf 10 >>$outputfile

echo ====== armos test >>$outputfile
x64os -h:100 bin/armos -p bin/tttu_arm 10 >>$outputfile

echo ====== m68 test >>$outputfile
x64os -h:100 bin/m68 -p bin/ttt68u 10 >>$outputfile

echo ====== sparcos test >>$outputfile
x64os -h:100 bin/sparcos -p bin/tttusp.elf 10 >>$outputfile

echo ====== x64os test >>$outputfile
x64os -h:100 bin/x64os -p bin/tttu_x64.elf 10 >>$outputfile

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
