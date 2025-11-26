#!/bin/bash
#set -x

if [ "$1" == "" ]; then
    echo "Usage: pone.sh appname"
    exit 1
fi

if ! test -f "$1.c"; then
    echo "File $1.c not found!"
    exit 1
fi

echo $1
for optflag in 0 1 2 3 fast;
do        
    cp bin"$optflag"/"$1"* /mnt/c/users/david/onedrive/x64os/c_tests/bin"$optflag" &
    cp clangbin"$optflag"/"$1"* /mnt/c/users/david/onedrive/x64os/c_tests/clangbin"$optflag" &
done

wait
echo "Done."
