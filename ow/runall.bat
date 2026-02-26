@echo off
setlocal

set outputfile=runall_test.txt
echo %date% %time% >%outputfile%

rem FAILED tests: tmmap (watcom tries to allocate at a given address)
rem               tdir (watcom's syscall wrapper fails when the string is returned. it's confused)
rem               mm, tao, ttypes, nantst, tatomic, tregex (watcom can't compile it)

set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 ^
             tstr fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex pis lenum ^
             trename nqueens fopentst fact triangle mm_old hidave ^
             termiosf mandle

( for %%a in (%_applist%) do (
    echo %%a
    echo %%a>>%outputfile%
    copy ..\c_tests\%%a.c . > NUL 2>&1
    call m.bat %%a
    ..\x32os %%a.elf >>%outputfile%
    del %%a.c > NUL 2>&1
    del %%a.obj > NUL 2>&1
    del %%a.elf > NUL 2>&1
    ) )
) )

echo %date% %time% >>%outputfile%
dos2unix %outputfile% > NUL 2>&1
diff -b baseline_%outputfile% %outputfile%

