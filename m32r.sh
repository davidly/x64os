g++ -DNDEBUG -DX32OS -fcf-protection=none -U_FORTIFY_SOURCE -O2 -Wno-psabi -Wno-stringop-overflow -Wno-format-security -fsigned-char -fno-builtin -I . x64os.cxx x64.cxx -o x32os -static
#cp x32os /mnt/c/users/david/onedrive/x64os/bin
#cp x32os bin
#objdump -d x32os >x32os.txt
