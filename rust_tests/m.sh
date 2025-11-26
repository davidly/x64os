rustc --edition 2021 -C opt-level=3 -O --emit asm -O $1.rs
rustc --edition 2021 -C opt-level=3 -O -C target-feature=+crt-static -C relocation-model=static --target x86_64-unknown-linux-gnu $1.rs

