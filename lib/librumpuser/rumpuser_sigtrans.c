/*	$NetBSD: rumpuser_sigtrans.c,v 1.1 2014/02/20 00:42:27 pooka Exp $	*/

/*
 * pseudo-automatically generated.  PLEASE DO EDIT (e.g. in case there
 * are errnos which are defined to be the same value)
 *
 * The body of the switch statement was generated using:
 *
 * awk '/^#define/ && $2 ~ "^SIG[A-Z]" \
 *   {printf "#ifdef %s\n\tcase %d\t: return %s;\n#endif\n", $2, $3, $2}' \
 *   signal.h
 */

#include <signal.h>

/*
 * Translate rump kernel signal number to host signal number
 */
int rumpuser__sig_rump2host(int); /* a naughty decouple */
int
rumpuser__sig_rump2host(int rumpsig)
{

	switch(rumpsig) {
	case 0  : return 0;
#ifdef SIGHUP
	case 1	: return SIGHUP;
#endif
#ifdef SIGINT
	case 2	: return SIGINT;
#endif
#ifdef SIGQUIT
	case 3	: return SIGQUIT;
#endif
#ifdef SIGILL
	case 4	: return SIGILL;
#endif
#ifdef SIGTRAP
	case 5	: return SIGTRAP;
#endif
#ifdef SIGABRT
	case 6	: return SIGABRT;
#endif
#ifdef SIGEMT
	case 7	: return SIGEMT;
#endif
#ifdef SIGFPE
	case 8	: return SIGFPE;
#endif
#ifdef SIGKILL
	case 9	: return SIGKILL;
#endif
#ifdef SIGBUS
	case 10	: return SIGBUS;
#endif
#ifdef SIGSEGV
	case 11	: return SIGSEGV;
#endif
#ifdef SIGSYS
	case 12	: return SIGSYS;
#endif
#ifdef SIGPIPE
	case 13	: return SIGPIPE;
#endif
#ifdef SIGALRM
	case 14	: return SIGALRM;
#endif
#ifdef SIGTERM
	case 15	: return SIGTERM;
#endif
#ifdef SIGURG
	case 16	: return SIGURG;
#endif
#ifdef SIGSTOP
	case 17	: return SIGSTOP;
#endif
#ifdef SIGTSTP
	case 18	: return SIGTSTP;
#endif
#ifdef SIGCONT
	case 19	: return SIGCONT;
#endif
#ifdef SIGCHLD
	case 20	: return SIGCHLD;
#elif defined(SIGCLD)
	case 20	: return SIGCLD;
#endif
#ifdef SIGTTIN
	case 21	: return SIGTTIN;
#endif
#ifdef SIGTTOU
	case 22	: return SIGTTOU;
#endif
#ifdef SIGIO
	case 23	: return SIGIO;
#endif
#ifdef SIGXCPU
	case 24	: return SIGXCPU;
#endif
#ifdef SIGXFSZ
	case 25	: return SIGXFSZ;
#endif
#ifdef SIGVTALRM
	case 26	: return SIGVTALRM;
#endif
#ifdef SIGPROF
	case 27	: return SIGPROF;
#endif
#ifdef SIGWINCH
	case 28	: return SIGWINCH;
#endif
#ifdef SIGINFO
	case 29	: return SIGINFO;
#endif
#ifdef SIGUSR1
	case 30	: return SIGUSR1;
#endif
#ifdef SIGUSR2
	case 31	: return SIGUSR2;
#endif
#ifdef SIGPWR
	case 32	: return SIGPWR;
#endif
	default:	return -1;
	}
}
