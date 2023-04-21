#! /usr/bin/env bash

BINARY=./build/wl-sim.elf

# if binary file does not exist, build it
if [ ! -f $BINARY ]; then
    idf.py build
fi

if [[ "$1" == "--help" || "$1" == "help" || "$1" == "-h" ]]; then
    echo "Run without args for simulation help. Additional modes are {test, clean}"
    exit 0
fi

# clean argument removes all wl-sim logs from current directory
if [ "$1" = "clean" ]; then
    if ls ./wl-sim.* 1>/dev/null 2>&1; then
        removed=$(eval ls -l wl-sim.* | wc -l)
        rm wl-sim.*
    else
        removed=0
    fi
    echo "Removed $removed wl-sim log files"
    exit 0
fi

# number of simulation runs
N=10

if [ "$1" = "test" ]; then
    N=1
fi

# output log file with name based on current timestamp
file="wl-sim.$(date +%Y%m%d%H%M%S).log"

# run wl-sim N times, appending output to file
for ((i = 1; i <= $N; i++)); do
    ./build/wl-sim.elf $1 $2 $3 $4 $5 >> $file
    if [ "$?" != "0" ]; then
        echo "Run $i/$N failed"
        ./build/wl-sim.elf $1 $2 $3 $4 $5
        echo "Example: run.sh f z z 10 0"
        rm -f $file
        exit $?
    fi
    echo "($i/$N)"
    sleep 0.01
done

# average the values of normalized endurance and cycle walks
cat $file | awk '{ NE_sum += $2; cycle_walk_sum += $4 } END { print "avg(NE):", NE_sum/NR, "avg(cycle_walks):", cycle_walk_sum/NR }' >> $file
cat $file

