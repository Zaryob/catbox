#!/usr/bin/python

#test cases for catbox remode

import catbox.catbox as catbox
import os
import stat
import sys

def logger(event, data):
    print event, data

def uid():
    sys.exit(os.getuid())

def gid():
    sys.exit(os.getgid())


def run_tests():
    cr = lambda func: catbox.run(func, [os.getcwd(),'/var/tmp'], logger)

    '''remember the owner'''
    os.chown('/var/tmp/hello.txt', os.getuid(), os.getgid())
    ret = cr(lambda: os.chown('/var/tmp/hello.txt', 0, 0))
    print "(retcode, ownerships) == ", ret.ret, ret.ownerships
    assert '/var/tmp/hello.txt' in ret.ownerships.keys(), "Chown: owner change was not trapped"
    assert os.stat('/var/tmp/hello.txt').st_uid == os.getuid(), "Chown: owner was changed"

    '''remember the mode'''
    os.chmod('/var/tmp/hello.txt', int("700", 8))
    ret = cr(lambda: os.chmod('/var/tmp/hello.txt', int("755", 8)))
    print "(retcode, modes) == ",ret.ret, ret.modes
    assert '/var/tmp/hello.txt' in ret.modes.keys(), "Chmod: mode change was not trapped"
    assert stat.S_IMODE(os.stat('/var/tmp/hello.txt').st_mode) == int("700", 8), \
            "Chmod: mode was changed"


    '''return the right uid, gid'''
    ret = cr( uid )
    print "(cr(uid), uid) == ", ret.ret, os.getuid()
    assert ret.ret == 0, "getuid/getuid32 not trapped"

    ret = cr( gid )    
    print "(cr(gid), gid) == ", ret.ret, os.getgid()
    assert ret.ret == 0, "getgid/getgid32 not trapped"


run_tests()