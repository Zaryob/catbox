#!/usr/bin/python

import os
import sys
import catbox
import threading

def test():
    try:
        file("lala", "w").write("hello world\n")
    except IOError as e:
        if e.errno != 13:
            raise

def main():
    a = threading.Thread(target=test)
    b = threading.Thread(target=test)
    a.start()
    b.start()
    test()
    a.join()
    test()
    b.join()

ret = catbox.run(main)
assert(ret.code == 0)
canonical = os.path.realpath(os.getcwd() + "/lala")
assert(len([x for x in ret.violations if x == ("open", "lala", canonical)]) == 4)
