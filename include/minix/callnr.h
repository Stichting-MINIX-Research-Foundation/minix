/* This header file defines the calls to PM and VFS. */
#ifndef _MINIX_CALLNR_H
#define _MINIX_CALLNR_H

/*===========================================================================*
 *				Calls to PM				     *
 *===========================================================================*/

#define PM_BASE			0x000

#define IS_PM_CALL(type)	(((type) & ~0xff) == PM_BASE)

/* Message type 0 is traditionally reserved. */
#define PM_EXIT			(PM_BASE + 1)
#define PM_FORK			(PM_BASE + 2)
#define PM_WAITPID		(PM_BASE + 3)
#define PM_GETPID		(PM_BASE + 4)
#define PM_SETUID		(PM_BASE + 5)
#define PM_GETUID		(PM_BASE + 6)
#define PM_STIME		(PM_BASE + 7)
#define PM_PTRACE		(PM_BASE + 8)
#define PM_SETGROUPS		(PM_BASE + 9)
#define PM_GETGROUPS		(PM_BASE + 10)
#define PM_KILL			(PM_BASE + 11)
#define PM_SETGID		(PM_BASE + 12)
#define PM_GETGID		(PM_BASE + 13)
#define PM_EXEC			(PM_BASE + 14)
#define PM_SETSID		(PM_BASE + 15)
#define PM_GETPGRP		(PM_BASE + 16)
#define PM_ITIMER		(PM_BASE + 17)
#define PM_GETMCONTEXT		(PM_BASE + 18)
#define PM_SETMCONTEXT		(PM_BASE + 19)
#define PM_SIGACTION		(PM_BASE + 20)
#define PM_SIGSUSPEND		(PM_BASE + 21)
#define PM_SIGPENDING		(PM_BASE + 22)
#define PM_SIGPROCMASK		(PM_BASE + 23)
#define PM_SIGRETURN		(PM_BASE + 24)
#define PM_SYSUNAME		(PM_BASE + 25)
#define PM_GETPRIORITY		(PM_BASE + 26)
#define PM_SETPRIORITY		(PM_BASE + 27)
#define PM_GETTIMEOFDAY		(PM_BASE + 28)
#define PM_SETEUID		(PM_BASE + 29)
#define PM_SETEGID		(PM_BASE + 30)
#define PM_ISSETUGID		(PM_BASE + 31)
#define PM_GETSID		(PM_BASE + 32)
#define PM_CLOCK_GETRES		(PM_BASE + 33)
#define PM_CLOCK_GETTIME	(PM_BASE + 34)
#define PM_CLOCK_SETTIME	(PM_BASE + 35)
#define PM_GETRUSAGE		(PM_BASE + 36)
#define PM_REBOOT		(PM_BASE + 37)
#define PM_SVRCTL		(PM_BASE + 38)
#define PM_SPROF		(PM_BASE + 39)
#define PM_CPROF		(PM_BASE + 40)
#define PM_SRV_FORK		(PM_BASE + 41)
#define PM_SRV_KILL		(PM_BASE + 42)
#define PM_EXEC_NEW		(PM_BASE + 43)
#define PM_EXEC_RESTART		(PM_BASE + 44)
#define PM_GETEPINFO		(PM_BASE + 45)
#define PM_GETPROCNR		(PM_BASE + 46)
#define PM_GETSYSINFO		(PM_BASE + 47)

#define NR_PM_CALLS		48	/* highest number from base plus one */

/* Field names for the getprocnr(2) call. */
#define PM_GETPROCNR_PID	m1_i1	/* pid_t */
#define PM_GETPROCNR_ENDPT	m1_i1	/* endpoint_t */

/* Field names for the getepinfo(2) call. */
#define PM_GETEPINFO_ENDPT	m1_i1	/* endpoint_t */
#define PM_GETEPINFO_UID	m1_i1	/* uid_t */
#define PM_GETEPINFO_GID	m1_i2	/* gid_t */

/* Field names for the exit(2) call. */
#define PM_EXIT_STATUS		m1_i1	/* int */

/* Field names for the waitpid(2) call. */
#define PM_WAITPID_PID		m1_i1	/* pid_t */
#define PM_WAITPID_OPTIONS	m1_i2	/* int */
#define PM_WAITPID_STATUS	m2_i1	/* int */

/* Field names for the gettimeofday(2), clock_*(2), adjtime(2), stime(2) calls.
 */
#define PM_TIME_CLK_ID		m2_i1	/* clockid_t */
#define PM_TIME_NOW		m2_i2	/* int */
#define PM_TIME_SEC		m2_ll1	/* time_t */
#define PM_TIME_USEC		m2_l2	/* long */
#define PM_TIME_NSEC		m2_l2	/* long */

