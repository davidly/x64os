#!/bin/bash
#set -x

mkdir bin 2> /dev/null
mkdir /mnt/c/users/david/onedrive/x64os/f_tests/bin 2> /dev/null

if [ "$1" == "" ]; then
    optflags="2"
else
    optflags="$1"
fi

for arg in sieve e mm ttt;
do
    echo $arg
    m.sh $arg $optflags
done

for arg in primes;
do
    echo $arg
    mf90.sh $arg $optflags
done

