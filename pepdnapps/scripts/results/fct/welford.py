from optparse import OptionParser
import math
import sys

__VERSION__ = 0.1

class Welford(object):
    """ Implements Welford's algorithm for computing a running mean
        and standard deviation as described at:
        http://www.johndcook.com/standard_deviation.html
        can take single values or iterables
        Properties:
            mean    - returns the mean
            std     - returns the std
            meanfull- returns the mean and std of the mean
        Usage:
            >>> foo = Welford()
            >>> foo(range(100))
            >>> foo
            <Welford: 49.5 +- 29.0114919759>
            >>> foo([1]*1000)
            >>> foo
            <Welford: 5.40909090909 +- 16.4437417146>
            >>> foo.mean
            5.409090909090906
            >>> foo.std
            16.44374171455467
            >>> foo.meanfull
            (5.409090909090906, 0.4957974674244838)
    """

    def __init__(self, x=None):
        self.k = 0
        self.M = 0
        self.S = 0

        self.__call__(x)

    def update(self, x):
        if x is None:
            return
        self.k += 1
        newM = self.M + (x - self.M) * 1. / self.k
        newS = self.S + (x - self.M) * (x - newM)
        self.M, self.S = newM, newS

    def __call__(self, x):
        self.update(x)

    @property
    def mean(self):
        return self.M

    @property
    def meanfull(self):
        return self.M, self.S / math.sqrt(self.k)

    @property
    def stddev(self):
        if self.k == 1:
            return 0
        return math.sqrt(self.S / (self.k - 1))

    def printt(self):
        print("Value {} {}".format(self.mean, self.stddev))

def main():
    usage = "usage: python %prog filename"
    version = "%%prog %s" % __VERSION__

    parser = OptionParser(usage=usage, version=version)
    (options, args) = parser.parse_args()
    if len(args) < 1:
        print(parser.error("need a filename, -h/--help for help"))
        raise SystemExit


    obj = Welford()
    for line in open(args[0]):
        obj(float(line))

    res = open('fct.dat', 'a+')

    res.write("{}    {}\n" .format(obj.mean, obj.stddev))


if __name__ == '__main__':
    main()
