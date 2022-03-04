#!/usr/bin/env bash

SCRIPT_DIR="$(dirname ${BASH_SOURCE[0]})"
REPO_ROOT="$SCRIPT_DIR/.."

source "$SCRIPT_DIR/utils/colors.sh"

requirements() {
    for cmd in grep cut sed lscpu free awk xargs jq valgrind curl gnuplot
    do
        command -v $cmd &> /dev/null

        if [ $? -ne 0 ]; then
            _errs+=($cmd)
        fi
    done

    if [ ${#_errs[@]} -ne 0 ]; then
        cprint "\ERequired commands\R (\B${_errs[*]}\R) \Ecould not be found!\R"
        exit 1
    fi
}

lscpu_value() {
    lscpu | grep "$1:" | cut -d ':' -f 2 | sed 's/\s*//'
}

free_value() {
    row=$1
    column=$2
    free -h | grep "$row" | tr -s ' ' | cut -d ' ' -f $(($column+1))
}

cpu_utilization() {
    _query1=$(grep 'cpu ' /proc/stat | awk '{u=$2+$4; t=u+$5} END { print u " " t }')
    sleep 1
    _query2=$(grep 'cpu ' /proc/stat | awk '{u=$2+$4; t=u+$5} END { print u " " t }')

    echo $_query1 $_query2 | awk '{ print ($3-$1) * 100 / ($4-$2) }' | xargs printf "%.2f%%"
}

# Reads value at key $1 from config.json
# and prints it, or print $2 if the key
# doesn't exist
config_value() {
    _cfg="$(dirname ${BASH_SOURCE[0]})/../config.json"
    _output=$(cat ${_cfg} | jq -e $1)
    if [ $? -eq 1 ]; then
        echo $2
    else
        echo $_output
    fi
}

check_vars() {
    _arr=()

    for i in "$@"; do
        _res=$(eval "echo -n \$$i")
        if [ -n "$_res" ]; then
            _arr+=("\B$i\R: \H$_res\R")
        fi
    done

    echo ${_arr[*]}
}

send_test_data() {
    curl -k "https://$URI/service/test" &>/dev/null; sleep 2
    curl -k "https://$URI/test_digest" &>/dev/null; sleep 2
    curl -k --digest --user bad:password "https://$URI/test_digest" &>/dev/null; sleep 2
    curl -k --digest --user admin:mypass "https://$URI/test_digest" &>/dev/null; sleep 2
}

check_latency() {
    _start=$(date +%s%N)
    curl -k "https://$URI/test" -f &>/dev/null;
    _end=$(date +%s%N)

    # Time is in milliseconds
    echo $(((_end-_start)/1000000))
}

average_latency() {
    _program=$1

    if [ -n "$_program" ]; then
        $_program &>/dev/null &
        CHILD_PID=$!
    fi

    _average=0
    for i in {1..10}
    do
        _lat=$(check_latency)
        ((_average+=$_lat))
    done

    if [ -n "$_program" ]; then
        kill -INT $CHILD_PID
        wait_for $CHILD_PID 5 INT
    fi

    echo $(( _average / 10 ))
}

wait_for() {
    _process=$1
    _timeout=$2
    _signal=$3

    if [[ -z "$_signal" ]]; then
        _signal=KILL
    fi

    _counter=0

    kill -s 0 $_process &>/dev/null
    while [ $? -eq 0 ]; do
        if [[ -n "$_timeout" ]]; then
            if [[ $_counter -ge $_timeout ]]; then
                cprint "\ATimeout reached while waiting for PID $_process to end, sending SIG$_signal to process.\R"
                kill -$_signal $_process
                _counter=0
            fi
        fi

        sleep 1

        if [[ -n "$_timeout" ]]; then
            ((_counter+=1))
        fi

        kill -s 0 $_process &>/dev/null
    done
}

valgrind_run() {
    _tool=$1
    _program=$2
    _extra_args=${@:3}

    cprint "\ARunning $_tool on $_program...\R"
    valgrind --tool=$_tool $_extra_args $_program >/dev/null &

    CHILD_PID=$!

    send_test_data &
    TESTING_PID=$!

    # Wait for tests to complete
    wait_for $TESTING_PID

    cprint "\ASending SIGINT (Ctrl-C) signal to $_program...\R"
    kill -INT $CHILD_PID

    # Wait for process termination
    wait_for $CHILD_PID 5 INT
}

_monitor() {
    _pid=$1
    _file=$2

    kill -s 0 $_pid &>/dev/null
    for (( i=0; $?==0; i++ )); do
        if ! echo $i $(grep VmSize /proc/${_pid}/status | grep -o '[0-9]*') >> "$_file"; then
            break
        fi
        sleep 1
        kill -s 0 $_pid &>/dev/null
    done
}

resource_monitor() {
    _program=$1
    cprint "\AMonitoring resource usage of $_program...\R"

    _file="$OUT_DIR/$(basename $_program).perfdata"
    echo "#Time(sec) Memory(Bytes)" > "$_file"

    $_program >/dev/null &
    CHILD_PID=$!

    send_test_data &
    TESTING_PID=$!

    # Monitor the memory usage of the program until the tests are done
    _monitor $CHILD_PID $_file &
    MONITOR_PID=$!

    # Wait for tests to complete
    wait_for $TESTING_PID

    cprint "\ASending SIGINT (Ctrl-C) signal to $_program...\R"
    kill -INT $CHILD_PID

    # Wait for process termination
    wait_for $CHILD_PID

    # Wait for monitoring to stop
    wait_for $MONITOR_PID

    cprint "\APlotting data from $_program...\R"
    gnuplot -e "set terminal x11; set title \"Memory Usage of $_program\"; set xlabel \"Time in Seconds\"; set ylabel \"Memory Usage in Bytes\"; set style fill solid 1.0; plot \"$_file\" smooth freq with fillsteps" -p
}

# Make sure we have every command we need before we do any processing.
requirements

help() {
    cprint "\FUsage: $0 \B[OPTIONS]\F...\R"
    cprint "\BThis script will perform various performance monitoring steps\R"
    cprint "\Bon the server in this repository.\R"
    print_opt "h|help" "Displays this message and quits."
    print_opt "r|remote" "Skips all hardware checks and only performs latency and other network checks."
    print_opt "u|uri" "The base uri of the server such that \F'https://{uri}/'\R resolves to the server."
    print_opt "o|out" "Directory to place all the performance monitoring results \F(default: ./perf)\R"
    echo
}

export REMOTE=0
export URI="localhost:8080"
export OUT_DIR="$(pwd)/perf"

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) help; exit;;
        -r|--remote) REMOTE=1; shift 1;;
        -u|--uri) URI="$2"; shift 2;;
        -o|--out) OUT_DIR="$2"; shift 2;;
        -i|--in) PROGRAM="$2"; shift 2;;

        -u=*|--uri=*) URI="${1#*=}"; shift 1;;
        -o=*|--out=*) OUT_DIR="${1#*=}"; shift 1;;
        -i=*|--in=*) PROGRAM="${1#*=}"; shift 1;;

        -*) echo "Unknown option: $1" >&2; help; exit 1;;
        *) echo "Ignoring extra argument: $1"; shift 1;;
    esac
