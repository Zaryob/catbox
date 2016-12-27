#!/usr/bin/python

import sys
import os
import catbox

normal_count = None

def testMaxPath():
    count = 1
    while True:
        name = "a" * count
        try:
            if os.path.exists(name):
                os.rmdir(name)
            os.mkdir(name)
            os.rmdir(name)
        except OSError, e:
            if e.errno == 36:
                if normal_count != None and count != normal_count:
                    print "Expected count %d, calculated %d" % (normal_count, count)
                    sys.exit(1)
                return count
            print e
            raise
        count += 1

normal_count = testMaxPath()

ret = catbox.run(testMaxPath, [os.getcwd()])
assert(ret.code == 0)
assert(ret.violations == [])
