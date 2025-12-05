@echo off
setlocal

if "%1" == "" (set _runcmd=x32os -h:100 )
if "%1" == "x32" (set _runcmd=x32osx32 -h:100 )
if "%1" == "nested" (set _runcmd=x32os -h:160 bin\x32os -h:100 )
if "%1" == "armos" (set _runcmd=..\armos\armos -h:160 ..\armos\bin\x32os -h:100 )
if "%1" == "rvos" (set _runcmd=..\rvos\rvos -h:160 ..\rvos\linux\x32os -h:100 )
if "%1" == "m68" (set _runcmd=..\m68\m68 -h:160 ..\m68\x32os\x32os -h:100 )
if "%1" == "sparcos" (set _runcmd=..\sparcos\sparcos -h:160 ..\sparcos\bin\x32os-sparc.elf -h:100 )

set outputfile=runall32_test.txt
echo %date% %time% >%outputfile%

set _folderprefix=x32
set _folderlist=bin0 bin1 bin2 bin3 binfast
set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 ^
             tmmap tstr tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex mm tao pis ttypes nantst sleeptm tatomic lenum ^
             tregex trename nqueens fopentst fact triangle mm_old hidave

( for %%a in (%_applist%) do (
    echo %%a
    ( for %%f in (%_folderlist%) do (
        echo c_tests/%_folderprefix%%%f/%%a>>%outputfile%
        %_runcmd% c_tests\%_folderprefix%%%f\%%a >>%outputfile%
        echo c_tests/%_folderprefix%clang%%f/%%a>>%outputfile%
        %_runcmd% c_tests\%_folderprefix%clang%%f\%%a >>%outputfile%
    ) )
) )

echo test AN
( for %%f in (%_folderlist%) do (
    echo c_tests/%_folderprefix%%%f/an david lee>>%outputfile%
    %_runcmd% c_tests\%_folderprefix%%%f\an david lee >>%outputfile%
    echo c_tests/%_folderprefix%clang%%f/an david lee>>%outputfile%
    %_runcmd% c_tests\%_folderprefix%clang%%f\an david lee >>%outputfile%
) )

echo test BA
set _optlist=6 8 a d 3 i I m o r x

( for %%f in (%_folderlist%) do (
    echo c_tests/%_folderprefix%%%f/ba c_tests/tp.bas>>%outputfile%
    %_runcmd% c_tests\\%_folderprefix%%%f\ba c_tests\tp.bas >>%outputfile%
    ( for %%o in (%_optlist%) do (
        %_runcmd% c_tests\%_folderprefix%%%f\ba -a:%%o -x c_tests\tp.bas >>%outputfile%
        %_runcmd% c_tests\%_folderprefix%clang%%f\ba -a:%%o -x c_tests\tp.bas >>%outputfile%
    ) )
) )

echo test ff . ff.c
( for %%f in (%_folderlist%) do (
    echo test c_tests/%_folderprefix%%%f/ff . ff.c>>%outputfile%
    %_runcmd% c_tests\%_folderprefix%%%f\ff . ff.c>>%outputfile%
    echo test c_tests/%_folderprefix%clang%%f/ff . ff.c>>%outputfile%
    %_runcmd% c_tests\%_folderprefix%clang%%f\ff . ff.c>>%outputfile%
) )

echo %date% %time% >>%outputfile%
dos2unix %outputfile%
diff baseline_%outputfile% %outputfile%


