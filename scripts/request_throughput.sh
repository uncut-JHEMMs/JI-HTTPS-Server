#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
COUNTER_DIR=$(mktemp -d)

uri=http://localhost:8080/echo
max=10
keep_running=1
failures=0
successes=0

function get()
{
  code=$(curl -o /dev/null -sw "%{http_code}" --data-binary @$SCRIPT_DIR/data/lorem_ipsum.txt $uri)
  if [ $code -ne 200 ]; then
    $SCRIPT_DIR/utils/parallel_counter.pl $COUNTER_DIR failures
  else
    $SCRIPT_DIR/utils/parallel_counter.pl $COUNTER_DIR successes
  fi
}

echo 0 > $COUNTER_DIR/failures
echo 0 > $COUNTER_DIR/successes

echo "success | fail | max"
while [ $failures -eq 0 ]; do
  for (( i=0; i<max; i++ )); do
    get &
  done

  failures=$(cat $COUNTER_DIR/failures)
  successes=$(cat $COUNTER_DIR/successes)
  printf "%d | %d | %d\n" $successes $failures $max >> req.txt
  printf "%d | %d | %d\r" $successes $failures $max

  ((max *= 2))
done