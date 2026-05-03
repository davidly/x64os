#!/bin/bash

apps=(sieve e ttt mm chk)

for arg in ${apps[@]}
do
    echo $arg
    mnf.sh $arg
    mnf32.sh $arg
done