done

if [ $REMOTE -eq 0 ]; then
    if [ -z "$PROGRAM" ]; then
        cprint "\E--remote not enabled so the server executable must be passed in with --in!\R"
    elif [ ! -x "$PROGRAM" ]; then
        cprint "\EPassed in file must be an executable!\R"
    fi
fi

if [ ! -d $OUT_DIR ]; then
    mkdir -p $OUT_DIR
fi

OUT_DIR=$(readlink -f $OUT_DIR)

printf "Processing...\r"

if [ $REMOTE -eq 0 ]; then
    CPU_UTILIZATION=$(cpu_utilization)

    ARCHITECTURE=$(lscpu_value Architecture)
    NAME=$(lscpu_value "Model name")
    CORES=$(lscpu_value "Core(s) per socket")
    THREADS=$(lscpu_value "Thread(s) per core")

    cprint "\BSystem Information\R:"
    cprint "  \BArchitecture\R:    \H$ARCHITECTURE\R"
    cprint "  \BCPU\R:             \H$NAME\R"
    cprint "  \BCores\R:           \H$CORES\R"
    cprint "  \BThreads\R:         \H$THREADS\R"

    TOTAL_MEM=$(free_value Mem 1)
    USED_MEM=$(free_value Mem 2)
    FREE_MEM=$(free_value Mem 3)
    CACHED_MEM=$(free_value Mem 5)

    TOTAL_SWAP=$(free_value Swap 1)
    USED_SWAP=$(free_value Swap 2)
    FREE_SWAP=$(free_value Swap 3)

    cprint "\BResources\R:"
    cprint "  \BMemory\R:          \H$USED_MEM/$TOTAL_MEM used\R \F($CACHED_MEM cached)\R - \H$FREE_MEM free\R"
    cprint "  \BSwap\R:            \H$USED_SWAP/$TOTAL_SWAP used\R - \H$FREE_SWAP free\R"
    cprint "  \BCPU Utilization\R: \H$CPU_UTILIZATION\R"
    echo "-------------------"
    echo

    if [ -f "$UTOPIA_CONFIG_FILE" ]; # Check for config in environ
    then
        _cfg="$UTOPIA_CONFIG_FILE"
    elif [ -f "config.json" ] # Check for config in current directory
    then
        _cfg="config.json"
    elif [ -f "$REPO_ROOT/config.json" ] # Then repository root
    then
        _cfg="$(dirname ${BASH_SOURCE[0]})/../config.json"
    fi

    if [ -f "$_cfg" ]; then
        cprint "\BCurrent JSON configuration file:\R"
        cat $_cfg | jq .
        echo
    else
        cprint "\ENo JSON configuration file was found, so none will be used!\R"
    fi

    ENVIRONS=(
        UTOPIA_CONFIG_FILE
        UTOPIA_PORT
        UTOPIA_MAX_CONNECTIONS
        UTOPIA_TIMEOUT
        UTOPIA_THREADS
        UTOPIA_THREAD_PER_CONNECTION
        UTOPIA_USE_IPV4
        UTOPIA_USE_IPV6
        UTOPIA_CERTIFICATE
        UTOPIA_PRIVATE_KEY
    )
    VALID=($(check_vars ${ENVIRONS[@]}))

    if [ ${#VALID[@]} -ne 0 ]; then
        for environ in "${VALID[@]}"; do
            cprint $environ
        done
    else
        cprint "\ENo environment variables have been set for the server!\R"
    fi
    echo

    cprint "\ACalculating average latency of the server...\R"
    _avlat=$(average_latency $1)
    cprint "  \BAverage Latency\R: \H${_avlat}ms\R"

    _base="$OUT_DIR/$(basename $1)"
    valgrind_run cachegrind $1 --branch-sim=yes --cachegrind-out-file="$_base.cachegrind"
    valgrind_run callgrind $1  --callgrind-out-file="$_base.callgrind"
    valgrind_run massif $1 --massif-out-file="$_base.massif"
    valgrind_run memcheck $1 --xtree-memory=full --xtree-memory-file="$_base.xtree"

    cprint "\AAnnotating callgraph from valgrind into\R '\B$_base.callgrind.annotated\R'\A...\R"
    callgrind_annotate --auto=yes --inclusive=yes --sort=curB:100,curBk:100 "$_base.xtree" > "$_base.callgrind.annotated"

    resource_monitor $1
else
    cprint "\E--remote enabled, assuming server isn't on this system and ignoring hardware.\R"
    cprint "\ACalculating average latency of the server...\R"
    _avlat=$(average_latency)
    cprint "  \BAverage Latency\R: \H${_avlat}ms\R"
fi
