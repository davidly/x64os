@echo off
setlocal

g++ -O2 -ggdb -D X64OS -D _MSC_VER x64os.cxx x64.cxx -I ../djl -D DEBUG -o x64os.exe -static


