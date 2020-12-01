import re, os, sys
import string
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "httperfpy"))

from httperfpy import *

global httperf_path, httperf_results, httperf_verbose_results

httperf_path = string.strip(os.popen("which httperf").read())

if httperf_path == '':
    raise Exception("httperf must be install")

httperf_results = """
httperf --client=0/1 --server=localhost --port=80 --uri=/ --send-buffer=4096 --recv-buffer=16384 --num-conns=1 --num-calls=1
httperf: warning: open file limit > FD_SETSIZE; limiting max. # of open files to FD_SETSIZE
Maximum connect burst length: 0

Total: connections 1 requests 1 replies 1 test-duration 0.000 s

Connection rate: 2114.1 conn/s (0.5 ms/conn, <=1 concurrent connections)
Connection time [ms]: min 0.6 avg 0.6 max 0.6 median 0.5 stddev 0.0
Connection time [ms]: connect 0.1
Connection length [replies/conn]: 1.000

Request rate: 2114.1 req/s (0.5 ms/req)
Request size [B]: 62.0

Reply rate [replies/s]: min 0.0 avg 0.0 max 0.0 stddev 0.0 (0 samples)
Reply time [ms]: response 0.4 transfer 0.0
Reply size [B]: header 216.0 content 151.0 footer 0.0 (total 367.0)
Reply status: 1xx=0 2xx=1 3xx=0 4xx=0 5xx=0

CPU time [s]: user 0.00 system 0.00 (user 0.0% system 0.0% total 0.0%)
Net I/O: 885.7 KB/s (7.3*10^6 bps)

Errors: total 0 client-timo 0 socket-timo 0 connrefused 0 connreset 0
Errors: fd-unavail 0 addrunavail 0 ftab-full 0 other 0
"""

httperf_verbose_results = """
httperf --verbose --client=0/1 --server=localhost --port=80 --uri=/ --send-buffer=4096 --recv-buffer=16384 --num-conns=10 --num-calls=1
httperf: warning: open file limit > FD_SETSIZE; limiting max. # of open files to FD_SETSIZE
httperf: maximum number of open descriptors = 1024
Connection lifetime = 0.56    
Connection lifetime = 0.35    
Connection lifetime = 0.34    
Connection lifetime = 0.33    
Connection lifetime = 0.75    
Connection lifetime = 0.42    
Connection lifetime = 0.31    
Connection lifetime = 0.31    
Connection lifetime = 0.30    
Connection lifetime = 0.29    
Maximum connect burst length: 1

Total: connections 10 requests 10 replies 10 test-duration 0.005 s

Connection rate: 2150.6 conn/s (0.5 ms/conn, <=1 concurrent connections)
Connection time [ms]: min 0.3 avg 0.4 max 0.7 median 0.5 stddev 0.1
Connection time [ms]: connect 0.1
Connection length [replies/conn]: 1.000

Request rate: 2150.6 req/s (0.5 ms/req)
Request size [B]: 62.0

Reply rate [replies/s]: min 0.0 avg 0.0 max 0.0 stddev 0.0 (0 samples)
Reply time [ms]: response 0.3 transfer 0.0
Reply size [B]: header 216.0 content 151.0 footer 0.0 (total 367.0)
Reply status: 1xx=0 2xx=10 3xx=0 4xx=0 5xx=0

CPU time [s]: user 0.00 system 0.00 (user 0.0% system 86.0% total 86.0%)
Net I/O: 901.0 KB/s (7.4*10^6 bps)

Errors: total 0 client-timo 0 socket-timo 0 connrefused 0 connreset 0
Errors: fd-unavail 0 addrunavail 0 ftab-full 0 other 0
""" 
