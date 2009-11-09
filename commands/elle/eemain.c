/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EEMAIN	ELLE Main Command Loop
 */

#include "elle.h"

#include <stdio.h>
#if !(V6)
#include <signal.h>
#else
#include "eesigs.h"		/* Use this on V6 system */
#endif /*V6*/

char *argfile[MAXARGFILES];	/* Filename args at startup */

extern int (*sbm_debug)();
extern int (*sbv_debug)();
int (*vfy_vec)();	/* If non-zero, routine to verify data
			 * after each main-loop command */

main (argc, argv)
int argc;
char **argv;
{
	register int c;		/* Current command character */
	register int i;
	static int waitct;
	extern int errsbm();
#if SUN
	extern int sun_rdevf;	/* from EESUN */
#endif
#ifdef STKMEM
	char stackm[STKMEM];		/* Allocate some unused stack space */
#endif /*STKMEM*/

	sbv_debug = errsbm;		/* Load with addrs of routine to */
	sbm_debug = errsbm;		/* process SB and SBM errors. */

#ifdef STKMEM
	sbm_init(&stackm[0],(SBMO)STKMEM);	/* Initialize mem alloc rtns */
#endif /*STKMEM*/
#if SUN
	sun_main(&argc, argv);		/* On SUN, invoke window startup */
#endif /*SUN*/

	setbuf(stdout, (char *)NULL);	/* Remove all stdio buffering */
	setbuf(stderr, (char *)NULL);	/* in case of error reports. */

	waitct = 0;			/* debugging */
	doargs(argc,argv);		/* Set up args */
	initialize ();			/* Initialize the editor */

	if (argfile[0])			/* shell line arg */
		find_file(argfile[0]);
#if MAXARGFILES > 1
	if(argfile[1])
	  {	f_2winds();		/* Make 2 windows, go to 2nd */
		i = 1;
#if MAXARGFILES > 2
		for (; i < MAXARGFILES; ++i)
#endif /* > 2 files */
			find_file(argfile[i]);	/* Get further file(s) */
		f_othwind();		/* Move back to 1st window */
	  }
#endif /* > 1 file */

	redp(RD_SCREEN|RD_MODE);	/* Clear and show mode line */
	setexit(0);			/* catch for ints, ^G throws */

/* -----------------------------------------------------------
**			ELLE MAIN LOOP
**
*/
	for (;;)
	  {
		/* First set up default arg unless last cmd specified it */
		if(this_cmd != ARGCMD)
		  {	exp = 1;		/* Default arg is 1 */
			exp_p = 0;		/* Say no explicit arg */
			last_cmd = this_cmd;
		  }
		this_cmd = 0;

		askclr();		/* If stuff asked, say to clear it */
		if(cmd_wait())
			waitct++;
		else if(rd_type != 0)
			redisplay();	/* Redisplay if needed and no input */
#if SUN
		sun_rdevf = 1;		/* Allow mouse events on this input */
#endif
		c = cmd_read();		/* Read an editor command */
		sayclr();		/* Ask to clear echo area cleverly */

#if SUN
		if(c != -1)		/* SUN may not have real input */
#endif					/*    if mouse event happened. */
			cmd_xct(c);	/* Execute the command char! */

		if(vfy_vec)		/* If debugging, */
			(*vfy_vec)(1);	/* verify data structs right away */
	  }
}

char *prof_file;	/* Can specify user profile filename */

doargs(argc,argv)
int argc;
char **argv;
{	register int cnt, c;
	register char **av;
	extern int tibfmsk;
	int argfiles = 0;
	int argsignored = 0;

	av = argv;
	cnt = argc;

#if V6	/* V6 doesn't have environment thus no TERM var */
	/* Hack to force terminal type; analyze pgm name to get
	 * possible ".type" suffix.
	 */
	if(cnt && (c = strlen(*av)))
	  while(--c >= 0)
	  {	switch(av[0][c])
		  {	case '.':
				tv_stype = &av[0][c+1];
			case '/':
				break;
			default: continue;
		  }
		break;
	  }
#endif /*V6*/

	while(--cnt > 0)
	  {	++av;
		if(*av[0] != '-')	/* If not switch, */
		  {			/* assume it's an input filename */
			if (argfiles < MAXARGFILES)
				argfile[argfiles++] = *av;
			else
				++argsignored;
			continue;
		  }
		c = upcase(av[0][1]);
		switch(c)		/* Switches without args */
		  {	case 'I':	/* Allow debug ints */
				dbg_isw = 1;
				continue;
			case '8':		/* Ask for 8-bit input */
				tibfmsk = 0377;
				continue;
			case '7':		/* Ask for 7-bit input */
				tibfmsk = 0177;
				continue;
#if IMAGEN
			case 'R':	/* Debug redisplay stuff */
				dbg_redp = 1;
				continue;
#endif /*IMAGEN*/
		  }
		if(--cnt <= 0)
			goto stop;
		++av;
		switch(c)		/* Switches with args */
		  {	case 'T':	/* Terminal type */
				tv_stype = *av;
				break;	
			case 'P':
				prof_file = *av;
			default:
				goto stop;
		  }
		continue;
	stop:	printf("ELLE: bad switch: %s\n",*av);
		exit(1);
	  }
	if (argsignored > 0)
	  {	printf("ELLE: more than %d file args, %d ignored.\n",
			MAXARGFILES, argsignored);
		sleep(2);	/* Complain but continue after pause */
	  }
}

