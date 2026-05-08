@echo off
setlocal

set _applist=sieve e ttt mm chk tap tphi nqueens nq1d

( for %%a in (%_applist%) do (
    echo %%a
    call m.bat %%a
    call m32.bat %%a
) )
