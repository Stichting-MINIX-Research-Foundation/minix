#ifndef _SYS_WAIT_H_
#define _SYS_WAIT_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/featuretest.h>

/* The <sys/wait.h> header contains macros related to wait(). The value
 * returned by wait() and waitpid() depends on whether the process 
 * terminated by an exit() call, was killed by a signal, or was stopped
 * due to job control, as follows:
 *
 *				 High byte   Low byte
 *				+---------------------+
 *	exit(status)		|  status  |    0     |
 *				+---------------------+
 *      killed by signal	|    0     |  signal  |
 *				+---------------------+
 *	stopped (job control)	|  signal  |   0177   |
 *				+---------------------+
 */

/*
 * Macros to test the exit status returned by wait
 * and extract the relevant values.
 */

#define _LOW(v)		( (v) & 0377)
#define _HIGH(v)	( ((v) >> 8) & 0377)

#define WIFEXITED(s)	(_LOW(s) == 0)			    /* normal exit */
#define WEXITSTATUS(s)	(_HIGH(s))			    /* exit status */
#define WTERMSIG(s)	(_LOW(s) & 0177)		    /* sig value */
#define WIFSIGNALED(s)	((((unsigned int)(s)-1) & 0xFFFFU) < 0xFFU) /* signaled */
#define WIFSTOPPED(s)	(_LOW(s) == 0177)		    /* stopped */
#define WSTOPSIG(s)	(_HIGH(s) & 0377)		    /* stop signal */

/*
 * Option bits for the third argument of waitpid.  WNOHANG causes the
 * wait to not hang if there are no stopped or terminated processes, rather
 * returning an error indication in this case (pid==0).  WUNTRACED
 * indicates that the caller should receive status about untraced children
 * which stop due to signals.  If children are stopped and a wait without
 * this option is done, it is as though they were still running... nothing
 * about them is returned.
 */
#define WNOHANG		0x00000001	/* don't hang in wait */
#define WUNTRACED	0x00000002	/* tell about stopped,
					   untraced children */

/* POSIX extensions and 4.2/4.3 compatibility: */

/*
 * Tokens for special values of the "pid" parameter to waitpid.
 */
#define	WAIT_ANY	(-1)	/* any process */
#define	WAIT_MYPGRP	0	/* any process in my process group */

__BEGIN_DECLS
pid_t	wait(int *);
pid_t	waitpid(pid_t, int *, int);
__END_DECLS

#endif /* !_SYS_WAIT_H_ */