/* Field names for the ptrace(2) call. */
#define PM_PTRACE_PID		m2_i1	/* pid_t */
#define PM_PTRACE_REQ		m2_i2	/* int */
#define PM_PTRACE_ADDR		m2_l1	/* long */
#define PM_PTRACE_DATA		m2_l2	/* long */

/* Field names for the sysuname(2) call. */
#define PM_SYSUNAME_REQ		m1_i1	/* int */
#define PM_SYSUNAME_FIELD	m1_i2	/* int */
#define PM_SYSUNAME_LEN		m1_i3	/* char * */
#define PM_SYSUNAME_VALUE	m1_p1	/* size_t */

/* Field names for the getitimer(2)/setitimer(2) calls. */
#define PM_ITIMER_WHICH		m1_i1	/* int */
#define PM_ITIMER_VALUE		m1_p1	/* const struct itimerval * */
#define PM_ITIMER_OVALUE	m1_p2	/* struct itimerval * */

/* Field names for the execve(2) call. */
#define PM_EXEC_NAME		m1_p1	/* const char * */
#define PM_EXEC_NAMELEN		m1_i1	/* size_t */
#define PM_EXEC_FRAME		m1_p2	/* char * */
#define PM_EXEC_FRAMELEN	m1_i2	/* size_t */
#define PM_EXEC_PS_STR		m1_p3	/* char * */

/* Field names for the kill(2), srv_kill(2), and sigaction(2) calls. */
#define PM_SIG_PID		m1_i1	/* pid_t */
#define PM_SIG_NR		m1_i2	/* int */
#define PM_SIG_ACT		m1_p1	/* const struct sigaction * */
#define PM_SIG_OACT		m1_p2	/* struct sigaction * */
#define PM_SIG_RET		m1_p3	/* int (*)(void) */

/* Field names for the remaining sigpending(2), sigprocmask(2), sigreturn(2),
 * sigsuspend(2) calls.
 */
#define PM_SIG_HOW		m2_i1	/* int */
#define PM_SIG_SET		m2_sigset /* sigset_t */
#define PM_SIG_CTX		m2_p1	/* struct sigcontext * */

/* Field names for the srv_fork(2) call. */
#define PM_SRV_FORK_UID		m1_i1	/* uid_t */
#define PM_SRV_FORK_GID		m1_i2	/* gid_t */

/* Field names for the getuid(2) call. */
#define PM_GETUID_EUID		m1_i1	/* uid_t */

/* Field names for the getgid(2) call. */
#define PM_GETGID_EGID		m1_i1	/* gid_t */

/* Field names for the setuid(2)/seteuid(2) calls. */
#define PM_SETUID_UID		m1_i1	/* uid_t */

/* Field names for the setgid(2)/setegid(2) calls. */
#define PM_SETGID_GID		m1_i1	/* gid_t */

/* Field names for the getppid(2) call. */
#define PM_GETPID_PARENT	m2_i1	/* pid_t */

/* Field names for the setsid(2) call. */
#define PM_GETSID_PID		m1_i1	/* pid_t */

/* Field names for the setgroups(2)/setgroups(2) calls. */
#define PM_GROUPS_NUM		m1_i1	/* int */
#define PM_GROUPS_PTR		m1_p1	/* gid_t * */

/* Field names for the getpriority(2)/setpriority(2) calls. */
#define PM_PRIORITY_WHICH	m1_i1	/* int */
#define PM_PRIORITY_WHO		m1_i2	/* int */
#define PM_PRIORITY_PRIO	m1_i3	/* int */

/* Field names for the getmcontext(2)/setmcontext(2) calls. */
#define PM_MCONTEXT_CTX		m1_p1	/* mcontext_t * */

/* Field names for the reboot(2) call. */
#define PM_REBOOT_HOW		m1_i1	/* int */

/* Field names for the PM_EXEC_NEW call. */
#define PM_EXEC_NEW_ENDPT	m1_i1	/* endpoint_t */
#define PM_EXEC_NEW_PTR		m1_p1	/* struct exec_info * */
#define PM_EXEC_NEW_SUID	m1_i2	/* int */

/* Field names for the PM_EXEC_RESTART call. */
#define PM_EXEC_RESTART_ENDPT	m1_i1	/* endpoint_t */
#define PM_EXEC_RESTART_RESULT	m1_i2	/* int */
#define PM_EXEC_RESTART_PC	m1_p1	/* vir_bytes */
#define PM_EXEC_RESTART_PS_STR	m1_p2	/* vir_bytes */

/*===========================================================================*
 *				Calls to VFS				     *
 *===========================================================================*/

#define VFS_BASE		0x100

