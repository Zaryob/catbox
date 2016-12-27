/*
** Copyright (c) 2006-2007, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include "catbox.h"

#include <sys/ptrace.h>
#include <sys/user.h>
#include <linux/unistd.h>
#include <fcntl.h>
#include <stddef.h>

// System call dispatch flags
#define CHECK_PATH    1 << 0  // First argument should be a valid path
#define CHECK_PATH2   1 << 1  // Second argument should be a valid path
#define DONT_FOLLOW   1 << 2  // Don't follow last symlink in the path while checking
#define OPEN_MODE     1 << 3  // Check the Write mode of open flags
#define LOG_OWNER     1 << 4  // Don't do the chown operation but log the new owner
#define LOG_MODE      1 << 5  // Don't do the chmod operation but log the new mode
#define FAKE_ID       1 << 6  // Fake builder identity as root
#define NET_CALL      1 << 7  // System call depends on network allowed flag
#define AT_FAMILY_12  1 << 8  // First argument is a file-descriptor and second argument is a path
#define AT_FAMILY_23  1 << 9  // Second argument is a file-descriptor and third argument is a path
#define AT_FAMILY_34  1 << 10 // Third argument is a file-descriptor and fourth argument is a path
#define AT_NOFLW_ARG4 1 << 11 // Check fourth argument that contains AT_SYMLINK_NOFOLLOW flag or not
#define AT_FLW_ARG5   1 << 12 // Check fifth argument that contains AT_SYMLINK_FOLLOW flag or not
#define AT_NOFLW_ARG5 1 << 13 // Check fitth argument that contains AT_SYMLINK_NOFOLLOW flag or not

// System call dispatch table
static struct syscall_def {
	int no;
	const char *name;
	unsigned int flags;
} system_calls[] = {
	{ __NR_open,       "open",       CHECK_PATH | OPEN_MODE },
	{ __NR_creat,      "creat",      CHECK_PATH },
	{ __NR_truncate,   "truncate",   CHECK_PATH },
#ifdef __i386__
	{ __NR_truncate64, "truncate64", CHECK_PATH },
#endif
	{ __NR_unlink,     "unlink",     CHECK_PATH | DONT_FOLLOW },
	{ __NR_link,       "link",       CHECK_PATH | CHECK_PATH2 },
	{ __NR_symlink,    "symlink",    CHECK_PATH2 | DONT_FOLLOW },
	{ __NR_rename,     "rename",     CHECK_PATH | CHECK_PATH2 },
	{ __NR_mknod,      "mknod",      CHECK_PATH },
	{ __NR_chmod,      "chmod",      CHECK_PATH | LOG_MODE },
	{ __NR_lchown,     "lchown",     CHECK_PATH | LOG_MODE | DONT_FOLLOW },
	{ __NR_chown,      "chown",      CHECK_PATH | LOG_OWNER },
#ifdef __i386__
	{ __NR_lchown32,   "lchown32",   CHECK_PATH | LOG_OWNER | DONT_FOLLOW },
	{ __NR_chown32,    "chown32",    CHECK_PATH | LOG_OWNER },
#endif
	{ __NR_mkdir,      "mkdir",      CHECK_PATH },
	{ __NR_rmdir,      "rmdir",      CHECK_PATH },
	{ __NR_mount,      "mount",      CHECK_PATH },
#ifndef __i386__
	{ __NR_umount2,    "umount",     CHECK_PATH },
#else
	{ __NR_umount,     "umount",     CHECK_PATH },
#endif
	{ __NR_utime,      "utime",      CHECK_PATH },
	{ __NR_getuid,     "getuid",     FAKE_ID },
	{ __NR_geteuid,    "geteuid",    FAKE_ID },
	{ __NR_getgid,     "getgid",     FAKE_ID },
	{ __NR_getegid,    "getegid",    FAKE_ID },
#ifdef __i386__
	{ __NR_getuid32,   "getuid32",   FAKE_ID },
	{ __NR_geteuid32,  "geteuid32",  FAKE_ID },
	{ __NR_getgid32,   "getgid32",   FAKE_ID },
	{ __NR_getegid32,  "getegid32",  FAKE_ID },
#endif
#ifndef __i386__
	{ __NR_socket,     "socketcall", NET_CALL },
#else
	{ __NR_socketcall, "socketcall", NET_CALL },
#endif
	{ __NR_unlinkat,   "unlinkat",   AT_FAMILY_12 | DONT_FOLLOW},
	{ __NR_mknodat,    "mknodat",    AT_FAMILY_12},
	{ __NR_renameat,   "renameat",   AT_FAMILY_12 | AT_FAMILY_34},
	{ __NR_openat,     "openat",     AT_FAMILY_12 | OPEN_MODE},
	{ __NR_linkat,     "linkat",     AT_FAMILY_12 | AT_FAMILY_34 | AT_FLW_ARG5},
	{ __NR_utimensat,  "utimensat",  AT_FAMILY_12 | AT_NOFLW_ARG4},
	{ __NR_mkdirat,    "mkdirat",    AT_FAMILY_12 },
	{ __NR_symlinkat,  "symlinkat",  AT_FAMILY_23 | DONT_FOLLOW},
	{ __NR_fchmodat,   "fchmodat",   AT_FAMILY_12 | AT_NOFLW_ARG4 | LOG_MODE},
	{ __NR_fchownat,   "fchownat",   AT_FAMILY_12 | AT_NOFLW_ARG5 | LOG_OWNER},

	{ 0, NULL, 0 }
};

// Architecture dependent register offsets

#define OFFSET_OF_REG(x) offsetof(struct user_regs_struct, x)

#ifndef __i386__
// x64
#define REG_ARG1 OFFSET_OF_REG(rdi)
#define REG_ARG2 OFFSET_OF_REG(rsi)
#define REG_ARG3 OFFSET_OF_REG(rdx)
#define REG_ARG4 OFFSET_OF_REG(r10)
#define REG_ARG5 OFFSET_OF_REG(r8)
#define REG_CALL orig_rax
#define REG_ERROR rax
#else
// i386
#define REG_ARG1 OFFSET_OF_REG(ebx)
#define REG_ARG2 OFFSET_OF_REG(ecx)
#define REG_ARG3 OFFSET_OF_REG(edx)
#define REG_ARG4 OFFSET_OF_REG(esi)
#define REG_ARG5 OFFSET_OF_REG(edi)
#define REG_CALL orig_eax
#define REG_ERROR eax
#endif

static char *
get_str(pid_t pid, unsigned long peekregister)
{
	static char ret[PATH_MAX];
	int i = 0;
	char ch;
	if(peekregister == 0)
		ret[0] = '\0';
	else {
		do {
			ch = (char) ptrace(PTRACE_PEEKDATA, pid, peekregister + i, NULL);
			ret[i] = ch;
			++i;
		} while(ch);
		ret[i] = '\0';
	}

	return ret;
}

static char *
get_pid_fd_path(pid_t pid, int fd)
{
	static char ret[PATH_MAX];
	ssize_t len;
	char procpath[128];

	if(fd == AT_FDCWD)
		sprintf(procpath, "/proc/%i/cwd", pid);
	else
		sprintf(procpath, "/proc/%i/fd/%i", pid, fd);

	len = readlink(procpath, ret, PATH_MAX);
	ret[len] = '\0';

	return ret;
}

static int
path_arg_writable(struct trace_context *ctx, pid_t pid, char *path, const char *name, int dont_follow)
{
	char *canonical;
	int ret;
	int mkdir_case;
	int err = 0;

	canonical = catbox_paths_canonical(pid, path, dont_follow);
	if (canonical) {
		mkdir_case = strcmp("mkdir", name) == 0;
		ret = path_writable(ctx->pathlist, canonical, mkdir_case);
		if (ret == 0) {
			if (strcmp("open", name) == 0) {
				// Special case for kernel build
				unsigned int flags;
				struct stat st;
				flags = ptrace(PTRACE_PEEKUSER, pid, REG_ARG2, 0);
				if ((flags & O_CREAT) == 0 && stat(canonical, &st) == -1 && errno == ENOENT) {
					free(canonical);
					return ENOENT;
				}
			}
			catbox_retval_add_violation(ctx, name, path, canonical);
			err = -EACCES;
		} else if (ret == -1) {
			err = -EEXIST;
		}
		free(canonical);
	} else {
		if (errno == ENAMETOOLONG)
			err = -ENAMETOOLONG;
		else if (errno == ENOENT)
			err = -ENOENT;
		else
			err = -EACCES;
	}

	return err;
}

static int
handle_syscall(struct trace_context *ctx, pid_t pid, int syscall)
{
	int i;
	int ret;
	unsigned long arg;
	char *path;
	unsigned int flags;
	unsigned int oflags;
	const char *name;

	char at_path[PATH_MAX];
	int at_nofollow;
	int fd;
	char *fdpath;

	for (i = 0; system_calls[i].name; i++) {
		if (system_calls[i].no == syscall)
			goto found;
	}
	return 0;
found:
	flags = system_calls[i].flags;
	name = system_calls[i].name;

	if (flags & CHECK_PATH) {
		arg = ptrace(PTRACE_PEEKUSER, pid, REG_ARG1, 0);
		path = get_str(pid, arg);
		if (flags & OPEN_MODE) {
			oflags = ptrace(PTRACE_PEEKUSER, pid, REG_ARG2, 0);
			if (!(oflags & O_WRONLY || oflags & O_RDWR)) return 0;
		}
		ret = path_arg_writable(ctx, pid, path, name, flags & DONT_FOLLOW);
		if (ret) return ret;
	}

	if (flags & CHECK_PATH2) {
		arg = ptrace(PTRACE_PEEKUSER, pid, REG_ARG2, 0);
		path = get_str(pid, arg);
		ret = path_arg_writable(ctx, pid, path, name, flags & DONT_FOLLOW);
		if (ret) return ret;
	}

	if (flags & AT_FAMILY_12) {
		arg = ptrace(PTRACE_PEEKUSER, pid, REG_ARG2, 0);
		path = get_str(pid, arg);

		if(flags & AT_FLW_ARG5) //for linkat() call
			at_nofollow = (ptrace(PTRACE_PEEKUSER, pid, REG_ARG5, 0) & AT_SYMLINK_FOLLOW) ? 0 : 1;
		else if (flags & AT_FLW_ARG5) //for fchownat() and fchmodat() calls
			at_nofollow = (ptrace(PTRACE_PEEKUSER, pid, REG_ARG5, 0) & AT_SYMLINK_NOFOLLOW) ? 1 : 0;
		else if(flags & AT_NOFLW_ARG4) //for utimensat() call
			at_nofollow = (ptrace(PTRACE_PEEKUSER, pid, REG_ARG4, 0) & AT_SYMLINK_NOFOLLOW) ? 1 : 0;
		else
			at_nofollow = flags & DONT_FOLLOW;

		if (flags & OPEN_MODE) {
			oflags = ptrace(PTRACE_PEEKUSER, pid, REG_ARG3, 0);
			if (!(oflags & O_WRONLY || oflags & O_RDWR)) return 0;
		}

		if(path[0] != '/' && *path != 0) {
			fd = ptrace(PTRACE_PEEKUSER, pid, REG_ARG1, 0);
			fdpath = get_pid_fd_path(pid, fd);
			sprintf(at_path, "%s/%s", fdpath, path);
			ret = path_arg_writable(ctx, pid, at_path, name, at_nofollow);
		} else if (path[0] != '/' && *path == 0) {
			fd = ptrace(PTRACE_PEEKUSER, pid, REG_ARG1, 0);
			fdpath = get_pid_fd_path(pid, fd);
			ret = path_arg_writable(ctx, pid, fdpath, name, at_nofollow);
		} else
			ret = path_arg_writable(ctx, pid, path, name, at_nofollow);

		if (ret) return ret;
	}

	if (flags & AT_FAMILY_23) {
		arg = ptrace(PTRACE_PEEKUSER, pid, REG_ARG3, 0);
		path = get_str(pid, arg);

		if(path[0] != '/' && *path != 0) {
			fd = ptrace(PTRACE_PEEKUSER, pid, REG_ARG2, 0);
			fdpath = get_pid_fd_path(pid, fd);
			sprintf(at_path, "%s/%s", fdpath, path);
			ret = path_arg_writable(ctx, pid, at_path, name, flags & DONT_FOLLOW);
		} else if (path[0] != '/' && *path == 0) {
			fd = ptrace(PTRACE_PEEKUSER, pid, REG_ARG2, 0);
			fdpath = get_pid_fd_path(pid, fd);
			ret = path_arg_writable(ctx, pid, fdpath, name, flags & DONT_FOLLOW);
		} else
			ret = path_arg_writable(ctx, pid, path, name, flags & DONT_FOLLOW);
		if (ret) return ret;
	} else if (flags & AT_FAMILY_34) {
		arg = ptrace(PTRACE_PEEKUSER, pid, REG_ARG4, 0);
		path = get_str(pid, arg);

		if(path[0] != '/' && *path != 0) {
			fd = ptrace(PTRACE_PEEKUSER, pid, REG_ARG3, 0);
			fdpath = get_pid_fd_path(pid, fd);
			sprintf(at_path, "%s/%s", fdpath, path);
			ret = path_arg_writable(ctx, pid, at_path, name, flags & DONT_FOLLOW);
		} else if (path[0] != '/' && *path == 0) {
			fd = ptrace(PTRACE_PEEKUSER, pid, REG_ARG3, 0);
			fdpath = get_pid_fd_path(pid, fd);
			ret = path_arg_writable(ctx, pid, fdpath, name, flags & DONT_FOLLOW);
		} else
			ret = path_arg_writable(ctx, pid, path, name, flags & DONT_FOLLOW);

		if (ret) return ret;
	}

	if (flags & NET_CALL && !ctx->network_allowed) {
		catbox_retval_add_violation(ctx, name, "", "");
		return -EACCES;
	}

return 0;
    //below we only trap changes to owner/mode within the fishbowl. 
    // The rest are taken care of in the above blocks
    if(0 & LOG_OWNER) {
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, pid, 0, &regs);
//        const char* path = get_str(pid, regs.ebx);
//        uid_t uid = (uid_t)regs.ecx;
//        gid_t gid = (gid_t)regs.edx;
//        PyObject* dict = PyObject_GetAttrString( ctx->ret_object, "ownerships" );
//        PyDict_SetItem( dict, PyString_FromString(path), PyTuple_Pack( 2, PyInt_FromLong(uid), PyInt_FromLong(gid)) );
        return 1;
    }
    if(0 & LOG_MODE) {
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, pid, 0, &regs);
//        const char* path = get_str(pid, regs.ebx);
//        mode_t mode = (mode_t)regs.ecx;
//        PyObject* dict = PyObject_GetAttrString( ctx->ret_object, "modes" );
//        PyDict_SetItem( dict, PyString_FromString(path), PyInt_FromLong(mode) );
        return 1;
    }
    if(0 & FAKE_ID) {
        return 2;
    }
	return 0;
}

void
catbox_syscall_handle(struct trace_context *ctx, struct traced_child *kid)
{
	int syscall;
	struct user_regs_struct regs;
	pid_t pid;

	pid = kid->pid;
	ptrace(PTRACE_GETREGS, pid, 0, &regs);
	syscall = regs.REG_CALL;

	if (kid->in_syscall) { // returning from syscall
		if (syscall == 0xbadca11) {
			// restore real call number, and return our error code
			regs.REG_ERROR = kid->error_code;
			regs.REG_CALL = kid->orig_call;
			ptrace(PTRACE_SETREGS, pid, 0, &regs);
		}
		kid->in_syscall = 0;
	} else { // entering syscall
		kid->in_syscall = 1;

		// skip extra sigtrap from execve call
		if (syscall == __NR_execve) {
			kid->in_execve = 1;
			goto out;
		}

		int ret = handle_syscall(ctx, pid, syscall);
		if (ret != 0) {
			kid->error_code = ret;
			kid->orig_call = regs.REG_CALL;
			if (!ctx->collect_only) {
				// prevent the call by giving an invalid call number
				regs.REG_CALL = 0xbadca11;
				ptrace(PTRACE_SETREGS, pid, 0, &regs);
			}
		}
	}
out:
	// continue tracing
	ptrace(PTRACE_SYSCALL, pid, 0, 0);
}
