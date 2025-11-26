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

set _elflist=hidave sieve e ttt tstr tm triangle fact t_setjmp targs ^
             fopentst trw trw2 tarray tmuldiv tpi tprintf tap tbits ^
             tphi nqueens terrno tcmp pis trename fileops lenum td ^
             ts tex glob tmmap tatomic tsimplef tregex ttime sleeptm ^
             tf mm_old mm tao ttypes tdir nantst

set _folderlist=bin0 bin1 bin2 bin3 binfast

( for %%a in (%_elflist%) do (
    echo test %%a
    ( for %%f in (%_folderlist%) do (
        echo test %%f/%%a >>%outputfile%
        %_x64osruncmd% %%f\%%a >>%outputfile%
    ))
))

echo test AN
( for %%f in (%_folderlist%) do (
    echo %%f/an david lee>>%outputfile%
    %_x64osruncmd% %%f\an david lee >>%outputfile%
) )

echo test BA
set _optlist=6 8 a d 3 i I m o r x

( for %%f in (%_folderlist%) do (
    echo %%f/ba c_tests/tp.bas>>%outputfile%
    %_x64osruncmd% %%f\ba tp.bas >>%outputfile%
    ( for %%o in (%_optlist%) do (
        %_x64osruncmd% %%f\ba -a:%%o -x tp.bas >>%outputfile%
    ) )
) )

echo test ff
( for %%f in (%_folderlist%) do (
    echo test %%f/ff >>%outputfile%
    %_x64osruncmd% %%f\ff -i . ff.c >>%outputfile%
))

set _s_elflist=e_x64 sieve_x64 tttu_x64

( for %%a in (%_s_elflist%) do (
    echo test %%a
    echo test %%a >>%outputfile%
    %_x64osruncmd% %%a >>%outputfile%
))

echo %date% %time% >>%outputfile%
diff baseline_%outputfile% %outputfile%

:eof

