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

# output log file with name based on simulation params
file="wl-sim.$1-$2-$3-$4-$5.log"

# if file does not exist i.e. from previous simulation runs, create it
if [ ! -f $file ]; then
    touch $file
fi

# get number of past runs, where every line with simulation results == one run
past_runs=$(eval cat $file | wc -l)

# max simulation runs for given parameter combination (each combination => unique file name)
N_MAX=100

# if past runs have not reached max number, calculate remaining runs
if [[ "$past_runs" -lt "$N_MAX" ]]; then
    # number of simulation runs to add in this execution
    N=$(($N_MAX - $past_runs))
else
    # otherwise set N to 0 so the following loop will not run
    N=0
fi

if [[ "$N" > 0 ]]; then
    echo "Detected $past_runs past results"
    echo "Running..."
fi

# run wl-sim N times, appending output to file
for ((i = 1; i <= $N; i++)); do
    # run the built binary passing simulation arguments, output gets appended to file
    $BINARY $1 $2 $3 $4 $5 2>&1 >> $file
    # nonzero return code signifies failure
    if [ "$?" != "0" ]; then
        echo "Run $(($past_runs + $i))/$(($past_runs + $N)) failed"
        # run once again without redirecting stdout
        $BINARY $1 $2 $3 $4 $5
        echo "Example: $0 f z z 10 0"
        # remove partially written file
        rm -f $file
        exit $?
    fi
    # otherwise report iteration number and wait a little bit for next round as address randomization is seeded by current timestamp
    # report x/N including past runs
    echo "($(($past_runs + $i))/$(($past_runs + $N)))"
    sleep 0.5
done

# if after the loop we got to the max number of results we wanted as set by N_MAX
# calculate and report averages
if [[ $(eval cat $file | wc -l) == $N_MAX ]]; then
    averages=$(eval cat $file | awk '{ NE_sum += $2; cycle_walk_sum += $4; restarted_sum += $6; feistel_calls_sum += $8} END { print "avg(NE):", NE_sum/NR, "avg(cycle_walks):", cycle_walk_sum/NR, "avg(restarted):", restarted_sum/NR, "avg(feistel_calls):", feistel_calls_sum/NR, "CW_percent:", cycle_walk_sum/feistel_calls_sum*100 }')
    # also append to file
    echo "$averages" >> $file
    # and print
    echo "Simulation finished!"
    echo "$averages"
else
    # if not, that means averages got already appended, in that case, print them from the last line
    echo "Results already calculated, reading from file:"
    tail -n1 $file
fi
