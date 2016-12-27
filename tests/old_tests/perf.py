#!/usr/bin/python

import os
import time
import catbox

def tryWrite(path):
    try:
        f = file(path, "w")
        f.write("hello world\n")
        f.close()
    except Exception, e:
        pass

def timeWrite(path):
    start = time.time()
    for i in range(10000):
        tryWrite(path)
    end = time.time()
    return end - start

def goodCase():
    print "Sandbox with allowed path:   %f" % timeWrite("catboxtest.txt")

def badCase():
    print "Sandbox with forbidden path: %f" % timeWrite("/tmp/catboxtest.txt")

def test():
    allowed = [os.getcwd()]
    print "Normal open/write time:      %f" % timeWrite("catboxtest.txt")
    catbox.run(goodCase, allowed)
    catbox.run(badCase, allowed)

test()
