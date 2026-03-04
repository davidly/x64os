#!/bin/bash

export WATCOM=/snap/open-watcom/2
export PATH=$WATCOM/binl64:$WATCOM/binl:$PATH
export EDPATH=$WATCOM/eddat
export INCLUDE=$WATCOM/lh:$WATCOM/h

# -oe=160 generates faulty code including i64 -3 * -14 = 0

wcl386 -q -oh -oi -ol+ -ei -ot -ombr -fp3 -3r -cc++ -xs -xr $1.c -bcl=LINUX -k8192 -fe=$1.elf -DWATCOM -DNDEBUG

