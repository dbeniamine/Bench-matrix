#!/bin/bash
echo "Algo, Run, Opti,Time" > results.csv
grep ms -R algo-par_* | sed -e \
    's/algo-par_\([^\/]*\)\/run-\([0-9]*\)\/\(.*\).log:[->]*\s\([0-9]*\).*/\1,\2,\3,\4/'\
    -e 's/modulo/Naif/' -e 's/bloc/Bloc/' -e 's/kmaf/Kmaf/' \
    -e 's/numactl/Numactl/' -e 's/base/Base/' >> results.csv
