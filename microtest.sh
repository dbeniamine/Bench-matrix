#!/bin/bash
./matrix -a par_bloc -S 4096 -s 42 -n 64 
./matrix -a par_bloc -S 4096 -s 42 -n 64 -N 4
./matrix -a par_modulo -S 4096 -s 42 -n 64 
./matrix -a par_modulo -S 4096 -s 42 -n 64 -N 4
./matrix.x -a par_bloc -S 4096 -s 42 -n 64 
./matrix.x -a par_modulo -S 4096 -s 42 -n 64 
numactl -i all ./matrix -a par_bloc -S 4096 -s 42 -n 64 
numactl -i all ./matrix -a par_modulo -S 4096 -s 42 -n 64 
