    gcc -m32 $1.s -o $1.elf -static -nostdlib
    objdump -d $1.elf >$1.txt
    cp $1.* /mnt/c/users/david/onedrive/x64os/c_tests/
    

    