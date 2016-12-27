#!/usr/bin/python

import sys
import catbox

class Test:
    def __init__(self, path):
        self.path = path
    
    def write(self):
        try:
            file(self.path, "w").write("lala\n")
        except IOError, e:
            if e.errno == 13:
                sys.exit(0)
        sys.exit(2)
    
    def boxedWrite(self):
        ret = catbox.run(self.write)
        if ret.code:
            print "Sandbox error"
            sys.exit(2)

test = Test("catboxtest.deleteme")
test.boxedWrite()

