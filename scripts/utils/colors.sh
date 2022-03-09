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
COL_CMDS=($RESET $BOLD $HIGHLIGHT $ERROR $FADE $ALERT)

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
    echo "$1" | awk "$COOKED_AWK"
}
