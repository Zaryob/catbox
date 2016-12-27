#!/usr/bin/python

import os
import sys

base = os.path.dirname(sys.argv[0])
for name in os.listdir(base):
    if name.endswith("-test.py"):
        print name
        os.system(os.path.join(base, name))
