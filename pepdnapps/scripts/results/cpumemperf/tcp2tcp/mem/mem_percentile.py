from optparse import OptionParser
import numpy as np

# This version is adapted for mem
__VERSION__ = 0.2


class percentile:
    """ Calculated 10th, 50th and 90th percentiles of an array
        Properties:
            tenth    - returns the tenth percentile
            median   - returns the median (or 50th percentile)
            nintieth - returns the nintieth percentile
    """

    def __init__(self, array=None):
        self.tenth = 0
        self.median = 0
        self.nintieth = 0

        self.__call__(array)

    def calculate(self, array):
        if array is None:
            return
        self.tenth = np.percentile(array, 10)
        self.median = np.percentile(array, 50)
        self.nintieth = np.percentile(array, 90)

    def __call__(self, array):
        self.calculate(array)

    def printt(self):
        print("Value {} {}".format(self.median, self.nintieth - self.tenth))


def main():
    usage = "usage: python %prog filename"
    version = "%%prog %s" % __VERSION__

    parser = OptionParser(usage=usage, version=version)
    (options, args) = parser.parse_args()
    if len(args) < 1:
        print(parser.error("need a filename, -h/--help for help"))
        raise SystemExit

    obj = percentile()
    imported_data = np.genfromtxt((args[0]))
    obj(imported_data)

    res = open('mem.dat', 'a+')

    res.write("{}    {}\n" .format(obj.median, obj.nintieth - obj.tenth))


if __name__ == '__main__':
    main()
