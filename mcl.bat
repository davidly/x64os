@echo off
setlocal
path=c:\program files\microsoft visual studio\2022\community\vc\tools\llvm\x64\bin;%path%

rem compile with -O3 not -Ofast so NaN works

rem using -mlong-double-80 produces binaries that expose multiple bugs in the clang++ implementation of 80-bit long doubles. avoid it here.
rem clang++ -mlong-double-80 -DX64OS -Wno-psabi -I . -x c++ x64os.cxx x64.cxx -o x64os.exe -O3 -static -fsigned-char -Wno-format -std=c++14 -Wno-deprecated-declarations -luser32.lib

clang++ -DX64OS -Wno-psabi -I . -x c++ x64os.cxx x64.cxx -o x64os.exe -O3 -static -fsigned-char -Wno-format -std=c++14 -Wno-deprecated-declarations -luser32.lib
