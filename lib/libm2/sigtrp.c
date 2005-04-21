/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	Mapping of Unix signals to EM traps
		(only when not using the MON instruction)
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*/

#if !defined(__em22) && !defined(__em24) && !defined(__em44)

#define EM_trap(n) TRP(n)	/* define to whatever is needed to cause the trap */

#include <signal.h>
#include <errno.h>

int __signo;

static int __traps[] = {
 -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
 -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
 -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
};

static void
__ctchsig(signo)
{
	signal(signo,__ctchsig);
#ifdef __BSD4_2
	sigsetmask(sigblock(0) & ~(1<<(signo - 1)));
#endif
	__signo = signo;
	EM_trap(__traps[signo]);
}

int
sigtrp(trapno, signo)
{
	/*	Let Unix signal signo cause EM trap trapno to occur.
		If trapno = -2, restore default,
		If trapno = -3, ignore.
		Return old trapnumber.
		Careful, this could be -2 or -3; But return value of -1
		indicates failure, with error number in errno.
	*/
	extern int errno;
	void (*ctch)() = __ctchsig;
	void (*oldctch)();
	int oldtrap;

	if (signo <= 0 || signo >= sizeof(__traps)/sizeof(__traps[0])) {
		errno = EINVAL;
		return -1;
	}

	if (trapno == -3)
		ctch = SIG_IGN;
	else if (trapno == -2)
		ctch = SIG_DFL;
	else if (trapno >= 0 && trapno <= 252)
		;
	else {
		errno = EINVAL;
		return -1;
	}

	oldtrap = __traps[signo];

	if ((oldctch = signal(signo, ctch)) == (void (*)())-1)  /* errno set by signal */
		return -1;
	
	else if (oldctch == SIG_IGN) {
		signal(signo, SIG_IGN);
	}
	else __traps[signo] = trapno;

	return oldtrap;
}
#endif
