#!/usr/bin/python

import sys
import os
import catbox

dir_name = "catboxdirtest"

def test():
    file(dir_name + "/lala", "w").write("hello world\n")
    try:
        os.mkdir(dir_name + "lala")
    except Exception, e:
        if e.errno != 13:
            raise

def cleanup():
    if os.path.exists(dir_name):
        os.system("rm -rf %s/" % dir_name)
    if os.path.exists(dir_name + "lala"):
        os.system("rm -rf %slala" % dir_name)

cleanup()
os.mkdir(dir_name)

ret = catbox.run(test, [os.getcwd() + "/" + dir_name + "/"])
assert(ret.code == 0)
canonical = os.path.realpath(os.getcwd() + "/" + dir_name + "lala")
assert(ret.violations == [("mkdir", "catboxdirtestlala", canonical)])

ret = catbox.run(test, [os.getcwd() + "/" + dir_name])
assert(ret.code == 0)
assert(ret.violations == [])
cleanup()
