@echo off
setlocal

set outputfile=runall_test.txt
echo %date% %time% >"%outputfile%"

rem FAILED tests: mm, tao, ttypes, nantst, tatomic, tregex (watcom can't compile it)
rem               an, ba fail in compilation, have missing code, or with bad code at execution time

set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 ^
             tstr fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex pis lenum tdir ^
             trename nqueens fopentst fact triangle mm_old hidave ^
             termiosf mandle tmmap ff targs tgets

( for %%a in (%_applist%) do (
    echo %%a
    echo %%a>>"%outputfile%"
    copy ..\c_tests\%%a.c . >NUL 2>&1
    call mn.bat %%a

    if "%%a"=="ff" (
        ..\x32os %%a.elf . %%a.c >>"%outputfile%"
    ) else if "%%a"=="targs" (
        ..\x32os "-e:solo=iu;group=i-dle" %%a.elf a bb ccc dddd >>"%outputfile%"
    ) else if "%%a"=="tgets" (
        ..\x32os %%a.elf <..\c_tests\tgets.txt >>"%outputfile%"
    ) else (
        ..\x32os %%a.elf >>"%outputfile%"
    )

    del %%a.c >NUL 2>&1
    del %%a.o >NUL 2>&1
    del %%a.map >NUL 2>&1
    del %%a.elf >NUL 2>&1
) )

echo %date% %time% >>"%outputfile%"
dos2unix "%outputfile%" >NUL 2>&1
diff -b "baseline_%outputfile%" "%outputfile%"