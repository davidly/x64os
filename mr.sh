g++ -DX64OS -DNDEBUG -fcf-protection=none -U_FORTIFY_SOURCE -O2 -Wno-psabi -Wno-stringop-overflow -Wno-format-security -fsigned-char -fno-builtin -I . x64os.cxx x64.cxx -o x64os -static
# cp x64os /mnt/c/users/david/onedrive/x64os/bin
# cp x64os bin
objdump -d x64os >x64os.txt