#define IS_VFS_CALL(type)	(((type) & ~0xff) == VFS_BASE)

#define VFS_READ		(VFS_BASE + 0)
#define VFS_WRITE		(VFS_BASE + 1)
#define VFS_LSEEK		(VFS_BASE + 2)
#define VFS_OPEN		(VFS_BASE + 3)
#define VFS_CREAT		(VFS_BASE + 4)
#define VFS_CLOSE		(VFS_BASE + 5)
#define VFS_LINK		(VFS_BASE + 6)
#define VFS_UNLINK		(VFS_BASE + 7)
#define VFS_CHDIR		(VFS_BASE + 8)
#define VFS_MKDIR		(VFS_BASE + 9)
#define VFS_MKNOD		(VFS_BASE + 10)
#define VFS_CHMOD		(VFS_BASE + 11)
#define VFS_CHOWN		(VFS_BASE + 12)
#define VFS_MOUNT		(VFS_BASE + 13)
#define VFS_UMOUNT		(VFS_BASE + 14)
#define VFS_ACCESS		(VFS_BASE + 15)
#define VFS_SYNC		(VFS_BASE + 16)
#define VFS_RENAME		(VFS_BASE + 17)
#define VFS_RMDIR		(VFS_BASE + 18)
#define VFS_SYMLINK		(VFS_BASE + 19)
#define VFS_READLINK		(VFS_BASE + 20)
#define VFS_STAT		(VFS_BASE + 21)
#define VFS_FSTAT		(VFS_BASE + 22)
#define VFS_LSTAT		(VFS_BASE + 23)
#define VFS_IOCTL		(VFS_BASE + 24)
#define VFS_FCNTL		(VFS_BASE + 25)
#define VFS_PIPE2		(VFS_BASE + 26)
#define VFS_UMASK		(VFS_BASE + 27)
#define VFS_CHROOT		(VFS_BASE + 28)
#define VFS_GETDENTS		(VFS_BASE + 29)
#define VFS_SELECT		(VFS_BASE + 30)
#define VFS_FCHDIR		(VFS_BASE + 31)
#define VFS_FSYNC		(VFS_BASE + 32)
#define VFS_TRUNCATE		(VFS_BASE + 33)
#define VFS_FTRUNCATE		(VFS_BASE + 34)
#define VFS_FCHMOD		(VFS_BASE + 35)
#define VFS_FCHOWN		(VFS_BASE + 36)
#define VFS_UTIMENS		(VFS_BASE + 37)
#define VFS_VMCALL		(VFS_BASE + 38)
#define VFS_GETVFSSTAT		(VFS_BASE + 39)
#define VFS_STATVFS1 	 	(VFS_BASE + 40)
#define VFS_FSTATVFS1		(VFS_BASE + 41)
#define VFS_GETRUSAGE		(VFS_BASE + 42)
#define VFS_SVRCTL		(VFS_BASE + 43)
#define VFS_GCOV_FLUSH		(VFS_BASE + 44)
#define VFS_MAPDRIVER		(VFS_BASE + 45)
#define VFS_COPYFD		(VFS_BASE + 46)
#define VFS_CHECKPERMS		(VFS_BASE + 47)
#define VFS_GETSYSINFO		(VFS_BASE + 48)

#define NR_VFS_CALLS		49	/* highest number from base plus one */

/* Field names for the ioctl(2) call. */
#define VFS_IOCTL_FD		m2_i1	/* int */
#define VFS_IOCTL_REQ		m2_i3	/* unsigned long */
#define VFS_IOCTL_ARG		m2_p1	/* void * */

/* Field names for the checkperms(2) call. */
#define VFS_CHECKPERMS_ENDPT	m2_i1	/* endpoint_t */
#define VFS_CHECKPERMS_GRANT	m2_i2	/* cp_grant_id_t */
#define VFS_CHECKPERMS_COUNT	m2_i3	/* size_t */

/* Field names for the copyfd(2) call. */
#define VFS_COPYFD_ENDPT	m1_i1	/* endpoint_t */
#define VFS_COPYFD_FD		m1_i2	/* int */
#define VFS_COPYFD_WHAT		m1_i3	/* int */

/* Field names for the mapdriver(2) call. */
#define VFS_MAPDRIVER_MAJOR	m1_i1	/* devmajor_t */
#define VFS_MAPDRIVER_LABELLEN	m1_i2	/* size_t */
#define VFS_MAPDRIVER_LABEL	m1_p1	/* char * */

/* Field names for the fsync(2) call. */
#define VFS_FSYNC_FD		m1_i1	/* int */

