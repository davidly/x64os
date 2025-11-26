@echo off
setlocal

if "%1" == "nested" (
  set _x64osruncmd=..\x64os -h:120 .\x64os.elf
)

if "%1" == "armos" (
  set _x64osruncmd=..\..\armos\armos -h:120 ..\..\armos\bin\x64os
)

if "%1" == "rvos" (
  set _x64osruncmd=..\..\rvos\rvos -h:120 ..\..\rvos\linux\x64os
)

if "%1" == "m68" (
  set _x64osruncmd=..\..\m68\m68 -h:120 ..\..\m68\x64os\x64os.elf
)

if "%1" == "gnu" (
  set _x64osruncmd=..\x64osg
)

if "%1" == "clang" (
  set _x64osruncmd=..\x64oscl
)

if "%_x64osruncmd%" == "" (
  set _x64osruncmd=..\x64os
)

set outputfile=test_x64os.txt
echo %date% %time% >%outputfile%

set _elflist=e td ttt tmm ato fileops real tphi mysort tap

set _folderlist=bin0 bin1 bin2 bin3

( for %%a in (%_elflist%) do (
    echo test %%a
    ( for %%f in (%_folderlist%) do (
        echo test %%f/%%a >>%outputfile%
        %_x64osruncmd% %%f\%%a >>%outputfile%
    ))
))

echo %date% %time% >>%outputfile%
diff baseline_%outputfile% %outputfile%

:eof

