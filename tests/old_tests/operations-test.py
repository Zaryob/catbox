#!/usr/bin/python

import sys
import os
import catbox
import stat

def tryOp(name, op, path):
    try:
        op(path)
        print "Sandbox violation: %s '%s'" % (name, path)
    except IOError, e:
        if e.errno == 13:
            return 0
    except OSError, e:
        if e.errno == 13:
            return 0
    return 1

def test():
    ret = 0
    ret += tryOp("writing", lambda x: file(x, "w").write("lala"), "catboxtest.txt")
    ret += tryOp("deleting", os.unlink, "catboxtest.deleteme")
    ret += tryOp("chmoding", lambda x: os.chmod(x, stat.S_IEXEC), "catboxtest.deleteme")
    ret += tryOp("chowning", lambda x: os.chown(x, 1000, 100), "catboxtest.deleteme")
    ret += tryOp("hardlinking", lambda x: os.link(x, x + ".hlink"), "catboxtest.deleteme")
    ret += tryOp("symlinking", lambda x: os.symlink(x, x + ".slink"), "catboxtest.deleteme")
    ret += tryOp("utiming", lambda x: os.utime(x, None), "catboxtest.deleteme")
    ret += tryOp("renaming", lambda x: os.rename(x, x + ".renamed"), "catboxtest.deleteme")
    ret += tryOp("mkdiring", os.mkdir, "catboxtestdir")
    ret += tryOp("rmdiring", os.rmdir, "catboxtestdir.deleteme")
    sys.exit(ret)

if os.path.isdir("catboxtestdir.deleteme"):
    os.rmdir("catboxtestdir.deleteme")
os.mkdir("catboxtestdir.deleteme")
file("catboxtest.deleteme", "w").write("deleteme\n")

ret = catbox.run(test, writable_paths=[])
sys.exit(ret.code)

