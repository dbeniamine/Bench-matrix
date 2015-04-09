#!/bin/bash
START_TIME=$(date +%y%m%d_%H%M%S)
CMDLINE="$0 $@"
EXP_NAME=$(basename $0 .sh)
OUTPUT="exp.log"
RUN=30
declare -A TARGET
CONFIGS=('base' 'kmaf' 'Tabarnac' 'numabalance')
TARGET=([base]="./matrix" [kmaf]="./matrix.x" [Tabarnac]="./matrix"\
    [numabalance]="sysctl kernel.numa_balancing=1 && ./matrix")
ALGOS=("par_bloc" "par_modulo")
SIZE=4096
SEED=42
THREADS=64
ARGS="-S $SIZE -s $SEED -n $THREADS"
declare -A  ARGS_SUP
ARGS_SUP=([base]="" [kmaf]="" [Tabarnac]="-N 4" [numactl]="")
#report error if needed
function testAndExitOnError
{
    err=$?
    if [ $err -ne 0 ]
    then
        echo "ERROR $err : $1"
        exit $err
    fi
}
function dumpInfos
{

    #Echo start time
    echo "Expe started at $START_TIME"
    #Echo args
    echo "#### Cmd line args : ###"
    echo "$CMDLINE"
    echo "EXP_NAME $EXP_NAME"
    echo "OUTPUT $OUTPUT"
    echo "RUN $RUN"
    echo "########################"
    # DUMP environement important stuff
    echo "#### Hostname: #########"
    hostname
    echo "########################"
    echo "##### git log: #########"
    git log | head
    echo "########################"
    echo "#### git diff: #########"
    git diff
    echo "########################"
    lstopo --of txt
    cat /proc/cpuinfo
    echo "########################"


    #DUMPING scripts
    cp -v $0 $EXP_DIR/
    cp -v ./*.sh $EXP_DIR/
    cp -v *.pl $EXP_DIR/
    cp -v *.rmd  $EXP_DIR/
    cp -v Makefile  $EXP_DIR/
}
#TODO: remove this crap
declare -A VAL
VAL=([base]=4 [kmaf]=3  [numactl]=3 [Tabarnac]=2)
declare -A FACT
FACT=([par_bloc]=1 [par_modulo]=2)
function res
{
    echo "----------> $(( ${VAL[$1]}*${FACT[$2]} )) (ms)"
}

if [ $(whoami) != "root" ]
then
    echo "This script must be run as root"
    exit 1
fi
machinelock
testAndExitOnError "can't lock machine"
#parsing args
while getopts "ho:e:r:" opt
do
    case $opt in
        h)
            usage
            exit 0
            ;;
        e)
            EXP_NAME=$OPTARG
            ;;
        o)
            OUTPUT=$OPTARG
            ;;
        r)
            RUN=$OPTARG
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done
#post init
EXP_DIR="$EXP_NAME"_$(date +%y%m%d_%H%M)
mkdir $EXP_DIR
OUTPUT="$EXP_DIR/$OUTPUT"

#Do the first compilation
cd ../src/module
make
cd -

#Continue but change the OUTPUT
exec > >(tee $OUTPUT) 2>&1
dumpInfos


cd matrix
make
cd -
for run in $(seq 1 $RUN)
do
    echo "RUN : $run"
    #Actual exp
    for algo in ${ALGOS[@]}
    do
        echo "Algo: $algo"
        LOGDIR="$EXP_DIR/algo-$algo/run-$run"
        mkdir -p $LOGDIR
        #Actual experiment
        for conf in ${CONFIGS[@]}
        do
        # set -x
        sysctl kernel.numa_balancing=0
        testAndExitOnError "can't disable numa balancing"
        echo "${TARGET[$conf]} -a $algo $ARGS ${ARGS_SUP[$conf]}" \
            > $LOGDIR/$conf.log 2> $LOGDIR/$conf.err
        #TODO: Remove me
        res $conf $algo    >> $LOGDIR/$conf.log
        # set +x
        testAndExitOnError "run number $run"
        done
    done
done

#cd $EXP_DIR/
#./parseAndPlot.sh
#cd -
#Echo thermal throttle info
echo "thermal_throttle infos :"
cat /sys/devices/system/cpu/cpu0/thermal_throttle/*
END_TIME=$(date +%y%m%d_%H%M%S)
echo "Expe ended at $END_TIME"
machineunlock

