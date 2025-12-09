@echo off
setlocal

set outputfile=runems_test.txt
echo %date% %time% >%outputfile%

echo ====== ntvao test>>%outputfile%
x64os -l bin\ntvao -a:0x1030 -c -p bin\tttaztec.hex >>%outputfile%
x32os -l x32bin\ntvao -a:0x1030 -c -p bin\tttaztec.hex >>%outputfile%
rem x64os bin\ntvao -a:400 -p -c -h bin\6502_functional_test.hex >>%outputfile%

echo ====== ntvcm test>>%outputfile%
x64os bin\ntvcm -p -c bin\tttcpm.com 10 >>%outputfile%
x32os x32bin\ntvcm -p -c bin\tttcpm.com 10 >>%outputfile%

echo ====== ntvdm test>>%outputfile%
x64os bin\ntvdm -p -c bin\ttt8086.com 10 >>%outputfile%
x32os x32bin\ntvdm -p -c bin\ttt8086.com 10 >>%outputfile%

echo ====== rvos test>>%outputfile%
x64os -h:100 bin\rvos -p bin\ttt_rvu.elf 10 >>%outputfile%
x32os -h:100 x32bin\rvos -p bin\ttt_rvu.elf 10 >>%outputfile%

echo ====== armos test>>%outputfile%
x64os -h:100 bin\armos -p bin\tttu_arm 10 >>%outputfile%
x32os -h:100 x32bin\armos -p bin\tttu_arm 10 >>%outputfile%

echo ====== m68 test>>%outputfile%
x64os -h:100 bin\m68 -p bin\ttt68u 10 >>%outputfile%
x32os -h:100 x32bin\m68 -p bin\ttt68u 10 >>%outputfile%

echo ====== sparcos test>>%outputfile%
x64os -h:100 bin\sparcos -p bin\tttusp.elf 10 >>%outputfile%
x32os -h:100 x32bin\sparcos -p bin\tttusp.elf 10 >>%outputfile%

echo ====== x64os test>>%outputfile%
x64os -h:100 bin\x64os -p bin\tttu_x64.elf 10 >>%outputfile%
x32os -h:100 x32bin\x64os -p bin\tttu_x64.elf 10 >>%outputfile%

echo ====== x32os test>>%outputfile%
x64os -h:100 bin\x32os -p bin\tttx32.elf 10 >>%outputfile%
xx32os -h:100 x32bin\x32os -p bin\tttx32.elf 10 >>%outputfile%

echo %date% %time% >>%outputfile%
dos2unix %outputfile%
diff baseline_%outputfile% %outputfile%

