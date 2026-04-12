@echo off
setlocal

set outputfile=runall_test.txt
echo %date% %time% >%outputfile%

rem FAILED tests: mm, tao, ttypes, nantst, tatomic, tregex (watcom can't compile it)
rem               ff, an, ba all fail in compilation, have missing code, or with bad code at execution time

set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 ^
             tstr fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex pis lenum tdir ^
             trename nqueens fopentst fact triangle mm_old hidave ^
             termiosf mandle tmmap

( for %%a in (%_applist%) do (
    echo %%a
    echo %%a>>%outputfile%
    copy ..\c_tests\%%a.c . > NUL 2>&1
    call mn.bat %%a
    ..\x32os %%a.elf >>%outputfile%
    del %%a.c > NUL 2>&1
    del %%a.o > NUL 2>&1
    del %%a.map > NUL 2>&1
    del %%a.elf > NUL 2>&1
) )

rem ff test
echo ff
echo ff>>%outputfile%
copy ..\c_tests\ff.c . > NUL 2>&1
call mn.bat ff
..\x32os ff.elf . ff.c>>%outputfile%
del ff.c > NUL 2>&1
del ff.o > NUL 2>&1
del ff.map > NUL 2>&1
del ff.elf > NUL 2>&1

rem targs test
echo targs
echo targs>>%outputfile%
copy ..\c_tests\targs.c . > NUL 2>&1
call mn.bat targs
..\x32os -e:solo=iu;group=i-dle targs.elf a bb ccc dddd >>%outputfile%
del targs.c > NUL 2>&1
del targs.o > NUL 2>&1
del targs.map > NUL 2>&1
del targs.elf > NUL 2>&1

rem tgets test
echo tgets
echo tgets>>%outputfile%
copy ..\c_tests\tgets.c . > NUL 2>&1
call mn.bat tgets
..\x32os tgets.elf <..\c_tests\tgets.txt >>%outputfile%
del tgets.c > NUL 2>&1
del tgets.o > NUL 2>&1
del tgets.map > NUL 2>&1
del tgets.elf > NUL 2>&1

echo %date% %time% >>%outputfile%
dos2unix %outputfile% > NUL 2>&1
diff -b baseline_%outputfile% %outputfile%

