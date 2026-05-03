DEST=/mnt/c/users/david/onedrive/x64os/pascal_tests

for DIR in bin1 bin2 bin3 x32bin1 x32bin2 x32bin3; do
    mkdir -p $DEST/$DIR
    cp $DIR/* $DEST/$DIR
done

