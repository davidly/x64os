@echo off
setlocal

set outputfile=runems_test.txt
echo %date% %time% >%outputfile%

echo ====== ntvao test>>%outputfile%
x64os -h:20 -m:0 -l bin\ntvao -a:0x1030 -c -p bin\tttaztec.hex >>%outputfile%
x32os -h:20 -m:0 -l x32bin\ntvao -a:0x1030 -c -p bin\tttaztec.hex >>%outputfile%
rem x64os -h:20 -m:0 bin\ntvao -a:400 -p -c -h bin\6502_functional_test.hex >>%outputfile%

echo ====== ntvcm test>>%outputfile%
x64os -h:20 -m:0 bin\ntvcm -p -c bin\tttcpm.com 10 >>%outputfile%
x32os -h:20 -m:0 x32bin\ntvcm -p -c bin\tttcpm.com 10 >>%outputfile%

echo ====== ntvdm test>>%outputfile%
x64os -h:20 -m:0 bin\ntvdm -p -c bin\ttt8086.com 10 >>%outputfile%
x32os -h:20 -m:0 x32bin\ntvdm -p -c bin\ttt8086.com 10 >>%outputfile%

echo ====== rvos test>>%outputfile%
x64os -h:20 -m:0 bin\rvos -h:3 -m:0 -p bin\ttt_rvu.elf 10 >>%outputfile%
x32os -h:20 -m:0 x32bin\rvos -h:3 -m:0 -p bin\ttt_rvu.elf 10 >>%outputfile%

echo ====== armos test>>%outputfile%
x64os -h:20 -m:0 bin\armos -h:3 -m:0 -p bin\tttu_arm 10 >>%outputfile%
x32os -h:20 -m:0 x32bin\armos -h:3 -m:0 -p bin\tttu_arm 10 >>%outputfile%

echo ====== m68 test>>%outputfile%
x64os -h:20 -m:0 bin\m68 -h:3 -m:0 -p bin\ttt68u 10 >>%outputfile%
x32os -h:20 -m:0 x32bin\m68 -h:3 -m:0 -p bin\ttt68u 10 >>%outputfile%

echo ====== sparcos test>>%outputfile%
x64os -h:20 -m:0 bin\sparcos -h:3 -m:0 -p bin\tttusp.elf 10 >>%outputfile%
x32os -h:20 -m:0 x32bin\sparcos -h:3 -m:0 -p bin\tttusp.elf 10 >>%outputfile%

echo ====== x64os test>>%outputfile%
x64os -h:20 -m:0 bin\x64os -h:3 -m:0 -p bin\tttu_x64.elf 10 >>%outputfile%
x32os -h:20 -m:0 x32bin\x64os -h:3 -m:0 -p bin\tttu_x64.elf 10 >>%outputfile%

echo ====== x32os test>>%outputfile%
x64os -h:20 -m:0 bin\x32os -h:3 -m:0 -p bin\tttx32.elf 10 >>%outputfile%
x32os -h:20 -m:0 x32bin\x32os -h:3 -m:0 -p bin\tttx32.elf 10 >>%outputfile%

echo %date% %time% >>%outputfile%
dos2unix %outputfile%
diff baseline_%outputfile% %outputfile%

