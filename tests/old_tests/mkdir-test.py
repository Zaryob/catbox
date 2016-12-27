#!/usr/bin/python

# Test for mkdir -p /var/pisi/package-1.2.3/install/usr/lib problem
#
# Given that path, mkdir -p command starts creating dirs from first
# directory without even checking if it exists first.
# Even if constrained program does not have write access to topmost
# parts, we should return -EEXIST instead of -EACCES
# or mkdir -p fails.

import sys
import os
import catbox

def test():
    try:
        os.mkdir("/var")
    except OSError, e:
        if e.errno == 17:
            # We want this error code, even though we dont have write access
            sys.exit(0)
        raise

def bad_test():
    os.mkdir("/home")

ret = catbox.run(test, ["/var/pisi/test-1.2"])
assert(ret.code == 0)
assert(ret.violations == [])

ret = catbox.run(bad_test, ["/var/pisi/test-1.2"])
assert(ret.code == 1)
assert(ret.violations == [("mkdir", "/home", "/home")])
