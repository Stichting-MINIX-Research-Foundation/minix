/* ELLE - Copyright 1982, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*	EEERR - Error handling & testing routines
 */

#include "elle.h"

#if V6
#include "eesigs.h"
#else
#include <signal.h>
#endif

/* EFUN: "Hit Breakpoint" */
f_bkpt()
{	clean_exit();
	bpt();
        set_tty();
}
bpt() {}		/* Put a DDT/ADB breakpoint here */

#if !(STRERROR)		/* If strerror() not supported, we provide it. */
extern int sys_nerr;		/* Max index into sys_errlist */
extern char *sys_errlist[];

char *
strerror(num)
int num;
{
	static char badbuf[30];
	if (num > 0 && num <= sys_nerr)
		return (sys_errlist[num]);
	sprintf(badbuf, "unknown error %d", num);
	return badbuf;
}
#endif /* -STRERROR */


errsbm(type,adr,str,a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12)
register int type;	/* Type, flags */
int (*adr)();		/* Addr called from */
char *str;		/* Printf string */
{	register struct buffer *b;
	int oldttystate;

	oldttystate = clean_exit();	/* Ensure not in editing mode */
	if(type == SBFERR)	/* File overwrite error?  A0 is FD */
	  {	printf("WARNING - FILE CORRUPTED!\nBuffers affected:\n");
		for(b = buf_head; b; b = b->b_next)
		  {	if(sb_fdinp((SBBUF *)b, a0))
				printf((b->b_fn ? "  %s: %s\n" : "  %s\n"),
					b->b_name, b->b_fn);
		  }
		if (oldttystate > 0) set_tty();
		return(1);	/* Try to continue normally */
	  }
	printf("%sERR: %o ", (type ? "SBX" : "SBM"), adr);
	printf(str,a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12);
	askerr();
}

/*
 *  Bite_bag -- Try to save our rear ends after a catastrophe.
 *	This routine is mainly called from "interrupt"
 *	level when a memory fault or bus error occurs.
 *	We try to save the buffer to the file "ELLE.crash"
 *	in the current working directory.  If it loses, well
 *	then you have really lost.  Note: this routine does
 *	not reset the appropriate signal handler, so it is
 *	never re-entered.  If a fault repeats once in this
 *	code, then the world dies.
 */

bite_bag(fault)				/* We come here on any memory error */
int fault;
{
	int ostate;
	/* Some systems, such as BSD4.x and SUN, do not reset caught signals
	 * to SIG_DFL.
	 * This is a win, but isn't what vanilla UNIX code expects.
	 * Since it doesn't hurt to do it explicitly, we always turn it off
	 * explicitly...
	 */
	signal(fault, SIG_DFL);		/* Reinstate default handling */

	ostate = clean_exit();		/* Fix up the terminal modes first! */
	printf("ELLE stopped by fatal interrupt (%d)!\n\
Type S or W to try saving your work.\n",fault);
	askerr();
	if(ostate > 0) set_tty();
	signal(fault, bite_bag);	/* If continued, re-enable signal */
}

/* HUP_EXIT - Called by a SIGHUP hangup signal.
 *	Tries to save all modified buffers before exiting.
 *	Note that the TTY is not touched at all, although the terminal mode
 *	flag is set just in case further error handling routines are invoked.
 */
hup_exit()
{	extern int trm_mode;		/* See e_disp.c */

	trm_mode = -1;			/* Say TTY is now detached */
	saveworld((struct buffer *)0, 0);	/* Save world, w/o feedback */
	exit(1);
}

errint()		/* Routine provided for ADB jumps */
{	askerr();
}
char askh1[] = "\
A - Abort process\n\
B - Breakpoint (must have \"bpt:b\" set in ADB)\n\
C - Continue\n\
D - Diagnostic command mode\n";
char askh2[] = "\
S - Try to save current buffer\n\
W - Try to save world (all modified buffers)\n";

int bsaving = 0;	/* Set while in middle of saving buffer(s) */

askerr()
{	register struct buffer *b;
	char linbuf[100];
	char *asklin();
	extern int (*funtab[])();	/* In E_CMDS.C */
	int ostate;

	ostate = clean_exit();		/* Clean up TTY if not already done */
reask:
	printf("(A,B,C,D,S,W,?)");
	switch(upcase(*asklin(linbuf)))
	  {
		case '?':
			writez(1,askh1);	/* Too long for &$@! printf */
			writez(1,askh2);	/* Too long for &$@! V6 C */
			break;			/*    optimizer (/lib/c2) */
		case 'A':
			abort();
			break;
		case 'B':
			bpt();
			break;
		case 'Q':
		case 'C':
			goto done;
		case 'D':
			if(funtab[FN_DEBUG])
				(*funtab[FN_DEBUG])(-1);
			else printf("Sorry, no diagnostics\n");
			break;
		case 'S':	/* Try to save current buffer only */
			b = cur_buf;
			goto savb;
		case 'W':	/* Try to save all modified buffers */
			b = 0;
		savb:	if(bsaving++)
			  {	printf("Already saving -- continued");
				goto done;
			  }
			saveworld(b, 1);	/* Save, with feedback */
			bsaving = 0;
			break;
	  }
	goto reask;
done:
	if(ostate > 0)
		set_tty();
}

char *
asklin(acp)
char *acp;
{	register char *cp;
	register int c;
	cp = acp;
	while((c = tgetc()) != LF)
		*cp++ = c;
	*cp++ = 0;
	return(acp);
}
