#!/usr/bin/env python
import socket
from optparse import OptionParser
from urlparse import urlparse
from httperfpy import Httperf


__VERSION__ = 0.1


class myHttperf:
    def __init__(self, url, total_conns, rate, calls, timeout):
        self.url = url
        self.total_conns = total_conns
        self.rate = rate
        self.calls = calls
        self.timeout = timeout
        self.urlparse()

    def urlparse(self):
        self.url_parsed = urlparse(self.url)

    def httperf(self):
        try:
            target_ip = socket.gethostbyname(self.url_parsed.hostname)
        except socket.gaierror as e:
            print("ERROR %s | Host %s not found" % (e.message,
                                                    self.url_parsed.hostname))
            raise SystemExit

        perf = Httperf('hog', server=target_ip, port=self.url_parsed.port,
                       uri="file.bin", num_conns=self.total_conns,
                       num_calls=self.calls, rate=self.rate,
                       timeout=self.timeout)
        perf.parser = True
        results = perf.run()
        self.report(results)

    @classmethod
    def report(cls, results):
        print("Target: " + results["total_connections"] + " connections")
        print("Test_Duration [s]: " + results["total_test_duration"])
        # print("CPU_Time [s]: " + results["cpu_time_user_sec"] + " "
        #       + results["cpu_time_system_sec"] + " "
        #       + str(float(results["cpu_time_user_sec"])
        #             + float(results["cpu_time_system_sec"])))
        print("Connection_Rate [conn/s] [ms/conn]: "
              + results["connection_rate_per_sec"] + " "
              + results["connection_rate_ms_conn"])
        print("Connection_Time [ms]: " + results["connection_time_avg"] + "/"
              + results["connection_time_stddev"])
        print("Connect_Connection_Time [ms]: "
              + results["connection_time_connect"])
        print("Total_Requests: " + results["total_requests"])
        print("Request_Rate [req/s]: " + results["request_rate_per_sec"])
        print("Total_Replies: " + results["total_replies"])
        print("Reply_Rate [replies/s]: " + results["reply_rate_avg"] + "/"
              + results["reply_rate_stddev"])
        print("Reply_Time [ms]: " + results["reply_time_response"])
        print("Total Errors: " + results["errors_total"])


def main():
    usage = ("Usage: %prog [options]\n" +
             "[-c] total_connections (default=1000)\n" +
             "[-r] rate (default=200)\n" +
             "[-s] calls (default=1 calls/conn)\n" +
             "[-t] timeout (default=10 s)")

    version = "%%prog %s" % __VERSION__

    parser = OptionParser(usage=usage, version=version)
    parser.add_option("-c", "--conns", default=4000, dest="conns",
                      help="total connections")
    parser.add_option("-r", "--rate", default=200, dest="rate",
                      help="connection rate")
    parser.add_option("-s", "--calls", default=1, dest="calls",
                      help="number of calls per connection")
    parser.add_option("-t", "--timeout", default=10, dest="timeout",
                      help="timeout")

    (options, args) = parser.parse_args()

    if len(args) < 1:
        print(parser.error("need a url to httperf, -h/--help for help"))
        raise SystemExit

    if args[0][:7] != "http://":
        print("url needs to start with 'http://'")
        raise SystemExit

    httperf = myHttperf(args[0], options.conns, options.rate, options.calls,
                        options.timeout)

    try:
        httperf.httperf()
    except KeyboardInterrupt:
        print("Something went wrong")


if __name__ == '__main__':
    main()
