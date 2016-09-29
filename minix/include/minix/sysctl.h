#ifndef _MINIX_SYSCTL_H
#define _MINIX_SYSCTL_H

/* MINIX3-specific sysctl(2) extensions. */

#include <sys/sysctl.h>
#include <minix/endpoint.h>

/* Special values. */
#define SYSCTL_NODE_FN	((sysctlfn)0x1)		/* node is function-driven */

/*
 * The top-level MINIX3 identifier is quite a bit beyond the last top-level
 * identifier in use by NetBSD, because NetBSD may add more later, and we do
 * not want conflicts: this definition is part of the MINIX3 ABI.
 */
#define CTL_MINIX	32

#if CTL_MAXID > CTL_MINIX
#error "CTL_MAXID has grown too large!"
#endif

/*
 * The identifiers below follow the standard sysctl naming scheme, which means
 * care should be taken not to introduce clashes with other definitions
 * elsewhere.  On the upside, not many places need to include this header file.
 */
#define MINIX_TEST	0
#define MINIX_MIB	1
#define MINIX_PROC	2
#define MINIX_LWIP	3

/*
 * These identifiers, under MINIX_TEST, are used by test87 to test the MIB
 * service.
 */
#define TEST_INT	0
#define TEST_BOOL	1
#define TEST_QUAD	2
#define TEST_STRING	3
#define TEST_STRUCT	4
#define TEST_PRIVATE	5
#define TEST_ANYWRITE	6
#define TEST_DYNAMIC	7
#define TEST_SECRET	8
#define TEST_PERM	9
#define TEST_DESTROY1	10
#define TEST_DESTROY2	11

#define SECRET_VALUE	0

/* Identifiers for subnodes of MINIX_MIB. */
#define MIB_NODES	1
#define MIB_OBJECTS	2
#define MIB_REMOTES	3

/* Identifiers for subnodes of MINIX_PROC. */
#define PROC_LIST	1
#define PROC_DATA	2

/* Structure used for PROC_LIST.  Not part of the ABI.  Used by ProcFS only. */
struct minix_proc_list {
	uint32_t mpl_flags;		/* process flags (MPLF_) */
	pid_t mpl_pid;			/* process PID */
	uid_t mpl_uid;			/* effective user ID */
	gid_t mpl_gid;			/* effective group ID */
};
#define MPLF_IN_USE	0x01		/* process slot is in use */
#define MPLF_ZOMBIE	0x02		/* process is a zombie */

/* Structure used for PROC_DATA.  Not part of the ABI.  Used by ProcFS only. */
struct minix_proc_data {
	endpoint_t mpd_endpoint;	/* process endpoint */
	uint32_t mpd_flags;		/* procses flags (MPDF_) */
	endpoint_t mpd_blocked_on;	/* blocked on other process, or NONE */
	uint32_t mpd_priority;		/* current process priority */
	uint32_t mpd_user_time;		/* user time, in clock ticks */
	uint32_t mpd_sys_time;		/* system time, in clock ticks */
	uint64_t mpd_cycles;		/* cycles spent by the process */
	uint64_t mpd_kipc_cycles;	/* cycles spent on kernel IPC */
	uint64_t mpd_kcall_cycles;	/* cycles spent on kernel calls */
	uint32_t mpd_nice;		/* nice value */
	char mpd_name[16];		/* short process name */
};
#define MPDF_SYSTEM	0x01		/* process is a system service */
#define MPDF_ZOMBIE	0x02		/* process is a zombie */
#define MPDF_RUNNABLE	0x04		/* process is runnable */
#define MPDF_STOPPED	0x08		/* process is stopped */

/*
 * Expose sysctl(2) to system services (ProcFS in particular), so as to avoid
 * including the CTL_USER subtree handling of sysctl(3) as well.
 */
int __sysctl(const int *, unsigned int, void *, size_t *, const void *,
	size_t);

#endif /* !_MINIX_SYSCTL_H */