/* Field names for the lseek(2) call. */
#define VFS_LSEEK_FD		m2_i1	/* int */
#define VFS_LSEEK_OFF		m2_ll1	/* off_t */
#define VFS_LSEEK_WHENCE	m2_i2	/* int */

/* Field names for the truncate(2) and ftruncate(2) calls. */
#define VFS_TRUNCATE_FD		m2_i1	/* int */
#define VFS_TRUNCATE_NAME	m2_p1	/* const char * */
#define VFS_TRUNCATE_LEN	m2_i1	/* size_t */
#define VFS_TRUNCATE_OFF	m2_ll1	/* off_t */

/* Field names for the pipe2(2) call. */
#define VFS_PIPE2_FD0		m1_i1	/* int */
#define VFS_PIPE2_FD1		m1_i2	/* int */
#define VFS_PIPE2_FLAGS		m1_i3	/* int */

/* Field names for the umask(2) call. */
#define VFS_UMASK_MASK		m1_i1	/* mode_t */

/* Field names for the link(2), symlink(2), and rename(2) call. */
#define VFS_LINK_NAME1		m1_p1	/* const char * */
#define VFS_LINK_LEN1		m1_i1	/* size_t */
#define VFS_LINK_NAME2		m1_p2	/* const char * */
#define VFS_LINK_LEN2		m1_i2	/* size_t */

/* Field names for the readlink(2) call. */
#define VFS_READLINK_NAME	m1_p1	/* const char * */
#define VFS_READLINK_NAMELEN	m1_i1	/* size_t */
#define VFS_READLINK_BUF	m1_p2	/* char * */
#define VFS_READLINK_BUFSIZE	m1_i2	/* size_t */

/* Field names for the stat(2) and lstat(2) calls. */
#define VFS_STAT_NAME		m1_p1	/* const char * */
#define VFS_STAT_LEN		m1_i1	/* size_t */
#define VFS_STAT_BUF		m1_p2	/* struct stat * */

/* Field names for the fstat(2) call. */
#define VFS_FSTAT_FD		m1_i1	/* int */
#define VFS_FSTAT_BUF		m1_p1	/* struct stat * */

/* Field names for the fcntl(2) call. */
#define VFS_FCNTL_FD		m1_i1	/* int */
#define VFS_FCNTL_CMD		m1_i2	/* int */
#define VFS_FCNTL_ARG_INT	m1_i3	/* int */
#define VFS_FCNTL_ARG_PTR	m1_p1	/* struct flock * */

/* Field names for the mknod(2) call. */
#define VFS_MKNOD_NAME		m1_p1	/* const char * */
#define VFS_MKNOD_LEN		m1_i1	/* size_t */
#define VFS_MKNOD_MODE		m1_i2	/* mode_t */
#define VFS_MKNOD_DEV		m1_ull1	/* dev_t */

/* Field names for the open(2), chdir(2), chmod(2), chroot(2), rmdir(2), and
 * unlink(2) calls.
 */
#define VFS_PATH_NAME		m3_p1	/* const char * */
#define VFS_PATH_LEN		m3_i1	/* size_t */
#define VFS_PATH_FLAGS		m3_i2	/* int */
#define VFS_PATH_MODE		m3_i2	/* mode_t */
#define VFS_PATH_BUF		m3_ca1	/* char[M3_STRING] */

/* Field names for the creat(2) call. */
#define VFS_CREAT_NAME		m1_p1	/* const char * */
#define VFS_CREAT_LEN		m1_i1	/* size_t */
#define VFS_CREAT_FLAGS		m1_i2	/* int */
#define VFS_CREAT_MODE		m1_i3	/* mode_t */

/* Field names for the chown(2) and fchown(2) calls. */
#define VFS_CHOWN_NAME		m1_p1	/* const char * */
#define VFS_CHOWN_LEN		m1_i1	/* size_t */
#define VFS_CHOWN_FD		m1_i1	/* int */
#define VFS_CHOWN_OWNER		m1_i2	/* uid_t */
#define VFS_CHOWN_GROUP		m1_i3	/* gid_t */

/* Field names for the fchdir(2) call. */
#define VFS_FCHDIR_FD		m1_i1	/* int */

/* Field names for the fchmod(2) call. */
#define VFS_FCHMOD_FD		m1_i1	/* int */
#define VFS_FCHMOD_MODE		m1_i2	/* mode_t */

/* Field names for the close(2) call. */
#define VFS_CLOSE_FD		m1_i1	/* int */

/* Field names for the read(2), write(2), and getdents(2) calls. */
#define VFS_READWRITE_FD	m1_i1	/* int */
#define VFS_READWRITE_BUF	m1_p1	/* char * */
#define VFS_READWRITE_LEN	m1_i2	/* size_t */

#endif /* !_MINIX_CALLNR_H */
