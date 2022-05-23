#!/usr/bin/env bash

SERVER_PID=$(pidof utopia-server)

read_bytes() {
  cat /proc/$SERVER_PID/io | sed '5q;d' | cut -d' ' -f2
}

write_bytes() {
  cat /proc/$SERVER_PID/io | sed '6q;d' | cut -d' ' -f2
}

get_drive_info() {
  fio --name utopia-server-test --eta-newline=5s --filename=$(maketmp) --rw=randread --size=2g --io_size=10g --blocksize=4K --ioengine=libaio --fsync=1 --iodepth=1 --direct=1 --numjobs=32 --runtime=60 --group_reporting
}

last_read_bytes=$(read_bytes)
last_write_bytes=$(write_bytes)

echo "Data In | Data Out"
printf "%d bps | %d bps\n" 0 0

while :
do
  sleep 1
  read=$(read_bytes)
  write=$(write_bytes)
  printf "%d bps | %d bps\n" $((read-last_read_bytes)) $((write-last_write_bytes))
  last_read_bytes=$read
  last_write_bytes=$write
done