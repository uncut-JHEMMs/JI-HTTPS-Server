### Styles ###
# The left side of the | is the symbol name, such that
# doing \<symbol name> in the string you wish to print
# will replace \<symbol name> with the right side of the
# |. So in the case of BOLD, it's symbol is \B, which
# will be replaced with \033[1m or \e[1m, the color
# code for bold
RESET="R|\033[0m"
BOLD="B|\033[1m"
HIGHLIGHT="H|\033[1;34m"
ERROR="E|\033[1;31m"
FADE="F|\033[37m"
ALERT="A|\033[1;32m"
EMPTY="-|"
COL_CMDS=($RESET $BOLD $HIGHLIGHT $ERROR $FADE $ALERT $EMPTY)

# Here I'm calculating the awk command, so that you can add
# more colors to COL_CMDS and it'll automatically be added
# or "cooked" into the awk command.
COOKED_AWK="{"
for col_cmd in "${COL_CMDS[@]}"; do
    _sym=$(echo $col_cmd | cut -d'|' -f1)
    _val=$(echo $col_cmd | cut -d'|' -f2)
    COOKED_AWK+="gsub(/\\\\${_sym}/,\"${_val}\")"
done
COOKED_AWK+="}1"

cprint() {
    if [ $# -gt 0 ]; then
        echo -e "$(echo "$@" | awk "$COOKED_AWK")"
    else
        cat /dev/stdin | awk "$COOKED_AWK"
    fi
}

repeat() {
    _start=1
    _end=${1:-80}
    _str=${2:- }
    for (( i=$_start; i<=$_end; i++ )); do echo -n "${_str}"; done
}

print_opt() {
    _short=$(echo $1 | cut -d'|' -f1)
    _long=$(echo $1 | cut -d'|' -f2)
    _desc=$2

    _str="  \B-$_short\R, \B--$_long\R"
    _str+="$(repeat $(expr 27 - ${#_str}) ' ')"

    _newlen=$(expr ${#_desc} + ${#_str})
    if [[ $_newlen -gt 72 ]]; then
        for (( i=72; i<=$_newlen; i++ )); do
            idx=$(expr $i - ${#_str} - 1)
            if [ "${_desc:$idx:1}" == ' ' ]; then
                _str+="\H${_desc:0:$idx}\R\n$(repeat 20 ' ')\H${_desc:$idx}\R"
                break
            fi
        done
    else
        _str+="\H${_desc}\R"
    fi

    cprint "$_str"
}
