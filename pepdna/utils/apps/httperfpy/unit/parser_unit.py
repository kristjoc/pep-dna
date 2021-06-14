import unit_helper
import unittest
import re, os, sys

from unit_helper import *

class HttperfTestCase(unittest.TestCase):

    def testParse(self):
        global presults
        presults = HttperfParser.parse(httperf_results)

    def testParseResultCount(self):
        self.assertEqual(len(presults.keys()), 50)

    def testParseVerboseResultCount(self):
        vpresults = HttperfParser.parse(httperf_verbose_results)
        self.assertEqual(len(vpresults.keys()), 57)
        self.assertEqual(len(vpresults["connection_times"]), 10)

if __name__ == "__main__":
    unittest.main()

