#!/bin/bash
SIZE=16000
echo "No Numa"
./matrix -S $SIZE -s 42 -a par_bloc -n 16
echo "fini $?"
echo "Numalize"
./matrix.x -S $SIZE -s 42 -a par_bloc -n 16
echo "fini $?"
dmesg
echo "Numa 2"
./matrix -S $SIZE -s 42 -a par_bloc -n 16 -N 2
echo "fini $?"
echo "Numa 3"
./matrix -S $SIZE -s 42 -a par_bloc -n 16 -N 3
echo "fini $?"
echo "FInit !"
