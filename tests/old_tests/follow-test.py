#!/usr/bin/python

import os
import sys
import catbox

good_path = "catboxtest.link"
bad_path = "catboxtest2.link"

def good_op():
    os.lchown(good_path, os.getuid(), os.getgid())
    os.unlink(good_path)
    os.symlink("/etc/passwd", good_path)

def bad_op():
    try:
        file(bad_path, "w").write("hello world\n")
    except:
        pass
    try:
        os.chown(bad_path, os.getuid(), os.getgid())
    except:
        pass
    os.symlink("/etc/passwd", "/tmp/catboxtest.link")

def mklink(dest, source):
    if os.path.exists(source):
        os.unlink(source)
    os.symlink(dest, source)

mklink("/var", good_path)
mklink("/tmp/hede", bad_path)

ret = catbox.run(good_op, writable_paths=[os.getcwd()])
assert(ret.code == 0)
assert(ret.violations == [])

ret = catbox.run(bad_op, writable_paths=[os.getcwd()])
assert(ret.code == 1)
vio = [x[0] for x in ret.violations]
assert(vio == ["open", "chown32", "symlink"] or vio == ["open", "chown", "symlink"])
