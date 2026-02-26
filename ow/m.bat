@echo off
setlocal

SET WATCOM=..\..\OW
SET PATH=%WATCOM%\BINNT64;%WATCOM%\BINNT;%PATH%
SET EDPATH=%WATCOM%\EDDAT
SET INCLUDE=%WATCOM%\LH;%WATCOM%\H

wcl386 -q -3r -cc++ -xs -xr %1.c -bcl=LINUX -k65536 -fe=%1 -DWATCOM -DNDEBUG -I.


