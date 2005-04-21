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

#ifndef _WAIT_H
#define _WAIT_H

#ifndef _TYPES_H
#include <sys/types.h>
#endif

#define _LOW(v)		( (v) & 0377)
#define _HIGH(v)	( ((v) >> 8) & 0377)

#define WNOHANG         1	/* do not wait for child to exit */
#define WUNTRACED       2	/* for job control; not implemented */

#define WIFEXITED(s)	(_LOW(s) == 0)			    /* normal exit */
#define WEXITSTATUS(s)	(_HIGH(s))			    /* exit status */
#define WTERMSIG(s)	(_LOW(s) & 0177)		    /* sig value */
#define WIFSIGNALED(s)	(((unsigned int)(s)-1 & 0xFFFF) < 0xFF) /* signaled */
#define WIFSTOPPED(s)	(_LOW(s) == 0177)		    /* stopped */
#define WSTOPSIG(s)	(_HIGH(s) & 0377)		    /* stop signal */

/* Function Prototypes. */
_PROTOTYPE( pid_t wait, (int *_stat_loc)			   	   );
_PROTOTYPE( pid_t waitpid, (pid_t _pid, int *_stat_loc, int _options)	   );

#endif /* _WAIT_H */
