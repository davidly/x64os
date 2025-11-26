#!/bin/bash


for arg in ato e fileops mysort real tap td tphi ttt tmm;
do
    echo $arg

    for optflag in 0 1 2 3;
    do
        mkdir bin"$optflag" 2>/dev/null
        if [ "$optflag" != "3" ]; then
            rustc --edition 2021 --out-dir bin"$optflag" -C overflow-checks=off -C opt-level="$optflag" -C target-feature=+crt-static -C relocation-model=static "$arg".rs &
        else
            rustc --edition 2021 --out-dir bin"$optflag" -C overflow-checks=off -C opt-level="$optflag" -C target-feature=+crt-static -C relocation-model=static "$arg".rs
        fi
    done    
done

echo "Waiting for all processes to complete..."
wait

for arg in ato e fileops mysort real tap td tphi ttt tmm;
do
    cp $arg.rs /mnt/c/users/david/onedrive/x64os/rust_tests/
    for optflag in 0 1 2 3;
    do
        cp bin"$optflag"/"$arg" /mnt/c/users/david/onedrive/x64os/rust_tests/bin"$optflag"/
    done    
done

