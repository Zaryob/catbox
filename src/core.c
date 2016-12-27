/*
** Copyright (c) 2006-2007, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include "catbox.h"

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <fcntl.h>
#include <errno.h>

static int child_pid = 0;
static int watchdog_pid = 0;

static void
run_python_callable(PyObject *callable, PyObject *args)
{
	PyObject *ret = PyObject_Call(callable, args, NULL);

	if (!ret) {
		PyObject *e;
		PyObject *val;
		PyObject *tb;
		PyErr_Fetch(&e, &val, &tb);
		if (PyErr_GivenExceptionMatches(e, PyExc_SystemExit)) {
			if (PyLong_Check(val)) {
				// Callable exits by sys.exit(n)
				exit(PyLong_AsLong(val));
			} else {
				// Callable exits by sys.exit()
				exit(2);
			}
		}
		// Callable exits by unhandled exception
		// So let child print error and value to stderr
		PyErr_Display(e, val, tb);
		exit(EXIT_FAILURE);
	}
}

static void
run_event_hook(struct trace_context *ctx, const char *hook_name, PyObject *args)
{
	PyObject *py_hook_name = PyBytes_FromString(hook_name);
	if (ctx->event_hooks && PyDict_Contains(ctx->event_hooks, py_hook_name) == 1) {
		PyObject *hook = PyDict_GetItem(ctx->event_hooks, py_hook_name);
		run_python_callable(hook, args);
	}
}

static void
watchdog(struct trace_context *ctx, int watchdog_read_fd) {
	char buf;
	// Block on reading from wathcdog pipe.
	int nread = read(watchdog_read_fd, &buf, 1);

	if (nread == 0) {
#ifdef DEBUG
		fprintf(stderr, "BORKBORK: Parent died! Watchdog is killing the traced process.\n");
#endif
		kill(child_pid, SIGKILL);
	}
	exit(0);
}

static void
start_watchdog(struct trace_context *ctx)
{
	int watchdog_fds[2];

	if (pipe(watchdog_fds) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	watchdog_pid = fork();
	if (!watchdog_pid) { // Watchdog
		close(watchdog_fds[1]); // Close the write end of the pipe.
		watchdog(ctx, watchdog_fds[0]); // Watchdog will poll on the read end.
	} else { // Parent (catbox) process
		close(watchdog_fds[0]); // Close the read end of the pipe.
	}
}

static void
setup_kid(struct traced_child *kid)
{
	int e;

	// We want to trace all sub children, and want special notify
	// to distinguish between normal sigtrap and syscall sigtrap.
	e = ptrace(PTRACE_SETOPTIONS,
		kid->pid,
		0,
		PTRACE_O_TRACESYSGOOD
		| PTRACE_O_TRACECLONE
		| PTRACE_O_TRACEFORK
		| PTRACE_O_TRACEVFORK
	);
	if (e != 0) {
		fprintf(stderr, "ptrace opts error for pid (%d): %s\n", kid->pid, strerror(errno));
	}
	kid->need_setup = 0;
}

static int
pid_hash(pid_t pid)
{
	return ((unsigned long) pid) % PID_TABLE_SIZE;
}

static struct traced_child *
find_child(struct trace_context *ctx, pid_t pid)
{
	int hash;
	struct traced_child *kid;

	hash = pid_hash(pid);
	for (kid = ctx->children[hash]; kid; kid = kid->next) {
		if (kid->pid == pid) return kid;
	}
	return NULL;
}

static struct traced_child *
add_child(struct trace_context *ctx, pid_t pid)
{
	struct traced_child *kid;
	int hash;

	kid = find_child(ctx, pid);
	if (kid) {
		fprintf(stderr, "BORKBORK: Trying to add existing child\n");
	}

	kid = malloc(sizeof(struct traced_child));
	memset(kid, 0, sizeof(struct traced_child));
	kid->pid = pid;
	kid->need_setup = 1;

	hash = pid_hash(pid);

	if (!ctx->children[hash]) {
		ctx->children[hash] = kid;
	} else {
		kid->next = ctx->children[hash];
		ctx->children[hash] = kid;
	}

	ctx->nr_children++;

	return kid;
}

static void
rem_child(struct trace_context *ctx, pid_t pid)
{
	struct traced_child *kid, *temp;
	int hash;

	if (pid == watchdog_pid) {
		return;
	}

	hash = pid_hash(pid);
	kid = ctx->children[hash];
	if (kid) {
		if (kid->pid == pid) {
			ctx->children[hash] = kid->next;
			free(kid);
			ctx->nr_children--;
			return;
		} else {
			temp = kid;
			for (kid = kid->next; kid; kid = kid->next) {
				if (kid->pid == pid) {
					temp->next = kid->next;
					free(kid);
					ctx->nr_children--;
					return;
				}
				temp = kid;
			}
		}
	}
	puts("BORKBORK: trying to remove non-tracked child");
}

enum {
	E_SETUP = 0,
	E_SETUP_PREMATURE,
	E_SYSCALL,
	E_FORK,
	E_EXECV,
	E_GENUINE,
	E_EXIT,
	E_EXIT_SIGNAL,
	E_UNKNOWN
};

static int
decide_event(struct trace_context *ctx, struct traced_child *kid, int status)
{
	unsigned int event;
	int sig;

	// We got a signal from child, and want to know the cause of it
	if (WIFSTOPPED(status)) {
		// 1. reason: Execution of child stopped by a signal
		sig = WSTOPSIG(status);
		if (sig == SIGSTOP && kid && kid->need_setup) {
			// 1.1. reason: Child is born and ready for tracing
			return E_SETUP;
		}
		if (sig == SIGSTOP && !kid) {
			// 1.2. reason: Child is born before fork event, and ready for tracing
			return E_SETUP_PREMATURE;
		}
		if (sig & SIGTRAP) {
			// 1.3. reason: We got a signal from ptrace
			if (sig == (SIGTRAP | 0x80)) {
				// 1.3.1. reason: Child made a system call
				return E_SYSCALL;
			}
			event = (status >> 16) & 0xffff;
			if (event == PTRACE_EVENT_FORK
				|| event == PTRACE_EVENT_VFORK
				|| event == PTRACE_EVENT_CLONE) {
				// 1.3.2. reason: Child made a fork
				return E_FORK;
			}
			if (kid && kid->in_execve) {
				// 1.3.3. reason: Spurious sigtrap after execve call
				return E_EXECV;
			}
		}
		// 1.4. reason: Genuine signal directed to the child itself
		return E_GENUINE;
	} else if (WIFEXITED(status)) {
		// 2. reason: Child is exited normally
		return E_EXIT;
	} else if (WIFSIGNALED(status)) {
		// 3. reason: Child is terminated by a signal
		return E_EXIT_SIGNAL;
	}
	return E_UNKNOWN;
}

static PyObject *
core_trace_loop(struct trace_context *ctx)
{
	int status;
	int event;
	pid_t pid;
	pid_t kpid;
	int e;
	long retcode = 0;
	struct traced_child *kid;

	while (ctx->nr_children) {
		pid = waitpid(-1, &status, __WALL);
		if (pid == (pid_t) -1) return NULL;
		kid = find_child(ctx, pid);

		event = decide_event(ctx, kid, status);
		if (!kid && event != E_SETUP_PREMATURE && pid != watchdog_pid) {
			// This shouldn't happen
			fprintf(stderr, "BORKBORK: nr %d, pid %d, status %x, event %d\n", ctx->nr_children, pid, status, event);

			PyObject *args = PyTuple_New(1);
			PyTuple_SetItem(args, 0, PyLong_FromLong(pid));
			run_event_hook(ctx, "child_died_unexpectedly", args);
		}

		switch (event) {
			case E_SETUP:
				setup_kid(kid);
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
				break;
			case E_SETUP_PREMATURE:
				kid = add_child(ctx, pid);
				setup_kid(kid);
				break;
			case E_SYSCALL:
				if (kid) catbox_syscall_handle(ctx, kid);
				break;
			case E_FORK:
				e = ptrace(PTRACE_GETEVENTMSG, pid, 0, &kpid); //get the new kid's pid
				if (e != 0) {
					fprintf(stderr, "geteventmsg %s\n", strerror(e));
				}
				if (find_child(ctx, kpid)) {
					// Kid is prematurely born, let it continue its life
					ptrace(PTRACE_SYSCALL, kpid, 0, 0);
				} else {
					// Add the new kid (setup will be done later)
					add_child(ctx, kpid);
				}
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
				break;
			case E_EXECV:
				kid->in_execve = 0;
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
				break;
			case E_GENUINE:
				ptrace(PTRACE_SYSCALL, pid, 0, (void*) WSTOPSIG(status));
				break;
			case E_EXIT:
				if (kid == ctx->first_child) {
					// If it is our first child, keep its return value
					retcode = WEXITSTATUS(status);
				}
				rem_child(ctx, pid);
				break;
			case E_EXIT_SIGNAL:
				ptrace(PTRACE_SYSCALL, pid, 0, (void*) WTERMSIG(status));
				retcode = 1;
				rem_child(ctx, pid);
				break;
			case E_UNKNOWN:
				fprintf(stderr, "BORKBORK: Unknown signal %x pid %d\n", status, pid);
				break;
		}
	}
	catbox_retval_set_exit_code(ctx, retcode);
	return ctx->retval;
}

static void terminate_child(void)
{
	if (child_pid) {
		kill(child_pid, SIGTERM);
	}
}

static void sigterm(int sig)
{
	if (sig == SIGTERM) {
		terminate_child();
	}
	exit(EXIT_FAILURE);
}

static void sigint(int sig)
{
	if (sig == SIGINT) {
		raise(SIGTERM);
	}
}

// Syncronization value, it has two copies in parent and child's memory spaces
static sig_atomic_t volatile got_sig = 0;

static void sigusr1(int dummy)
{
	got_sig = 1;
}

PyObject *
catbox_core_run(struct trace_context *ctx)
{
	struct traced_child *kid;
	pid_t pid;

	got_sig = 0;
	signal(SIGUSR1, sigusr1);

	pid = fork();
	if (pid == (pid_t) -1) {
		PyErr_SetString(PyExc_RuntimeError, "fork failed");
		return NULL;
	}

	if (pid == 0) {
		// child process comes to life
		PyObject *args;

		// set up tracing mode
		ptrace(PTRACE_TRACEME, 0, 0, 0);
		// tell the parent we are ready
		kill(getppid(), SIGUSR1);
		// wait until parent tells us to continue
		while (!got_sig);

		// let the callable do its job
		args = ctx->func_args;
		if (!args) args = PyTuple_New(0);
		run_python_callable(ctx->func, args);

		// Callable exits by returning from function normally
		exit(0);
	}

	// parent process continues

	// wait until child set ups tracing mode, and sends a signal
	while (!got_sig);

	// when we're interrupted child shouldn't continue on
	// running. pass the signal on to child...
	child_pid = pid;
	signal(SIGINT, sigint);
	signal(SIGTERM, sigterm);
	atexit(terminate_child);

	// Run child_initialized hook before notifying child to continue.
	PyObject *args = PyTuple_New(1);
	PyTuple_SetItem(args, 0, PyLong_FromLong(child_pid));
	run_event_hook(ctx, "child_initialized", args);

	// Start watchdog process
	start_watchdog(ctx);

	// tell the kid that it can start given callable now
	kill(child_pid, SIGUSR1);


	waitpid(child_pid, NULL, 0);

	kid = add_child(ctx, child_pid);
	setup_kid(kid);
	ctx->first_child = kid;
	ptrace(PTRACE_SYSCALL, child_pid, 0, (void *) SIGUSR1);

	return core_trace_loop(ctx);
}
