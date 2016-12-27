#!/usr/bin/python

import os
import catbox
import sys

def tryWrite(who, path="/tmp/catboxtest.txt"):
    try:
        f = file(path, "w")
        f.write("hello world\n")
        f.close()
    except IOError, e:
        if e.errno == 13:
            return 0
        print "Sandbox error in %s: cannot write '%s': %s" % (who, path, e)
        return 1
    
    print "Sandbox violation in %s: wrote '%s'" % (who, path)
    return 1

def testNode(level):
    pid = 1
    if level < 3:
        pid = os.fork()
        if pid == -1:
            print "fork failed"
            sys.exit(0)

    if pid == 0:
        testNode(level + 1)
        tryWrite("child*%d" % level)

def test():
    tryWrite("parent")
    for i in range(7):
        testNode(1)
        tryWrite("parent")
    tryWrite("parent")

ret = catbox.run(test, writable_paths=[os.getcwd()])
#print len(ret.violations)
assert(len(ret.violations) == 8746)

