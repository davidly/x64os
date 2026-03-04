#!/bin/bash

export WATCOM=/snap/open-watcom/2
export PATH=$WATCOM/binl64:$WATCOM/binl:$PATH
export EDPATH=$WATCOM/eddat
export INCLUDE=$WATCOM/lh:$WATCOM/h

_env=WATCOM=$WATCOM\;PATH=$WATCOM/binl64:$WATCOM/binl\;EDPATH=$EDPATH\;INCLUDE=$INCLUDE

rm -f $1.map $1.o $1 $1.elf 1>/dev/null 2>&1

# run the Open Watcom 2.0 compiler and linker in the x32os emulator to help validate the emulator

../x32os -e:$_env $WATCOM/binl/wpp386 -q -oh -oi -ol+ -ei -ot -ombr -fp3 -3r -xs -xr -bt=linux -fo$1.o $1.c -DWATCOM -DNDEBUG
../x32os -e:$_env $WATCOM/binl/wlink system linux name $1.elf file $1.o option quiet option stack=8192 option map
