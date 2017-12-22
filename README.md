# CATBOX
[![Build Status](https://travis-ci.org/Zaryob/catbox.svg?branch=master)](https://travis-ci.org/Zaryob/catbox)

a sandbox compilation environment for package manager

Sandbox is a mechanism which provides a tighly-controlled set of resources
for guest programs to run in. They are used for security and testing purposes.

There are many uses for a sandbox in package management:

1. In the build process, we dont want packages to modify host operating
system while they are configuring or compiling. We want them to be
constrained in their temporary build directory. Denying the write
operations outside of this directory is most basic application of
a sandbox here.

2. Instead of actually doing some operations like changing permission or
ownership of files, we can just log them, and mark in the generated package.
That way we dont need to be a root user in order to build a package with
such properties.

3. We can log many build operations, and analyze them to see actual
build dependencies of the package for example.

4. We can build package in a temporary directory, but let it think that
in runs in root directory with everything out there is read only mapped
inside. That can greatly simplify build scripts.

There are two ways to sandbox a build script within user context without
resorting to special kernel modules:

1. We can override functions of glibc with LD_PRELOAD. Since this requires
executing the script inside a new shell, passing Python variables between
builder and scripts is hacky and cumbersome.

2. We can intercept system calls with kernel ptrace interface.

We decided to go with the latter way. There is already a good ptrace
sandbox framework for Python, called Subterfugue. Apart from a small
C binding for ptrace call, it is completely written in Python.
Unfortunately dealing with each system call with Python is quite
slow for a build farm. There are over a thousand packages, and
some of them like OpenOffice.org or kdebase takes a huge time to
compile even on high end computers.

Thus we wrote catbox, a small sandboxing module for PiSi (the package
manager for Pardus Linux). It is completely written in C, and it wont
provide custom system call hooks or advanced modifications to the
guest environment like Subterfugue.

Although, catbox started as a sandboxing module for PiSi, it is now a
more generic sandbox module that can be used generically.

Event Hooks:

* child_initialized(pid): Event hook is called on parent process but
  after child is initialized to be traced and before parent notifies
  child to continue. Pid of the child process is given as the only
  argument. (available in version 1.4+)

* child_died_unexpectedly(pid): Event hook is called when we got a
  signal/event from a child but the child was already dead. Pid of the
  child process is given as the only argument. (available in version
  1.4+)


Dependencies:

* pcre (OPTIONAL): If enabled path can be defined with regular
  expressions. Just append --enable-pcre to the setup.py command.

* Testify: (https://github.com/Yelp/Testify) Testify is used for
  unittests only.
