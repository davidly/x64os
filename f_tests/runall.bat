@echo off
setlocal

if "%1" == "nested" (
  set _x64osruncmd=..\x64os -h:120 ..\c_tests\x64os.elf
)

if "%1" == "armos" (
  set _x64osruncmd=..\..\armos\armos -h:120 ..\..\armos\bin\x64os
)

if "%1" == "rvos" (
  set _x64osruncmd=..\..\rvos\rvos -h:120 ..\..\rvos\linux\x64os
)

if "%1" == "x64os" (
  set _x64osruncmd=..\..\x64os\x64os -h:120 ..\..\x64os\bin\x64os
)

if "%_x64osruncmd%" == "" (
  set _x64osruncmd=..\x64os
)

set outputfile=test_x64os.txt
echo %date% %time% >%outputfile%

set _elflist=primes sieve e ttt mm

( for %%a in (%_elflist%) do (
    echo test %%a
    echo test %%a>>%outputfile%
    %_x64osruncmd% %%a >>%outputfile%
))

echo %date% %time% >>%outputfile%
diff baseline_%outputfile% %outputfile%

:eof

