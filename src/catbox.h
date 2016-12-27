/*
** Copyright (c) 2006-2007, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include <python3.4m/Python.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#ifndef VERSION
	#warning "CATBOX_VERSION is not defined"
	#define VERSION "UNDEFINED"
#endif
#define STR_EXPAND(x) #x
#define STR(x) STR_EXPAND(x)
#define CATBOX_VERSION() STR(VERSION)

/* per process tracking data */
struct traced_child {
	/* process id of the traced kid */
	pid_t pid;
	/* we will get a stop signal from kid and will setup tracing flags */
	int need_setup;
	/* kid is in syscall */
	int in_syscall;
	/* kid called execve, and we'll get spurious sigtrap in next wait */
	int in_execve;
	/* original syscall number when a syscall is faked */
	unsigned long orig_call;
	/* faked syscall will fail with this error code */
	int error_code;
	/* used for hash table collision handling */
	struct traced_child *next;
};

#define PID_TABLE_SIZE 367

/* general tracking data */
struct trace_context {
	/* main callable */
	PyObject *func;
	/* arguments to callable */
	PyObject *func_args;
	/* violation logger function */
	PyObject *logger;
	/* Python functions that will be run by parent after child is ready to be traced. */
	PyObject *event_hooks;
	/* this object keeps everything to be returned to the caller */
	PyObject *retval;
	/* allowed path list */
	char **pathlist;
	/* is network connection allowed */
	int network_allowed;
	/* collect violations only or block syscall violations */
	int collect_only;
	/* per process data table, hashed by process id */
	unsigned int nr_children;
	struct traced_child *children[PID_TABLE_SIZE];
	/* first child pointer is kept for determining return code */
	struct traced_child *first_child;
};

char *catbox_paths_canonical(pid_t pid, char *path, int dont_follow);
int path_writable(char **pathlist, const char *canonical, int mkdir_case);
void free_pathlist(char **pathlist);
char **make_pathlist(PyObject *paths);

int catbox_retval_init(struct trace_context *ctx);
void catbox_retval_set_exit_code(struct trace_context *ctx, int retcode);
void catbox_retval_add_violation(struct trace_context *ctx, const char *operation, const char *path, const char *canonical);

PyObject *catbox_core_run(struct trace_context *ctx);

void catbox_syscall_handle(struct trace_context *ctx, struct traced_child *kid);
