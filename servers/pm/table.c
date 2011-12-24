/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "pm.h"
#include <minix/callnr.h>
#include <signal.h>
#include "mproc.h"
#include "param.h"

int (*call_vec[])(void) = {
	no_sys,		/*  0 = unused	*/
	do_exit,	/*  1 = exit	*/
	do_fork,	/*  2 = fork	*/
	no_sys,		/*  3 = read	*/
	no_sys, 	/*  4 = write	*/
	no_sys,		/*  5 = open	*/
	no_sys,		/*  6 = close	*/
	do_waitpid,	/*  7 = wait	*/
	no_sys,		/*  8 = creat	*/
	no_sys,		/*  9 = link	*/
	no_sys,		/* 10 = unlink	*/
	do_waitpid,	/* 11 = waitpid	*/
	no_sys,		/* 12 = chdir	*/
	do_time,	/* 13 = time	*/
	no_sys,		/* 14 = mknod	*/
	no_sys,		/* 15 = chmod	*/
	no_sys,		/* 16 = chown	*/
	do_brk,		/* 17 = break	*/
	no_sys,		/* 18 = stat	*/
	no_sys,		/* 19 = lseek	*/
	do_get,		/* 20 = getpid	*/
	no_sys,		/* 21 = mount	*/
	no_sys,		/* 22 = umount	*/
	do_set,		/* 23 = setuid	*/
	do_get,		/* 24 = getuid	*/
	do_stime,	/* 25 = stime	*/
	do_trace,	/* 26 = ptrace	*/
	do_alarm,	/* 27 = alarm	*/
	no_sys,		/* 28 = fstat	*/
	do_pause,	/* 29 = pause	*/
	no_sys,		/* 30 = utime	*/
	do_getepinfo,	/* 31 = getepinfo */
	do_set, 	/* 32 = setgroups */
	no_sys,		/* 33 = access	*/
	do_get,		/* 34 = getgroups */
	no_sys,		/* 35 = unused	*/
	no_sys,		/* 36 = sync	*/
	do_kill,	/* 37 = kill	*/
	no_sys,		/* 38 = rename	*/
	no_sys,		/* 39 = mkdir	*/
	no_sys,		/* 40 = rmdir	*/
	no_sys,		/* 41 = dup	*/
	no_sys,		/* 42 = pipe	*/
	do_times,	/* 43 = times	*/
	no_sys,		/* 44 = unused	*/
	no_sys,		/* 45 = unused	*/
	do_set,		/* 46 = setgid	*/
	do_get,		/* 47 = getgid	*/
	no_sys,		/* 48 = (signal)*/
	no_sys,		/* 49 = unused	*/
	no_sys,		/* 50 = lstat	*/
	no_sys,		/* 51 = (stat)	*/
	no_sys,		/* 52 = (fstat)	*/
	no_sys,		/* 53 = (lstat)	*/
	no_sys,		/* 54 = ioctl	*/
	no_sys,		/* 55 = fcntl	*/
	no_sys,		/* 56 = unused	*/
	no_sys,		/* 57 = unused	*/
	no_sys,		/* 58 = unused	*/
	do_exec,	/* 59 = execve	*/
	no_sys,		/* 60 = umask	*/
	no_sys,		/* 61 = chroot	*/
	do_set,		/* 62 = setsid	*/
	do_get,		/* 63 = getpgrp	*/
	do_itimer,	/* 64 = itimer  */
	do_get,		/* 65 = getgroups */
	do_set, 	/* 66 = setgroups */
	do_getmcontext,	/* 67 = getmcontext */
	do_setmcontext,	/* 68 = setmcontext */
	no_sys,		/* 69 = unused	*/
	no_sys,		/* 70 = unused	*/
	do_sigaction,	/* 71 = sigaction   */
	do_sigsuspend,	/* 72 = sigsuspend  */
	do_sigpending,	/* 73 = sigpending  */
	do_sigprocmask,	/* 74 = sigprocmask */
	do_sigreturn,	/* 75 = sigreturn   */
	do_reboot,	/* 76 = reboot	*/
	do_svrctl,	/* 77 = svrctl	*/
	do_sysuname,	/* 78 = sysuname */
	no_sys,		/* 79 = unused */
	no_sys,		/* 80 = (getdents) */
	no_sys, 	/* 81 = unused */
	no_sys, 	/* 82 = (fstatfs) */
	no_sys, 	/* 83 = unused */
	no_sys, 	/* 84 = unused */
	no_sys,		/* 85 = (select) */
	no_sys,		/* 86 = (fchdir) */
	no_sys,		/* 87 = (fsync) */
	do_getsetpriority, /* 88 = getpriority */
	do_getsetpriority, /* 89 = setpriority */
	do_time,	/* 90 = gettimeofday */
	do_set,		/* 91 = seteuid	*/
	do_set,		/* 92 = setegid	*/
	no_sys,		/* 93 = (truncate) */
	no_sys,		/* 94 = (ftruncate) */
	no_sys,		/* 95 = (fchmod) */
	no_sys,		/* 96 = (fchown) */
	no_sys,		/* 97 = unused */
	do_sprofile,	/* 98 = sprofile */
	do_cprofile,	/* 99 = cprofile */
	do_newexec,	/* 100 = newexec */
	do_srv_fork,	/* 101 = srv_fork */
	do_execrestart,	/* 102 = exec_restart */
	no_sys,		/* 103 = unused */
	do_getprocnr,	/* 104 = getprocnr */
	no_sys,		/* 105 = unused */
	do_get,		/* 106 = issetugid */
	do_getepinfo_o,	/* 107 = getepinfo XXX: old implementation*/
	no_sys,		/* 108 = (utimens) */
	no_sys,		/* 109 = unused */
	no_sys,		/* 110 = unused */
	do_srv_kill,	/* 111 = srv_kill */
 	no_sys, 	/* 112 = gcov_flush */
	do_get,		/* 113 = getsid	*/
	do_getres,	/* 114 = clock_getres */
	do_gettime,	/* 115 = clock_gettime */
	do_settime,	/* 116 = clock_settime */
};
/* This should not fail with "array size is negative": */
extern int dummy[sizeof(call_vec) == NCALLS * sizeof(call_vec[0]) ? 1 : -1];
