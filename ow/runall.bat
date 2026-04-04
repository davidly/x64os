@echo off
setlocal

set outputfile=runall_test.txt
echo %date% %time% >%outputfile%

rem FAILED tests: tdir (watcom's syscall wrapper fails when the string is returned. it's confused)
rem               mm, tao, ttypes, nantst, tatomic, tregex (watcom can't compile it)
rem               ff, an, ba all fail in compilation, have missing code, or with bad code at execution time

set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 ^
             tstr fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex pis lenum ^
             trename nqueens fopentst fact triangle mm_old hidave ^
             termiosf mandle tmmap

( for %%a in (%_applist%) do (
    echo %%a
    echo %%a>>%outputfile%
    copy ..\c_tests\%%a.c . > NUL 2>&1
    call mn.bat %%a
    ..\x32os %%a.elf >>%outputfile%
    del %%a.c > NUL 2>&1
    del %%a.obj > NUL 2>&1
    del %%a.o > NUL 2>&1
    del %%a.map > NUL 2>&1
    del %%a.elf > NUL 2>&1
) )

echo %date% %time% >>%outputfile%
dos2unix %outputfile% > NUL 2>&1
diff -b baseline_%outputfile% %outputfile%

