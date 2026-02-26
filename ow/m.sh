#!/bin/bash

export WATCOM=/snap/open-watcom/2
export PATH=$WATCOM/binl64:$WATCOM/binl:$PATH
export EDPATH=$WATCOM/eddat
export INCLUDE=$WATCOM/lh:$WATCOM/h

wcl386 -q -3r -cc++ -xs -xr $1.c -bcl=LINUX -k65536 -fe=$1.elf -DWATCOM -DNDEBUG -I.

