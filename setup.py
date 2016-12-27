#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2006-2008, TUBITAK/UEKAE
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version. Please read the COPYING file.
#

import sys
import os
import glob
import shutil
from distutils.core import setup, Extension
from distutils.command.install import install
from distutils.command.bdist import bdist
from distutils.command.build import build
from distutils.command.build_ext import build_ext

version='1.6.1'

distfiles = """
    setup.py
    OKUBENİ
    README
    src/*.c
    src/*.h
    tests/*.py
"""

source = [
    'src/catbox.c',
    'src/core.c',
    'src/syscall.c',
    'src/paths.c',
    'src/retval.c',
]

if 'dist' in sys.argv:
    distdir = "catbox-%s" % version
    list = []
    for t in distfiles.split():
        list.extend(glob.glob(t))
    if os.path.exists(distdir):
        shutil.rmtree(distdir)
    os.mkdir(distdir)
    for file_ in list:
        cum = distdir[:]
        for d in os.path.dirname(file_).split('/'):
            dn = os.path.join(cum, d)
            cum = dn[:]
            if not os.path.exists(dn):
                os.mkdir(dn)
        shutil.copy(file_, os.path.join(distdir, file_))
    os.popen("tar -czf %s %s" % ("catbox-" + version + ".tar.gz", distdir))
    shutil.rmtree(distdir)
    sys.exit(0)

catbox_options = [('enable-pcre', None, "Enable regular expressions in path definitions (using PCRE)")]
catbox_boolean_options = ['enable-pcre']
enable_pcre = False

class Bdist(bdist):
    user_options = bdist.user_options
    boolean_options = bdist.boolean_options
    user_options.extend(catbox_options)
    boolean_options.extend(catbox_boolean_options)

    def initialize_options(self):
        self.enable_pcre = enable_pcre
        bdist.initialize_options(self)

    def finalize_options(self):
        global enable_pcre
        enable_pcre = self.enable_pcre
        bdist.finalize_options(self)


class BuildExt(build_ext):
    user_options = build_ext.user_options
    boolean_options = build_ext.boolean_options
    user_options.extend(catbox_options)
    boolean_options.extend(catbox_boolean_options)

    def initialize_options(self):
        self.enable_pcre = enable_pcre
        build_ext.initialize_options(self)

    def finalize_options(self):
        global enable_pcre
        enable_pcre = self.enable_pcre
        build_ext.finalize_options(self)

    def build_extension(self, ext):
        global enable_pcre
        if enable_pcre:
            ext.extra_compile_args.append('-DENABLE_PCRE')
            ext.libraries=['pcre']
        ext.extra_compile_args.append('-Wall')
        ext.extra_compile_args.append('-DVERSION=%s' % version)
        build_ext.build_extension(self, ext)

class Build(build):
    user_options = build.user_options
    boolean_options = build.boolean_options
    user_options.extend(catbox_options)
    boolean_options.extend(catbox_boolean_options)

    def initialize_options(self):
        self.enable_pcre = enable_pcre
        build.initialize_options(self)

    def finalize_options(self):
        global enable_pcre
        enable_pcre = self.enable_pcre
        build.finalize_options(self)

class Install(install):
    user_options = install.user_options
    boolean_options = install.boolean_options
    user_options.extend(catbox_options)
    boolean_options.extend(catbox_boolean_options)

    def initialize_options(self):
        self.enable_pcre = enable_pcre
        install.initialize_options(self)

    def finalize_options(self):
        global enable_pcre
        # NOTE: for Pardus distribution
        if os.path.exists("/etc/pardus-release"):
            self.install_platlib = '$base/lib/pardus'
            self.install_purelib = '$base/lib/pardus'
        enable_pcre = self.enable_pcre
        install.finalize_options(self)

    def run(self):
        install.run(self)

setup(
    name='catbox',
    description='Fast sandbox implementation for Python',
    author='Pardus Linux',
    url='https://github.com/Pardus-Linux/catbox',
    version=version,
    scripts=['bin/catbox'],
    ext_modules=[
        Extension(
            'catbox',
            source,
        )
    ],
    cmdclass = {
        'install'   : Install,
        'build'     : Build,
        'build_ext' : BuildExt,
        'bdist'     : Bdist,
    }
)
