@echo off
setlocal

rem -O2 with g++ 13.2.0 generates bad code that's revealed when running x32os built for linux (which then runs the Watcom compiler)
rem same for g++ 15.2.0

rem -O2 with -fno-store-merging -- passed
rem -O2 with -fno-strict-aliasing -- passed
rem g++ -O2 -ggdb -D X32OS -D _MSC_VER x64os.cxx x64.cxx -I ../djl -D DEBUG -o x32os.exe -static

rem -O3 results in a bigger binary but no such bug
g++ -O3 -ggdb -D X32OS -D _MSC_VER x64os.cxx x64.cxx -I ../djl -D DEBUG -o x32os.exe -static


