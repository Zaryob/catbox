#!/usr/bin/python

import sys
import os
import catbox

# Test case which checks whether kernel module compilation fails or not

def good_open_syscall():
    # Shouldn't bork because file doesn't exist, open returns -1
    f = os.open("/usr/src/linux/null.gcda", os.O_RDWR)

def bad_open_syscall():
    # Should bork because it creates the file if it doesn't exist
    f = os.open("/usr/src/linux/null.gcda", os.O_RDWR | os.O_CREAT)

ret = catbox.run(good_open_syscall, writable_paths=[os.getcwd()])
assert(ret.code == 0)
assert(ret.violations == [])

ret = catbox.run(bad_open_syscall, writable_paths=[os.getcwd()])
assert(ret.code != 0)
assert(ret.violations != [])
