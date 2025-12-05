@echo off
setlocal

rem -O2 generates faster code than -O3 perhaps because smaller code means better cache utility

g++ -O2 -ggdb -D X64OS -D _MSC_VER x64os.cxx x64.cxx -I ../djl -D NDEBUG -o x64os.exe -static

g++ -O2 -ggdb -D X64OS -D _MSC_VER x64.cxx -I ../djl -D NDEBUG -S -fverbose-asm -o x64.s -static



