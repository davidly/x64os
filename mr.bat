@echo off
cl /DX64OS /W4 /wd4201 /wd4996 /nologo x64os.cxx x64.cxx /I. /EHsc /DNDEBUG /GS- /GL /Ot /Ox /Ob3 /Oi /Qpar /Zi /Fa /FAs /link /OPT:REF user32.lib



