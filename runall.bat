@echo off
setlocal

if "%1" == ""       (set _runcmd=x64os -h:100 )
if "%1" == "x32"    (set _runcmd=x64osx32 -h:100 )
if "%1" == "nested" (set _runcmd=x64os -h:160 bin\x64os -h:100 )
if "%1" == "armos"  (set _runcmd=..\armos\armos -h:160 ..\armos\bin\x64os -h:100 )
if "%1" == "rvos"   (set _runcmd=..\rvos\rvos -h:160 ..\rvos\linux\x64os -h:100 )
if "%1" == "x32os"  (set _runcmd=x32os -h:160 x32bin\x64os -h:100 )
if "%1" == "m68"    (set _runcmd=..\m68\m68 -h:160 ..\m68\x64os\x64os -h:100 )
if "%1" == "sparcos" (set _runcmd=..\sparcos\sparcos -h:160 ..\sparcos\bin\x64os-sparc.elf -h:100 )

set outputfile=runall_test.txt
echo %date% %time% >"%outputfile%"

set _folderlist=bin0 bin1 bin2 bin3 binfast
set _optlist=6 8 a d 3 i I m o r x

set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 ^
             tmmap tstr tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex mm tao pis ttypes nantst sleeptm tatomic lenum ^
             tregex trename nqueens fopentst fact triangle mm_old hidave tscas tpopcnt ^
             termiosf mandle an ba ff tgets targs taux

( for %%a in (%_applist%) do (
    echo %%a
    ( for %%f in (%_folderlist%) do (
        if "%%a"=="an" (
            echo c_tests/%%f/%%a david lee>>"%outputfile%"
            %_runcmd% c_tests\%%f\%%a david lee >>"%outputfile%"
            echo c_tests/clang%%f/%%a david lee>>"%outputfile%"
            %_runcmd% c_tests\clang%%f\%%a david lee >>"%outputfile%"
        ) else if "%%a"=="ba" (
            echo c_tests/%%f/%%a c_tests/tp.bas>>"%outputfile%"
            %_runcmd% c_tests\%%f\%%a c_tests\tp.bas >>"%outputfile%"
            echo c_tests/clang%%f/%%a c_tests/tp.bas>>"%outputfile%"
            %_runcmd% c_tests\clang%%f\%%a c_tests\tp.bas >>"%outputfile%"
            ( for %%o in (%_optlist%) do (
                %_runcmd% c_tests\%%f\%%a -a:%%o -x c_tests\tp.bas >>"%outputfile%"
                %_runcmd% c_tests\clang%%f\%%a -a:%%o -x c_tests\tp.bas >>"%outputfile%"
            ) )
        ) else if "%%a"=="ff" (
            echo test c_tests/%%f/%%a . %%a.c>>"%outputfile%"
            %_runcmd% c_tests\%%f\%%a . %%a.c >>"%outputfile%"
            echo test c_tests/clang%%f/%%a . %%a.c>>"%outputfile%"
            %_runcmd% c_tests\clang%%f\%%a . %%a.c >>"%outputfile%"
        ) else if "%%a"=="tgets" (
            echo test c_tests/%%f/%%a>>"%outputfile%"
            %_runcmd% c_tests\%%f\%%a <c_tests\tgets.txt >>"%outputfile%"
            echo test c_tests/clang%%f/%%a>>"%outputfile%"
            %_runcmd% c_tests\clang%%f\%%a <c_tests\tgets.txt >>"%outputfile%"
        ) else if "%%a"=="targs" (
            echo test c_tests/%%f/%%a>>"%outputfile%"
            %_runcmd% "-e:solo=iu;group=i-dle" c_tests\%%f\%%a a bb ccc dddd >>"%outputfile%"
            echo test c_tests/clang%%f/%%a>>"%outputfile%"
            %_runcmd% "-e:solo=iu;group=i-dle" c_tests\clang%%f\%%a a bb ccc dddd >>"%outputfile%"
        ) else (
            echo c_tests/%%f/%%a>>"%outputfile%"
            %_runcmd% c_tests\%%f\%%a >>"%outputfile%"
            echo c_tests/clang%%f/%%a>>"%outputfile%"
            %_runcmd% c_tests\clang%%f\%%a >>"%outputfile%"
        )
    ) )
) )

set _sapplist=e_x64 sieve_x64 tttu_x64 xlat64 incdec64 pushpop64 string64 jmpcall64 sse2_64 ^
              muldiv64 tfildstp bt_bts64 rotate64
( for %%a in (%_sapplist%) do (
    echo %%a
    echo c_tests/%%a>>"%outputfile%"
    %_runcmd% c_tests\%%a.elf >>"%outputfile%"
) )

set _rustlist=e td ttt fileops ato tap real tphi mysort tmm
set _rustfolders=bin0 bin1 bin2 bin3

( for %%a in (%_rustlist%) do (
    echo %%a
    ( for %%f in (%_rustfolders%) do (
        echo rust_tests/%%f/%%a>>"%outputfile%"
        %_runcmd% rust_tests\%%f\%%a >>"%outputfile%"
    ) )
) )

set _fortranlist=e sieve ttt primes mm
( for %%a in (%_fortranlist%) do (
    echo %%a
    echo f_tests/bin/%%a>>"%outputfile%"
    %_runcmd% f_tests\bin\%%a >>"%outputfile%"
) )

echo %date% %time% >>"%outputfile%"
dos2unix "%outputfile%"
diff -b "baseline_%outputfile%" "%outputfile%"
