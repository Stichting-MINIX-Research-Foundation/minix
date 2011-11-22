/* <sys/ptrace.h>
 * definitions for ptrace(2) 
 */

#ifndef _PTRACE_H
#define _PTRACE_H

/* Trace requests. */
#define T_STOP	       -1	/* stop the process */
#define T_OK		0	/* enable tracing by parent for this process */
#define T_GETINS	1	/* return value from instruction space */
#define T_GETDATA	2	/* return value from data space */
#define T_GETUSER	3	/* return value from user process table */
#define	T_SETINS	4	/* set value from instruction space */
#define T_SETDATA	5	/* set value from data space */
#define T_SETUSER	6	/* set value in user process table */
#define T_RESUME	7	/* resume execution */
#define T_EXIT		8	/* exit */
#define T_STEP		9	/* set trace bit */
#define T_SYSCALL	10	/* trace system call */
#define T_ATTACH	11	/* attach to a running process */
#define T_DETACH	12	/* detach from a traced process */
#define T_SETOPT	13	/* set trace options */
#define T_GETRANGE	14	/* get range of values */
#define T_SETRANGE	15	/* set range of values */
#define T_DUMPCORE      16      /* dumps the core for the process with the given pid */

#define T_READB_INS	100	/* Read a byte from the text segment of an
				 * untraced process (only for root)
				 */
#define T_WRITEB_INS	101	/* Write a byte in the text segment of an
				 * untraced process (only for root)
				 */

/* Trace options. */
#define TO_TRACEFORK	0x1	/* automatically attach to forked children */
#define TO_ALTEXEC	0x2	/* send SIGSTOP on successful exec() */
#define TO_NOEXEC	0x4	/* do not send signal on successful exec() */

/* Trace spaces. */
#define TS_INS		0	/* text space */
#define TS_DATA		1	/* data space */

/* Trance range structure. */
struct ptrace_range {
  int pr_space;			/* space in traced process */
  long pr_addr;			/* address in traced process */
  void *pr_ptr;			/* buffer in caller process */
  size_t pr_size;		/* size of range, in bytes */
};

/* Function Prototypes. */
#include <sys/cdefs.h>

__BEGIN_DECLS
long ptrace(int _req, pid_t _pid, long _addr, long _data);
__END_DECLS

#endif /* _PTRACE_H */
