for OPT in 1 2 3; do
    DIR="bin${OPT}"
    mkdir $DIR > /dev/null 2>&1
    fpc -Px86_64 -g -O${OPT} -o${DIR}/$1 $1.pas
    rm ${DIR}/$1.o > /dev/null 2>&1
done
