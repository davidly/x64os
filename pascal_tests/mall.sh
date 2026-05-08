#!/bin/bash

apps=(sieve e ttt mm chk tap tphi nqueens nq1d trw mandle tf tdir tcmp tstr \
      trename sleeptm tmuldiv lenum tpi pis tshift ff)

for arg in ${apps[@]}
do
    echo $arg
    mnf.sh $arg
    mnf32.sh $arg
done
