@echo off
setlocal

SET WINWATCOM=..\watcom_linux

SET WATCOM=../watcom_linux
SET EDPATH=%WATCOM%/EDDAT
SET INCLUDE=%WATCOM%/LH:%WATCOM%/H

set _env=WATCOM=%WATCOM%;PATH=%WATCOM%/binl64:%WATCOM%/binl;EDPATH=%EDPATH%;INCLUDE=%INCLUDE%

del %1.map 1>NUL 2>&1
del %1.o 1>NUL 2>&1
del %1.elf 1>NUL 2>&1

..\x32os -e:%_env% %WINWATCOM%\binl\wpp386 -q -oh -oi -ol+ -ei -ot -ombr -fp3 -3r -xs -xr -bt=linux -fo%1.o %1.c -DWATCOM -DNDEBUG
..\x32os -e:%_env% %WINWATCOM%\binl\wlink system linux name %1.elf file %1.o option quiet option stack=8192 option map


