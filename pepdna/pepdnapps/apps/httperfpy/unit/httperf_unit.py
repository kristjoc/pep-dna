import unit_helper
import unittest
import re

# import everything unit_helper does
from unit_helper import *


class HttperfTestCase(unittest.TestCase):

    def testHttperfDisplayParams(self):
        assert Httperf.display_params

    def testInitHttperfWithDefaults(self):
        global httperf0
        httperf0 = Httperf()
        assert httperf0, "Httperf did not instantiate"

    def testInitWithDefaultPathSet(self):
        self.assertEqual(httperf0.path, "httperf")

    def testInitHttperfWithPath(self):
        global httperf1
        httperf1 = Httperf(path=unit_helper.httperf_path)
        assert httperf1, "Httperf did not instantiate with "+httperf_path

    def testInitWithPathSetsPath(self):
        self.assertEqual(httperf1.path, unit_helper.httperf_path)

    def testInitWithArgsCreatesArg(self):
        # arg method
        httperf = Httperf('hog', server="localhost", port=8080, num_conns=100)
        self.assertEqual(httperf.params["server"], "localhost")
        self.assertEqual(('--hog' in httperf.args), True)

        res_str = httperf.run()
        m = re.search('--hog', res_str)
        self.assertNotEqual(m, None)

        m = re.search('--hog=', res_str)
        self.assertEqual(m, None)

        # param method
        httperf = Httperf(hog=True, server="localhost", port=8080, num_conns=100)
        self.assertEqual(httperf.params["hog"], True)

        res_str = httperf.run()
        m = re.search('--hog', res_str)
        self.assertNotEqual(m, None)

        m = re.search('--hog=', res_str)
        self.assertEqual(m, None)

    def testInitWithBadKwArg(self):
        with self.assertRaises(Exception):
            Httperf(bad="localhost")

    def testInitWithBadArg(self):
        with self.assertRaises(Exception):
            Httperf('bad')

    def testUpdateOptionAlreadyCreated(self):
        httperf = Httperf(server="localhost")
        httperf.update_option("server", "newhost")
        self.assertEqual(httperf.params["server"], "newhost")

    def testUpdateOptionNotYetCreated(self):
        httperf = Httperf()
        httperf.update_option("server", "newhost")
        self.assertEqual(httperf.params["server"], "newhost")

    def testRun(self):
        httperf = Httperf()
        global res_string
        res_string = httperf.run()

        self.assertNotEqual(res_string, None)

    def testRunResultsNotEmpty(self):
        self.assertNotEqual(res_string, "")

    def testRunResultsContainsHttperfResults(self):
        m = re.search('httperf --client=0/1 --server=localhost', res_string)
        self.assertNotEqual(m, None)

    def testRunResultsWithParser(self):
        httperf = Httperf()
        httperf.parser = True
        results = httperf.run()
        self.assertEqual(len(results), 50)

    def testSluggedCmdArgs(self):
        httperf = Httperf(server="localhost", num_conns="5")
        results = httperf.run()
        m = re.search('unrecognized option \'--num_conns=5\'', results)
        self.assertEqual(m, None)


if __name__ == "__main__":
    unittest.main()
