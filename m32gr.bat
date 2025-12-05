@echo off
setlocal

g++ -O2 -ggdb -D X32OS -D _MSC_VER x64os.cxx x64.cxx -I ../djl -D NDEBUG -o x32os.exe -static


