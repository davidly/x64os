@echo off

rem: the msvc compiler produces a generally-working x64os, but "long double" is just 8 bytes not 10, so their usage will give unexpected results

cl /DX64OS /nologo x64os.cxx x64.cxx /I. /EHsc /DDEBUG /O2 /Oi /Fa /Qpar /Zi /link /OPT:REF user32.lib


