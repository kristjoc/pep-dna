import re
import subprocess


class Httperf(object):

    def __init__(self, *args, **kwargs):
        self.params = {}
        self.args = []
        self.parser = False

        # Poping path from kwargs if exist
        if 'path' in kwargs:
            self.path = kwargs['path']
            del kwargs['path']
        else:
            self.path = 'httperf'

        # Args for each argument which is not key:value based (hog, ssl...)
        for argument in args:
            if argument not in set(self.__params().keys()):
                raise Exception("Invalid httperf option passed: {}".format(argument))
            self.args.append("--%s" % argument)

        # Arguments passed to httperf
        for key in kwargs:
            if key not in set(self.__params().keys()):
                raise Exception("Invalid httperf option passed:: " + key)
            self.params[key] = kwargs[key]

    def update_option(self, key, val):
        self.params[key] = val

    def run(self):
        try:
            self.results = subprocess.check_output(self.__cmd(),
                                                   shell=True,
                                                   stderr=subprocess.STDOUT,
                                                   close_fds=True)
            if self.parser:
                return HttperfParser.parse(self.results)
            else:
                return self.results
        except subprocess.CalledProcessError as er:
            return {"error": er.output}

    @classmethod
    def display_params(self):
        h = Httperf()
        for param in h.__params().keys():
            print(param)

    def __cmd(self):
        args = [self.path]
        if self.args:
            args += self.args

        for key in self.params.keys():
            val = str(self.params[key])
            key = key.replace('_', '-')
            if key in self.__boolean_params():
                args.append('--%s' % key)
            else:
                args.append('--%s="%s"' % (key, val))

        return ' '.join(args)

    def __boolean_params(self):
        return [
            "hog",
            "no_host_hdr",
            "retry_on_failure",
            "session_cookies",
            "ssl",
            "ssl_no_reuse",
            "verbose",
            "version"
        ]

    def __params(self):
        return {
            "add_header": None,
            "burst_length": None,
            "client": None,
            "close_with_reset": None,
            "debug": None,
            "failure_status": None,
            "hog": None,
            "http_version": None,
            "max_connections": None,
            "max_piped_calls": None,
            "method": None,
            "no_host_hdr": None,
            "num_calls": None,
            "num_conns": None,
            "period": None,
            "port": None,
            "print_reply": None,
            "print_request": None,
            "rate": None,
            "recv_buffer": None,
            "retry_on_failure": None,
            "send_buffer": None,
            "server": None,
            "server_name": None,
            "session_cookies": None,
            "ssl": None,
            "ssl_ciphers": None,
            "ssl_no_reuse": None,
            "think_timeout": None,
            "timeout": None,
            "uri": None,
            "verbose": None,
            "version": None,
            "wlog": None,
            "wsess": None,
            "wsesslog": None,
            "wset": None
        }


