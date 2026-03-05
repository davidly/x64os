@echo off
setlocal

set outputfile=runems_test.txt
echo %date% %time% >%outputfile%

set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 ^
             tstr fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex pis lenum ^
             trename nqueens fopentst fact triangle mm_old hidave ^
             termiosf mandle

( for %%a in (%_applist%) do (
    echo %%a
    echo %%a>>%outputfile%
    copy ..\c_tests\%%a.c . > NUL 2>&1
    call mn.bat %%a

    echo x64os %%a>>%outputfile%
    ..\x64os -h:100 ..\bin\x32os %%a.elf >>%outputfile%

    echo x32os %%a>>%outputfile%
    ..\x32os -h:100 ..\x32bin\x32os %%a.elf >>%outputfile%

    echo rvos %%a>>%outputfile%
    ..\..\rvos\rvos -h:100 ..\..\rvos\bin\x32os %%a.elf >>%outputfile%

    echo armos %%a>>%outputfile%
    ..\..\armos\armos -h:100 ..\..\armos\bin\x32os %%a.elf >>%outputfile%

    echo m68 %%a>>%outputfile%
    ..\..\m68\m68 -h:100 ..\..\m68\bin\x32os %%a.elf >>%outputfile%

    echo sparcos %%a>>%outputfile%
    ..\..\sparcos\sparcos -h:100 ..\..\sparcos\bin\x32os %%a.elf >>%outputfile%

    del %%a.c > NUL 2>&1
    del %%a.o > NUL 2>&1
    del %%a.obj > NUL 2>&1
    del %%a.map > NUL 2>&1
    del %%a.elf > NUL 2>&1
    ) )
) )

echo %date% %time% >>%outputfile%
dos2unix %outputfile% > NUL 2>&1
diff -b baseline_%outputfile% %outputfile%

