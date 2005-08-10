/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "pm.h"
#include <minix/callnr.h>
#include <signal.h>
#include "mproc.h"
#include "param.h"

/* Miscellaneous */
char core_name[] = "core";	/* file name where core images are produced */

_PROTOTYPE (int (*call_vec[NCALLS]), (void) ) = {
	no_sys,		/*  0 = unused	*/
	do_pm_exit,	/*  1 = exit	*/
	do_fork,	/*  2 = fork	*/
	no_sys,		/*  3 = read	*/
	no_sys,		/*  4 = write	*/
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
	do_getset,	/* 20 = getpid	*/
	no_sys,		/* 21 = mount	*/
	no_sys,		/* 22 = umount	*/
	do_getset,	/* 23 = setuid	*/
	do_getset,	/* 24 = getuid	*/
	do_stime,	/* 25 = stime	*/
	do_trace,	/* 26 = ptrace	*/
	do_alarm,	/* 27 = alarm	*/
	no_sys,		/* 28 = fstat	*/
	do_pause,	/* 29 = pause	*/
	no_sys,		/* 30 = utime	*/
	no_sys,		/* 31 = (stty)	*/
	no_sys,		/* 32 = (gtty)	*/
	no_sys,		/* 33 = access	*/
	no_sys,		/* 34 = (nice)	*/
	no_sys,		/* 35 = (ftime)	*/
	no_sys,		/* 36 = sync	*/
	do_kill,	/* 37 = kill	*/
	no_sys,		/* 38 = rename	*/
	no_sys,		/* 39 = mkdir	*/
	no_sys,		/* 40 = rmdir	*/
	no_sys,		/* 41 = dup	*/
	no_sys,		/* 42 = pipe	*/
	do_times,	/* 43 = times	*/
	no_sys,		/* 44 = (prof)	*/
	no_sys,		/* 45 = unused	*/
	do_getset,	/* 46 = setgid	*/
	do_getset,	/* 47 = getgid	*/
	no_sys,		/* 48 = (signal)*/
	no_sys,		/* 49 = unused	*/
	no_sys,		/* 50 = unused	*/
	no_sys,		/* 51 = (acct)	*/
	no_sys,		/* 52 = (phys)	*/
	no_sys,		/* 53 = (lock)	*/
	no_sys,		/* 54 = ioctl	*/
	no_sys,		/* 55 = fcntl	*/
	no_sys,		/* 56 = (mpx)	*/
	no_sys,		/* 57 = unused	*/
	no_sys,		/* 58 = unused	*/
	do_exec,	/* 59 = execve	*/
	no_sys,		/* 60 = umask	*/
	no_sys,		/* 61 = chroot	*/
	do_getset,	/* 62 = setsid	*/
	do_getset,	/* 63 = getpgrp	*/

	no_sys,		/* 64 = unused */
	no_sys,		/* 65 = UNPAUSE	*/
	no_sys, 	/* 66 = unused  */
	no_sys,		/* 67 = REVIVE	*/
	no_sys,		/* 68 = TASK_REPLY  */
	no_sys,		/* 69 = unused	*/
	no_sys,		/* 70 = unused	*/
	do_sigaction,	/* 71 = sigaction   */
	do_sigsuspend,	/* 72 = sigsuspend  */
	do_sigpending,	/* 73 = sigpending  */
	do_sigprocmask,	/* 74 = sigprocmask */
	do_sigreturn,	/* 75 = sigreturn   */
	do_reboot,	/* 76 = reboot	*/
	do_svrctl,	/* 77 = svrctl	*/

	no_sys,		/* 78 = unused */
	do_getsysinfo,	/* 79 = getsysinfo */
	do_getprocnr,	/* 80 = getprocnr */
	no_sys, 	/* 81 = unused */
	no_sys, 	/* 82 = fstatfs */
	do_allocmem, 	/* 83 = memalloc */
	do_freemem, 	/* 84 = memfree */
	no_sys,		/* 85 = select */
	no_sys,		/* 86 = fchdir */
	no_sys,		/* 87 = fsync */
	do_getsetpriority,	/* 88 = getpriority */
	do_getsetpriority,	/* 89 = setpriority */
	do_time,	/* 90 = gettimeofday */
};
/* This should not fail with "array size is negative": */
extern int dummy[sizeof(call_vec) == NCALLS * sizeof(call_vec[0]) ? 1 : -1];
