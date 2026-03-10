@echo off
setlocal

g++ -O3 -ggdb -D X32OS -mfpmath=387 -D _MSC_VER x64os.cxx x64.cxx -I ../djl -D NDEBUG -o x32os.exe -static

rem g++ -O2 -ggdb -D X32OS -D _MSC_VER x64.cxx -I ../djl -D NDEBUG -S -fverbose-asm -o x32.s -static

