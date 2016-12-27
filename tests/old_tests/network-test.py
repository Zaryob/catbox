#!/usr/bin/python

import sys
import os
import catbox
import socket

def test():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

test()

ret = catbox.run(test)
assert(ret.code == 0)
assert(ret.violations == [])

ret = catbox.run(test, network=True)
assert(ret.code == 0)
assert(ret.violations == [])

ret = catbox.run(test, network=False)
assert(ret.code == 1)
assert(map(lambda x: x[0], ret.violations) == ["socketcall"])
