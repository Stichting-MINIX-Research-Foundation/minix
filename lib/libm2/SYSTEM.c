/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	SYSTEM
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*/

/*
	An implementation of the Modula-2 NEWPROCESS and TRANSFER facilities
	using the topsize, topsave, and topload facilities.
	For each coroutine, a proc structure is built. For the main routine,
	a static space is declared to save its stack. For the other coroutines,
	the user specifies this space.
*/

#include <m2_traps.h>

#define MAXMAIN	2048

struct proc {
	unsigned size;		/* size of saved stackframe(s) */
	int (*proc)();		/* address of coroutine procedure */
	char *brk;		/* stack break of this coroutine */
};

extern unsigned topsize();

static struct proc mainproc[MAXMAIN/sizeof(struct proc) + 1];

static struct proc *curproc = 0;/* current coroutine */
extern char *MainLB;		/* stack break of main routine */

_SYSTEM__NEWPROCESS(p, a, n, p1)
	int (*p)();		/* coroutine procedure */
	struct proc *a;		/* pointer to area for saved stack-frame */
	unsigned n;		/* size of this area */
	struct proc **p1;	/* where to leave coroutine descriptor,
				   in this implementation the address of
				   the area for saved stack-frame(s) */
{
	/*	This procedure creates a new coroutine, but does not
		transfer control to it. The routine "topsize" will compute the
		stack break, which will be the local base of this routine.
		Notice that we can do this because we do not need the stack
		above this point for this coroutine. In Modula-2, coroutines
		must be level 0 procedures without parameters.
	*/
	char *brk = 0;
	unsigned sz = topsize(&brk);

	if (sz + sizeof(struct proc) > n) {
		/* not enough space */
		TRP(M2_TOOLARGE);
	}
	a->size = n;
	a->proc = p;
	a->brk = brk;
	*p1 = a;
	if (topsave(brk, a+1))
		/* stack frame saved; now just return */
		;
	else {
		/* We get here through the first transfer to the coroutine
		   created above.
		   This also means that curproc is now set to this coroutine.
		   We cannot trust the parameters anymore.
		   Just call the coroutine procedure.
		*/
		(*(curproc->proc))();
		_cleanup();
		_exit(0);
	}
}

_SYSTEM__TRANSFER(a, b)
	struct proc **a, **b;
{
	/*	transfer from one coroutine to another, saving the current
		descriptor in the space indicated by "a", and transfering to
		the coroutine in descriptor "b".
	*/
	unsigned size;

	if (! curproc) {
		/* the current coroutine is the main process;
		   initialize a coroutine descriptor for it ...
		*/
		mainproc[0].brk = MainLB;
		mainproc[0].size = sizeof(mainproc);
		curproc = &mainproc[0];
	}
	*a = curproc;		/* save current descriptor in "a" */
	if (*b == curproc) {
		/* transfer to itself is a no-op */
		return;
	}
	size = topsize(&(curproc->brk));
	if (size + sizeof(struct proc) > curproc->size) {
		TRP(M2_TOOLARGE);
	}
	if (topsave(curproc->brk, curproc+1)) {
		/* stack top saved. Now restore context of target
		   coroutine
		*/
		curproc = *b;
		topload(curproc+1);
		/* we never get here ... */
	}
	/* but we do get here, when a transfer is done to the coroutine in "a".
	*/
}