class HttperfParser(object):

    @classmethod
    def parse(self, result_string):

        verbose_expression = re.compile("^Connection lifetime = ([0-9]+\.[0-9]+)(\s?)", re.M|re.I)

        lines = result_string.split("\n")
        matches = {}

        verbose_connection_times = []

        for line in lines:

            verbose_match = verbose_expression.match(line)
            if verbose_match:
                verbose_connection_times.append(verbose_match.group(1))
                continue

            if not line == "":
                exps = self.__expressions()
                for key in exps:
                    line_match = exps[key].match(line)
                    if line_match:
                        matches[key] = line_match.group(1)

        if not len(verbose_connection_times) == 0:
            matches["connection_times"] = verbose_connection_times
            for pct in self.__percentiles():
                matches["connection_time_"+str(pct)+"_pct"] = self.__calulate_percentiles(pct, verbose_connection_times)

        return matches
    # end parse

    @classmethod
    def __calulate_percentiles(self, pct, vct):

        if len(vct) == 1:
            return vct[0]

        sorted_vct = sorted(vct)

        if len(vct) == 2:
            return sorted_vct[1]

        sorted_vct = sorted(vct)
        index = int((float(len(vct))/100 * float(pct)))
        return sorted(vct)[index]

    @classmethod
    def __percentiles(self):
        return [75, 80, 85, 90, 95, 99]

    @classmethod
    def __expressions(self):
        return {
            "command": re.compile("^(httperf .+)$"),

            # Maximum connect burst length:
            "max_connect_burst_length": re.compile("Maximum connect burst length: ([0-9]*?\.?[0-9]+)$", re.I|re.M),

            # Total:
            "total_connections": re.compile("^Total: connections ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "total_requests": re.compile("^Total: connections .+ requests ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "total_replies": re.compile("^Total: connections .+ replies ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "total_test_duration": re.compile("^Total: connections .+ test-duration ([0-9]*?\.?[0-9]+) ", re.I|re.M),

            # Connection rate:
            "connection_rate_per_sec": re.compile("^Connection rate: ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "connection_rate_ms_conn": re.compile("^Connection rate: .+ \(([0-9]*?\.?[0-9]+) ms", re.I|re.M),

            # Connection time [ms]:
            "connection_time_min": re.compile("^Connection time \[ms\]: min (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "connection_time_avg": re.compile("^Connection time \[ms\]: min .+ avg (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "connection_time_max": re.compile("^Connection time \[ms\]: min .+ max (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "connection_time_median": re.compile("^Connection time \[ms\]: min .+ median (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "connection_time_stddev": re.compile("^Connection time \[ms\]: min .+ stddev (-?[0-9]*?\.?[0-9]+)$", re.I|re.M),
            "connection_time_connect": re.compile("^Connection time \[ms\]: connect (-?[0-9]*?\.?[0-9]+)$", re.I|re.M),

            # Connection length [replies/conn]:
            "connection_length": re.compile("^Connection length \[replies\/conn\]: ([0-9]*?\.?[0-9]+)$", re.I|re.M),

            # Request rate:
            "request_rate_per_sec": re.compile("^Request rate: ([0-9]*?\.?[0-9]+) req", re.I|re.M),
            "request_rate_ms_request": re.compile("^Request rate: .+ \(([0-9]*?\.?[0-9]+) ms", re.I|re.M),

            # Request size [B]:
            "request_size": re.compile("^Request size \[B\]: ([0-9]*?\.?[0-9]+)$", re.I|re.M),

            # Reply rate [replies/s]:
            "reply_rate_min": re.compile("^Reply rate \[replies\/s\]: min (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_rate_avg": re.compile("^Reply rate \[replies\/s\]: min .+ avg (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_rate_max": re.compile("^Reply rate \[replies\/s\]: min .+ max (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_rate_stddev": re.compile("^Reply rate \[replies\/s\]: min .+ stddev (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_rate_samples": re.compile("^Reply rate \[replies\/s\]: min .+ \((-?[0-9]*?\.?[0-9]+) samples", re.I|re.M),

            # Reply time [ms]:
            "reply_time_response": re.compile("^Reply time \[ms\]: response (-?[0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_time_transfer": re.compile("^Reply time \[ms\]: response .+ transfer (-?[0-9]*?\.?[0-9]+)$", re.I|re.M),

            # Reply size [B]:
            "reply_size_header": re.compile("^Reply size \[B\]: header ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_size_content": re.compile("^Reply size \[B\]: header .+ content ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_size_footer": re.compile("^Reply size \[B\]: header .+ footer ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_size_total": re.compile("^Reply size \[B\]: header .+ \(total ([0-9]*?\.?[0-9]+)\)", re.I|re.M),

            # Reply status:
            "reply_status_1xx": re.compile("^Reply status: 1xx=([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_status_2xx": re.compile("^Reply status: .+ 2xx=([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_status_3xx": re.compile("^Reply status: .+ 3xx=([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_status_4xx": re.compile("^Reply status: .+ 4xx=([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "reply_status_5xx": re.compile("^Reply status: .+ 5xx=([0-9]*?\.?[0-9]+)", re.I|re.M),

            # CPU time [s]:
            "cpu_time_user_sec": re.compile("^CPU time \[s\]: user ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "cpu_time_system_sec": re.compile("^CPU time \[s\]: user .+ system ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "cpu_time_user_pct": re.compile("^CPU time \[s\]: user .+ \(user ([0-9]*?\.?[0-9]+)\% ", re.I|re.M),
            "cpu_time_system_pct": re.compile("^CPU time \[s\]: user .+ system .+ system ([0-9]*?\.?[0-9]+)\% ", re.I|re.M),
            "cpu_time_total_pct": re.compile("^CPU time \[s\]: user .+ total ([0-9]*?\.?[0-9]+)\%", re.I|re.M),

            # Net I/O:
            "net_io_kb_sec": re.compile("^Net I\/O: ([0-9]*?\.?[0-9]+) KB", re.I|re.M),
            "net_io_bps": re.compile("^Net I\/O: .+ \((.+) bps\)", re.I|re.M),

            # Errors:
            "errors_total": re.compile("^Errors: total ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "errors_client_timeout": re.compile("^Errors: total .+ client-timo ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "errors_socket_timeout": re.compile("^Errors: total .+ socket-timo ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "errors_conn_refused": re.compile("^Errors: total .+ connrefused ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "errors_conn_reset": re.compile("^Errors: total .+ connreset ([0-9]*?\.?[0-9]+)", re.I|re.M),
            "errors_fd_unavail": re.compile("^Errors: fd-unavail ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "errors_addr_unavail": re.compile("^Errors: fd-unavail .+ addrunavail ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "errors_ftab_full": re.compile("^Errors: fd-unavail .+ ftab-full ([0-9]*?\.?[0-9]+) ", re.I|re.M),
            "errors_other": re.compile("^Errors: fd-unavail .+ other ([0-9]*?\.?[0-9]+)", re.I|re.M)
        }
