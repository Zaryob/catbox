#!/usr/bin/python

import sys
import os
import catbox

def good_read():
    file("/etc/pardus-release").read()

def good_write():
    file("catboxtest.txt", "w").write("hello world\n")

def bad_write():
    file("/tmp/catboxtest.txt", "w").write("hello world\n")

def bad_write2():
    file("%s/catboxtest.txt" % '/'.join(os.getcwd().split('/')[:-1]), "w").write("Hello world\n")

ret = catbox.run(good_read, writable_paths=[os.getcwd()])
assert(ret.code == 0)
assert(ret.violations == [])

ret = catbox.run(good_write, writable_paths=[os.getcwd()])
assert(ret.code == 0)
assert(ret.violations == [])

ret = catbox.run(bad_write, writable_paths=[os.getcwd()])
assert(ret.code == 1)
assert(len(ret.violations) == 1)
assert(ret.violations[0][0] == "open")

ret = catbox.run(bad_write2, writable_paths=[os.getcwd() + "/"])
assert(ret.code == 1)
assert(len(ret.violations) == 1)
assert(ret.violations[0][0] == "open")
