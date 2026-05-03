@echo off
setlocal

set outputfile=runall_test.txt
echo %date% %time% >%outputfile%

set _applist=sieve e ttt mm chk

( for %%a in (%_applist%) do (
    echo %%a
    echo %%a>>%outputfile%
    for %%o in (1 2 3) do (
        ..\x32os x32bin%%o\%%a >>%outputfile% 2>&1
        ..\x64os bin%%o\%%a >>%outputfile% 2>&1
    )
) )

echo %date% %time% >>%outputfile%
dos2unix %outputfile%
diff -b baseline_%outputfile% %outputfile%


