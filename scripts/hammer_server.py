#!/usr/bin/python3

import threading
import os

def send_request():
    for x in range(100):
        os.system("curl -k 'https://localhost:8080/helloworld'")

thread_count = 10

threads = []
for x in range(thread_count):
    threads.append(threading.Thread(target=send_request))
    threads[x].start()

for x in range(thread_count):
    threads[x].join()