int f_throw();		/* throw function */
int bite_bag();		/* Error handling routine */
int hup_exit();		/* Hangup handling routine */

struct majmode ifunmode = { "Fundamental" };

initialize ()				/* Initialization */
{
#if SUN
	extern int sun_winfd;
#endif
	cur_mode = fun_mode = &ifunmode;	/* Set current major mode */
	unrchf = pgoal = -1;
	if(!homedir)
	  {
#if V6
		extern char *logdir();
		homedir = logdir();
#else /* V7 */
		homedir = getenv("HOME");
#endif /*-V6*/
	  }

	sbx_tset((chroff)0,0);		/* Create swapout file */
					/* (Temporary hack, fix up later) */
	hoard();			/* Hoard a FD for write purposes */

	redp_init();			/* Set up the display routines */
	init_buf();			/* Set up initial buffers */
	set_profile(prof_file);		/* Set up user profile */

#if SUN
	if(sun_winfd) sun_init();
#endif /*SUN*/

	/* Set up signal handlers */
#if 0					/* not really used */
	signal (SIGQUIT, f_throw);	/* Quit - on ^G */
#endif
#if !(MINIX)
	signal (SIGSYS, bite_bag);	/* Bad arg to Sys call */
#endif
	signal (SIGSEGV, bite_bag);	/* Segmentation Violation */
#if !(COHERENT)
	signal (SIGILL, bite_bag);	/* Illegal Instruction interrupt */
	signal (SIGBUS, bite_bag);	/* Bus Error interrupt */
#endif /*-COHERENT*/
#if !(TOPS20)				/* T20 just detaches job */
	signal (SIGHUP, hup_exit);	/* Terminal Hangup interrupt */
#endif /*-TOPS20*/
}


/* NOTE: This routine is not actually used, because ELLE does not
 * allow interrupts to do anything.
 */
/* EFUN: "Error Throw" */
f_throw ()			       /* abort whatever is going on */
{
	ring_bell ();
	curs_lin = -1000;		/* make t_curpos do something */
	redp(RD_MOVE);		/* crock: cursor seems to move, so fix it */
	signal(SIGQUIT, f_throw);	/* rearm signal */
/*	unwind_stack(main); */
	reset(1);			/* throw to main loop */
}

/* RING_BELL - General-purpose feeper when something goes wrong with
 *	a function.
 */
ring_bell()
{	t_bell();		/* Tell user something's wrong */

#if FX_SKMAC
        f_ekmac();		/* Stop collecting keyboard macro if any */
#endif /*FX_SKMAC*/
}

/* EFUN: "Return to Superior"
 *	Behavior here is somewhat system-dependent.  If it is possible to
 * suspend the process and continue later, we do not ask about modified
 * buffers.  Otherwise, we do.  Questioning can always be forced by using
 * the prefix ^U.
 *	Note that here we try to be very careful about not letting the user
 * exit while buffers are still modified, since UNIX flushes the process
 * if we exit.  Also, the code here conspires with sel_mbuf to rotate
 * through all modified buffers, complaining about a different one each time,
 * so that the user need not even know how to select a buffer!
 */
f_retsup()
{	register char *reply;
	register int c;
	register struct buffer *b, *b2;
	extern struct buffer *sel_mbuf();
	extern int tsf_pause;

	/* If we have capability of pausing and later continuing, do that,
	 * except if CTRL-U forces us into question/save/quit behavior.
	 */
	if(tsf_pause && (exp_p != 4))
	  {	clean_exit();		/* Return TTY to normal mode */
		ts_pause();		/* Pause this inferior */
		set_tty();		/* Continued, return to edit mode */
		redp(RD_SCREEN);
		return;
	  }

	/* Sigh, do more typical "Are you sure" questioning prior to
	 * killing the editor permanently.
	 */
	b = cur_buf;
	if((b = sel_mbuf(b)) || (b = sel_mbuf((struct buffer *)0)) )
	  {	if(b2 = sel_mbuf(b))
			reply = ask(
		"Quit: buffers %s, %s,... still have changes - forget them? ",
				b->b_name, b2->b_name);
		else
			reply = ask(
		"Quit: buffer %s still has changes - forget them? ",
				b->b_name);
		
	  }
	else
	  {
#if IMAGEN	/* Do not ask further if nothing modified */
		barf("Bye");
		clean_exit();
		exit(0);
#else
		reply = ask("Quit? ");
#endif /*-IMAGEN*/
	  }

	if (reply == 0)
		return;			/* Aborted, just return */

	c = upcase(*reply);		/* Get 1st char of reply */
	chkfree(reply);

	switch(c)
	  {	case 'Y':
#if IMAGEN
			barf("Bye");
#endif /*IMAGEN*/
			clean_exit();
			exit(0);
#if 0
		case 'S':		/* Suspend command for debugging */
			bkpt();
			return;
#endif /*COMMENT*/
		default:		/* Complain */
			ring_bell();
		case 'N':
			if(b)	/* B set if we have any modified buffers */
			  {	sel_buf(b);
				if(b->b_fn)
					saynow("Use ^X ^S to save buffer");
				else	saynow("Use ^X ^W to write out buffer");
			  }
	  }
}


