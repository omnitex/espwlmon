#! /usr/bin/env bash

BINARY=./build/wl-sim.elf


# if first argument is build, do as it says and shift remaining arguments
if [[ "$1" == "build" ]]; then
    idf.py build
    shift
    # if no other argument, exit
    if [[ "$1" == "" ]]; then
        exit 0
    fi
fi

# if binary file does not exist, build it
if [ ! -f $BINARY ]; then
    idf.py build
fi

# offer at least some primitive help
if [[ "$1" == "--help" || "$1" == "help" || "$1" == "-h" ]]; then
    echo -e "Run without arguments for simulation help. Or with {test, clean}.\nAlso can prepend args with 'build' to rebuild first e.g. '$0 build test' (useful if changing sources)\nOtherwise build is done automatically if the binary is missing"
    exit 0
fi

# 'clean' argument removes all wl-sim logs from current directory
if [ "$1" = "clean" ]; then
    if ls ./wl-sim.* 1>/dev/null 2>&1; then
        removed=$(eval ls -l wl-sim.* | wc -l)
        rm wl-sim.*.log
    else
        removed=0
    fi
    echo "Removed $removed wl-sim log files"
    exit 0
fi

# 'test' argument runs only the test and exits
if [ "$1" = "test" ]; then
    $BINARY $1
    exit 0
fi

# output log file with name based on current timestamp
file="wl-sim.$1-$2-$3-$4-$5.$(date +%Y%m%d%H%M%S).log"

# register response Ctrl+C to remove log file and exit
trap "rm -f $file; exit 0" SIGINT

# number of simulation runs
N=10

# run wl-sim N times, appending output to file
for ((i = 1; i <= $N; i++)); do
    # run the built binary passing simulation arguments, output gets appended to file
    $BINARY $1 $2 $3 $4 $5 2>&1 >> $file
    # nonzero return code signifies failure
    if [ "$?" != "0" ]; then
        echo "Run $i/$N failed"
        # run once again without redirecting stdout
        $BINARY $1 $2 $3 $4 $5
        echo "Example: $0 f z z 10 0"
        # remove partially written file
        rm -f $file
        exit $?
    fi
    # otherwise report iteration number and wait a little bit for next round as address randomization is seeded by current timestamp
    echo "($i/$N)"
    sleep 0.01
done

# calculate average values of normalized endurance and cycle walks, append to file as well
averages=$(eval cat $file | awk '{ NE_sum += $2; cycle_walk_sum += $4; restarted_sum += $6} END { print "avg(NE):", NE_sum/NR, "avg(cycle_walks):", cycle_walk_sum/NR, "avg(restarted):", restarted_sum/NR }')
echo "$averages" >> $file
echo "$averages"
