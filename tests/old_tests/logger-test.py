#!/usr/bin/python

import sys
import os
import catbox

bad_path = "catboxtest.deleteme"

def logger(op, path, canonical):
    assert(op == "open")
    assert(path == bad_path)
    assert(canonical == os.path.realpath(os.getcwd() + "/" + bad_path))

def test():
    file(bad_path, "w").write("hello world\n")

ret = catbox.run(test, logger=logger)
assert(ret.code == 1)
assert(ret.violations != [])