#if FX_WFEXIT
/* EFUN: "Write File Exit" (not EMACS) - from IMAGEN config */
f_wfexit()
{
	exp_p = 1;		/* Ensure f_savefiles asks no questions */
	if (! f_savefiles())	/* Save all modified buffers, but */
		return;		/*  stay here if any save fails */
	saynow("Bye");
	clean_exit();
	exit(0);
}
#endif /*FX_WFEXIT*/

/* Subprocess-handling stuff; put here for time being. */

/* EFUN: "Push to Inferior" */
#if TOPS20
#include <frkxec.h>	/* Support for KCC forkexec() call */
#endif
f_pshinf()
{
	register int res;
	register int (*sav2)(), (*sav3)();
	int pid, status;
	char *shellname;
#if IMAGEN
	char fullshell[64];
#endif /*IMAGEN*/

	sav2 = signal(SIGINT, SIG_IGN);		/* Ignore TTY interrupts */
	sav3 = signal(SIGQUIT, SIG_IGN);	/* Ditto TTY "quit"s */
	clean_exit();				/* Restore normal TTY modes */

#if TOPS20
    {
	struct frkxec fx;
	fx.fx_flags = FX_WAIT | FX_T20_PGMNAME;
	fx.fx_name = "SYS:EXEC.EXE";
	fx.fx_argv = fx.fx_envp = NULL;
	if (forkexec(&fx) < 0)
		writerr("Cannot run EXEC");
    }
#else /*-TOPS20*/
	switch(pid = fork())
	  {	case -1:
			writerr("Cannot fork");
			break;
		case 0:		/* We're the child */
			for(res = 3; res < 20;)	/* Don't let inf hack fd's */
				close(res++);
#if V6
			execl("/bin/sh","-sh",0);
#else
			signal(SIGINT, SIG_DFL);	/* V7 shell wants this?? */
			signal(SIGQUIT, SIG_DFL);	/*	*/
#if IMAGEN
			if((shellname = getenv("SHELL")) == 0)
				shellname = "sh";
			strcpy(fullshell, "/bin/");
			strcat(fullshell, shellname);
			shellname = fullshell;
#else
			if((shellname = getenv("SHELL")) == 0)
				shellname = "/bin/sh";
#endif /*-IMAGEN*/

			if((shellname = getenv("SHELL")) == 0)
				shellname = "/bin/sh";
			execl(shellname, shellname, 0);
#endif /*-V6*/
			writerr("No shell!");
			exit(1);
			break;
		default:
			while((res = wait(&status)) != pid && res != -1);
			break;
	  }
#endif /*-TOPS20*/

	signal(SIGINT, sav2);		/* Restore signal settings */
	signal(SIGQUIT, sav3);
	set_tty();			/* Restore editor TTY modes */
	redp(RD_SCREEN|RD_MODE);	/* Done, redisplay */
}

/* Miscellaneous utility routines - memory alloc/free and string hacking.
 * If this page becomes overly large, it can be split off into a separate
 * file called E_MISC.
 */
char *
strdup(s)
char *s;	/* Note that STRCPY's return val must be its 1st arg */
{	char *strcpy();
	return(strcpy(memalloc((SBMO)(strlen(s)+1)), s));
}

char *
memalloc(size)
SBMO size;
{	register SBMA ptr;
	extern SBMA sbx_malloc();

	if ((ptr = (SBMA)sbx_malloc(size)) != 0)
		return((char *)ptr);
	barf("ELLE: No memory left");
	askerr();
	return(0);		/* If we dare to continue... */
}

chkfree (ptr)
SBMA ptr;
{
	if(!free(ptr))
	  {	errbarf("Something overwrote an allocated block!");
		askerr();
	  }
}


/* USTRCMP - Uppercase String Compare.
 *	Returns 0 if mismatch,
 *		1 if full match,
 *		-1 if str1 runs out first (partial match)
 */
ustrcmp(str1,str2)
char *str1, *str2;
{	register char *s1, *s2;
	register int c;
	s1 = str1; s2 = str2;
	while(c = *s1++)
	  {	if(c != *s2 && upcase(c) != upcase(*s2))
			return(0);
		s2++;
	  }
	return(c == *s2 ? 1 : -1);
}


/* WRITERR(str) - Output string to standard error output.
**	This is a separate routine to save a little space on calls.
*/
writerr(str)
char *str;
{	return(writez(2, str));
}

/* WRITEZ(fd, str) - Miscellaneous general-purpose string output.
 */
writez(fd,acp)
int fd;
char *acp;
{	register char *cp;
	cp = acp;
	while(*cp++);
	write(fd,acp,cp-acp-1);
}
