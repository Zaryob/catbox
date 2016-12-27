#!/usr/bin/python

import sys
import os
import catbox

cur = os.getcwd()

def test():
    os.chdir(cur)
    try:
        file("/proc/self/cwd/catboxtest.deleteme", "w").write("Hello world\n")
    except Exception, e:
        print e

def parent():
    os.chdir("/var")
    ret = catbox.run(test, [cur])
    assert(ret.code == 0)
    assert(ret.violations == [])

parent()
