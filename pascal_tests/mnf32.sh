#!/bin/bash
# Check if a filename was provided
if [ -z "$1" ]; then
    echo "Usage: $0 <filename_without_extension>"
    exit 1
fi

for OPT in 1 2 3; do
    DIR="x32bin${OPT}"
    mkdir $DIR > /dev/null 2>&1
    rm $DIR/$1 > /dev/null 2>&1
    rm $DIR/$1.o > /dev/null 2>&1
    ../x32os /usr/bin/ppc386 -Aelf -g -O${OPT} -o${DIR}/$1 $1.pas
    objdump -d $DIR/$1 > $DIR/$1.txt
    rm $DIR/$1.o > /dev/null 2>&1
done
 
