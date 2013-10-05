/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include "file.h"
#include "lock.h"
#include "scratchpad.h"
#include "vnode.h"
#include "vmnt.h"

int (*call_vec[])(message *m_out) = {
	no_sys,		/*  0 = unused	*/
	no_sys,		/*  1 = (exit)	*/
	no_sys,		/*  2 = (fork)	*/
	do_read,	/*  3 = read	*/
	do_write,	/*  4 = write	*/
	do_open,	/*  5 = open	*/
	do_close,	/*  6 = close	*/
	no_sys,		/*  7 = wait	*/
	no_sys,		/*  8 = unused */
	do_link,	/*  9 = link	*/
	do_unlink,	/* 10 = unlink	*/
	no_sys,		/* 11 = waitpid	*/
	do_chdir,	/* 12 = chdir	*/
	no_sys,		/* 13 = time	*/
	do_mknod,	/* 14 = mknod	*/
	do_chmod,	/* 15 = chmod	*/
	do_chown,	/* 16 = chown	*/
	no_sys,		/* 17 = break	*/
	no_sys,		/* 18 = unused (was old stat)*/
	no_sys,		/* 19 = unused	*/
	no_sys,		/* 20 = getpid	*/
	do_mount,	/* 21 = mount	*/
	do_umount,	/* 22 = umount	*/
	no_sys,		/* 23 = (setuid) */
	no_sys,		/* 24 = getuid	*/
	no_sys,		/* 25 = (stime)	*/
	no_sys,		/* 26 = ptrace	*/
	no_sys,		/* 27 = alarm	*/
	no_sys,		/* 28 = unused (was old fstat)*/
	no_sys,		/* 29 = pause	*/
	do_utime,	/* 30 = utime	*/
	no_sys,		/* 31 = (stty)	*/
	no_sys,		/* 32 = (gtty)	*/
	do_access,	/* 33 = access	*/
	no_sys,		/* 34 = (nice)	*/
	no_sys,		/* 35 = (ftime)	*/
	do_sync,	/* 36 = sync	*/
	no_sys,		/* 37 = kill	*/
	do_rename,	/* 38 = rename	*/
	do_mkdir,	/* 39 = mkdir	*/
	do_unlink,	/* 40 = rmdir	*/
	no_sys,		/* 41 = unused  */
	do_pipe,	/* 42 = pipe	*/
	no_sys,		/* 43 = times	*/
	no_sys,		/* 44 = (prof)	*/
	do_slink,	/* 45 = symlink	*/
	no_sys,		/* 46 = (setgid)*/
	no_sys,		/* 47 = getgid	*/
	no_sys,		/* 48 = (signal)*/
	do_rdlink,	/* 49 = readlink*/
	no_sys,		/* 50 = unused (was old lstat)*/
	do_stat,	/* 51 = stat	*/
	do_fstat,	/* 52 = fstat	*/
	do_lstat,	/* 53 = lstat	*/
	do_ioctl,	/* 54 = ioctl	*/
	do_fcntl,	/* 55 = fcntl	*/
	do_copyfd,	/* 56 = copyfd	*/
	do_fsready,	/* 57 = FS proc ready */
	do_pipe2,	/* 58 = pipe2	*/
	no_sys,		/* 59 = (execve)*/
	do_umask,	/* 60 = umask	*/
	do_chroot,	/* 61 = chroot	*/
	no_sys,		/* 62 = (setsid)*/
	no_sys,		/* 63 = (getpgrp)*/
	no_sys,		/* 64 = (itimer)*/
	do_stat,	/* 65 = stat  - badly numbered, being phased out */
	do_fstat, 	/* 66 = fstat - badly numbered, being phased out */
	do_lstat,	/* 67 = lstat - badly numbered, being phased out */
	no_sys,		/* 68 = (setmcontext) */
	do_getdents,	/* 69 = getdents */
	do_ftruncate,	/* 70 = ftruncate  */
	no_sys,		/* 71 = (sigaction) */
	no_sys,		/* 72 = (sigsuspend) */
	no_sys,		/* 73 = (sigpending) */
	no_sys,		/* 74 = (sigprocmask) */
	no_sys,		/* 75 = (sigreturn) */
	no_sys,		/* 76 = (reboot) */
	do_svrctl,	/* 77 = svrctl */
	no_sys,		/* 78 = (sysuname) */
	no_sys,		/* 79 = unused */
	no_sys,		/* 80 = unused */
	do_lseek,	/* 81 = llseek */
	do_getvfsstat,	/* 82 = getvfsstat */
	do_statvfs,	/* 83 = fstatvfs */
	do_fstatvfs,	/* 84 = statvfs */
	do_select,	/* 85 = select */
	do_fchdir,	/* 86 = fchdir */
	do_fsync,	/* 87 = fsync */
	no_sys,		/* 88 = (getpriority) */
	no_sys,		/* 89 = (setpriority) */
	no_sys,		/* 90 = (gettimeofday) */
	no_sys,		/* 91 = (seteuid) */
	no_sys,		/* 92 = (setegid) */
	no_sys,		/* 93 = unused */
	no_sys,		/* 94 = unused */
	do_chmod,	/* 95 = fchmod */
	do_chown,	/* 96 = fchown */
	do_lseek,	/* 97 = lseek */
	no_sys,		/* 98 = (sprofile) */
	no_sys,		/* 99 = (cprofile) */
	no_sys,		/* 100 = (newexec) */
	no_sys,		/* 101 = (srv_fork) */
	no_sys,		/* 102 = (exec_restart) */
	no_sys,		/* 103 = unused */
	no_sys,		/* 104 = (getprocnr) */
	no_sys,		/* 105 = unused */
	no_sys,		/* 106 = unused */
	no_sys,		/* 107 = (getepinfo) */
	do_utimens,	/* 108 = utimens */
	do_fcntl,	/* 109 = fcntl */
	do_truncate,	/* 110 = unused */
	no_sys,		/* 111 = (srv_kill) */
	do_gcov_flush,	/* 112 = gcov_flush */
	no_sys,		/* 113 = (getsid) */
	no_sys,		/* 114 = (clock_getres) */
	no_sys,		/* 115 = (clock_gettime) */
	no_sys,		/* 116 = (clock_settime) */
	do_vm_call,	/* 117 = call from vm */
	no_sys,		/* 118 = unsused */
	no_sys,		/* 119 = unsused */
	no_sys,		/* 120 = unsused */
	no_sys,		/* 121 = (task reply) */
	do_mapdriver,	/* 122 = mapdriver */
	do_getrusage,	/* 123 = getrusage */
	do_checkperms,	/* 124 = checkperms */
};
/* This should not fail with "array size is negative": */
extern int dummy[sizeof(call_vec) == NCALLS * sizeof(call_vec[0]) ? 1 : -1];
