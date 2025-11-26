#!/bin/bash
#set -x

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

