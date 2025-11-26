#!/bin/bash

for optflag in 0 1 2 3 fast;
do
    mkdir /mnt/c/users/david/onedrive/x64os/c_tests/bin"$optflag" 2>/dev/null
    mkdir /mnt/c/users/david/onedrive/x64os/c_tests/clangbin"$optflag" 2>/dev/null

    cp bin"$optflag"/* /mnt/c/users/david/onedrive/x64os/c_tests/bin"$optflag" &
    cp clangbin"$optflag"/* /mnt/c/users/david/onedrive/x64os/c_tests/clangbin"$optflag" &
done

echo "Waiting for all processes to complete..."
wait
