#!/usr/local/bin/python
"""
httping - Ping like tool for http, display return-code, latency etc

Copyright (C) 2009 Fredrik Steen. Free use of this software is granted
under the terms of the GNU General Public License (GPL).

Edited by Kr1stj0n C1k0 (kristjoc@ifi.uio.no)
"""

from __future__ import division
from optparse import OptionParser
from datetime import datetime
from urllib.parse import urlparse
from statistics import stdev
import http.client
import socket
import time
import requests

# Icky Globals
__VERSION__ = 0.2
USER_AGENT = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.0.10) "\
             "Gecko/2009042523 Ubuntu/9.04 (jaunty) Firefox/3.0.10"


class HTTPing:
    def __init__(self, url, count, debug=False, quiet=False, flood=False,
                 server_report=False, incremental=False, persistent=False):
        self.url = url
        self.count = count
        self.debug = debug
        self.quiet = quiet
        self.flood = flood
        self.server_report = server_report
        self.incremental = incremental
        self.persistent = persistent
        self.totals = []
        self.failed = 0
        self.server_header = None
        self.fail_codes = [500]
        self.urlparse()

    def urlparse(self):
        self.url_parsed = urlparse(self.url)

    def ping(self):
        try:
            self.ip = socket.gethostbyname(self.url_parsed.hostname)
        except socket.gaierror as e:
            print("ERROR %s | Host %s not found" % (e.message,
                                                    self.url_parsed.hostname))
            raise SystemExit
        if not self.quiet:
            print("HTTPING %s (%s)" % (self.url_parsed.hostname, self.ip))

        with requests.Session() as s:
            s.get(self.url, headers={'Connection': 'close'}, timeout=10)
            seq = 0
            for i in range(0, int(self.count)):
                if not self.incremental:
                    (tt, code, reason, frame_len) = self.http_get(s, 0)
                else:
                    (tt, code, reason, frame_len) = self.http_get(s, i)

                seq += 1
                if tt is None:
                    self.failed += 1
                    continue
                elif code in self.fail_codes:
                    self.failed += 1
                    continue

                self.totals.append(tt)
                if not self.quiet:
                    print("sent %d bytes to %s (%s) seq=%s time=%.3f ms" %
                          (frame_len, self.url_parsed.netloc, self.ip, seq,
                           tt))

                if not self.flood:
                    time.sleep(1)

        self.report()
        if self.server_report:
            self.print_server_report()

    def print_server_report(self):

        for x in self.server_header:
            print("%s: %s" % x)

    def http_connect(self):
        try:
            conn = http.client.HTTPConnection(self.url_parsed.hostname,
                                              port=self.url_parsed.port,
                                              timeout=10)

        except TypeError:
            print("Something went wrong")

        conn.set_debuglevel(self.debug)
        return conn

    def http_get(self, s, x):
        try:
            body = bytes(int(pow(2, 10 + (3 * x))))
            if self.persistent:
                headers = {'Host': 'localhost',
                           'Connection': 'keep-alive',
                           'Accept-Encoding': 'gzip, deflate',
                           'Accept': '*/*',
                           'User-Agent': USER_AGENT,
                           'Content-Type': 'application/octet-stream',
                           'Content-Length': str(len(body))
                           }
            else:
                headers = {'Host': 'localhost',
                           'Connection': 'close',
                           'Accept-Encoding': 'gzip, deflate',
                           'Accept': '*/*',
                           'User-Agent': USER_AGENT,
                           'Content-Type': 'application/octet-stream',
                           'Content-Length': str(len(body))
                           }
            hdr2str = str(headers).encode('utf-8')
            frame_len = 14 + 20 + 20 + len(hdr2str) + len(body)

            stime = datetime.now()
            resp = s.post(self.url, data=body, headers=headers, stream=False,
                          timeout=None)
            etime = datetime.now()

            resp_code = resp.status_code
            resp_reason = resp.reason
            # if self.server_header is None:
            # self.server_header = resp.getheaders()
        except Exception as e:
            print(e)
            return (None, None, None, None)

        ttime = etime - stime
        sec = ttime.seconds
        milis = (sec * 1000) + (ttime.microseconds / 1000)
        return (milis, resp_code, resp_reason, frame_len)

    def report(self):
        _num = len(self.totals)
        _min = min(self.totals)
        _max = max(self.totals)
        _average = float(sum(self.totals)) / len(self.totals)
        _stddev = stdev(self.totals)
        print("--- %s ---" % self.url)
        print("%s connects, %s ok, %s%% failed" % (_num, _num - self.failed,
                                                   self.failed / _num))
        print("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.4f ms" %
              (_min, _average, _max, _stddev))


def main():
    usage = "usage: %prog [options] url"
    version = "%%prog %s" % __VERSION__
    parser = OptionParser(usage=usage, version=version)
    parser.add_option("-d", "--debug", action="store_true", dest="debug",
                      default=False, help="make lots of noise")
    parser.add_option("-q", "--quiet", action="store_true", dest="quiet",
                      default=False, help="be very quiet (I'm hunting ants)")
    parser.add_option("-f", "--flood", action="store_true", dest="flood",
                      default=False, help="no delay between pings")
    parser.add_option("-s", "--server", action="store_true",
                      dest="server_report",
                      default=False, help="display verbose report")
    parser.add_option("-c", "--count", default=7, dest="count",
                      help="number of requests")
    parser.add_option("-i", "--incremental", action="store_true",
                      dest="incremental", default=False,
                      help="increment the body size")
    parser.add_option("-p", "--persistent", action="store_true",
                      dest="persistent", default=False,
                      help="persistent http connections")

    (options, args) = parser.parse_args()

    if len(args) < 1:
        print(parser.error("need a url to ping, -h/--help for help"))
        raise SystemExit

    if not (args[0].startswith('http://') or args[0].startswith('https://')):
        print("url needs to start with 'http(s)://'")
        raise SystemExit

    hping = HTTPing(args[0], options.count, options.debug,
                    options.quiet, options.flood, options.server_report,
                    options.incremental, options.persistent)
    try:
        hping.ping()
    except KeyboardInterrupt:
        hping.report()


if __name__ == '__main__':
    main()
