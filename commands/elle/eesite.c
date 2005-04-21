/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/* EESITE	Site dependent frobs
 *	Primarily TS_ routines for TTY control.  Most site-dependent
 *	routine is TS_INP for detection of TTY input.
 */

#include "elle.h"

#if !(V6)
#include <signal.h>	/* For SIGTSTP in ts_pause */
#else
#include "eesigs.h"
#endif

int tsf_pause = 0;	/* Set if ts_pause works.  Ref'd by equit in e_main */

#if !(SYSV || BBN)	/* SYSV and BBN have weird tty calls */

#if MINIX
#include <termios.h>
struct termios origterm, newterm;
#else
#if V6
	/* Normal V6 declarations, must provide explicitly */
struct sgttyb {
	char sg_ispeed;
	char sg_ospeed;
	char sg_erase;
	char sg_kill;
	int sg_flags;
};
#define ECHO (010)
#define CRMOD (020)
#define RAW (040)
#else
	/* Normal V7 UNIX declarations, can use include file */
#include <sgtty.h>
#endif

struct sgttyb nstate;	/* Both V6 and V7 */
struct sgttyb ostate;	/* Both V6 and V7 */
#endif /*!(SYSV || BBN)*/
#endif /*!MINIX*/


#if BBN		/* BBN system frobs */
#include "/sys/sys/h/modtty.h"
struct modes  nstate;
struct modes  ostate;
#endif /*BBN*/

#if DNTTY		/* DN TTY frobs */
#include <tty.h>
char partab[2];		/* to satisfy obscene ref in tty.h */
#endif /*DNTTY*/


#if (UCB || TOPS20)		/* UCB, TOPS20 additional frobs */
#include <sys/ioctl.h>		/* For ts_inp() and tldisc */
#if IMAGEN
struct tchars otchars, ntchars;	/* Original and new tchars */
#endif /*IMAGEN*/
#endif /*(UCB || TOPS20)*/

#if SYSV		/* System V (and PC/IX) crocks */
#include <termio.h>
#include <sys/ioctl.h>

struct termio	/* terminal i/o status flags */
	origterm,	/* status of terminal at start of ELLE */
	newterm;	/* status of terminal when using ELLE */
#endif /*SYSV*/

/* TS_INP
 *	Ask system if terminal input is available (on file descriptor 0).
 *	Returns non-zero if so, else returns zero.
 *	Very important that this call NOT hang or block in any way,
 *	because it is used to detect type-ahead by the user;
 *	return should be immediate whether or not input is waiting.
 */
ts_inp()
{
#if BBN				/* Idiosyncratic */
	int   cap_buf[2];
	capac (0, &cap_buf[0], 4);
	return (cap_buf[0]);
#endif /*BBN*/

#if (DNTTY || ONYX)		/* Have "empty()" syscall */
	return(empty(0) ? 0 : 1);
#endif /*DNTTY || ONYX*/
#if (UCB || TOPS20)		/* Have FIONREAD ioctl */
	long retval;
	if(ioctl(0,FIONREAD,&retval))	/* If this call fails, */
		return(0);		/* assume no input waiting */
	return((retval ? 1 : 0));
#endif /*UCB || TOPS20*/
#if COHERENT
	int retval;
	ioctl(0, TIOCQUERY, &retval);
	return((retval ? 1 : 0));
#endif /*COHERENT*/
#if VENIX86
	struct sgttyb iocbuf;
	ioctl(0, TIOCQCNT, &iocbuf);
	return(iocbuf.sg_ispeed != 0 );
#endif /*VENIX86*/

#if !(BBN||COHERENT||DNTTY||ONYX||TOPS20||UCB||VENIX86)
	return(0);		/* Default - never any type-ahead, sigh */
#endif
}


/* TS_INIT()
 *	Get terminal information from system, initialize things for
 *	ts_enter and ts_exit.  This is called before t_init.
 *	Must set "trm_ospeed".
 */
ts_init()
{
#if DNTTY
	signal(16,1);		/* DN peculiar - turn off ctl-A */
#endif /*DNTTY*/

#if !(MINIX || SYSV || BBN)		/* Normal UNIX stuff */
	ioctl(1, TIOCGETP, &ostate);	/* Remember old state */
	nstate = ostate;		/* Set up edit-mode state vars */
	nstate.sg_flags |= RAW;			/* We'll want raw mode */
	nstate.sg_flags &= ~(ECHO|CRMOD);	/* with no echoing */
	trm_ospeed = ostate.sg_ospeed;

#if (IMAGEN && UCB)
	/* Get around 4.1+ remote/local flow control bug (from Gosmacs) */
	ioctl(0, TIOCGETC, &otchars);  /* Save original tchars */
	ntchars = otchars;
	ntchars.t_startc = -1;		/* Kill start/stop */
	ntchars.t_stopc  = -1;
	ioctl(0, TIOCSETC, &ntchars);
#endif /*IMAGEN && UCB*/
#endif /*!(SYSV || BBN)*/

#if BBN
	modtty(1, M_GET | M_MODES, &ostate, sizeof(ostate));	/* Save old */
	modtty(1, M_GET | M_MODES, &nstate, sizeof(nstate));	/* Setup new */
	nstate.t_erase = nstate.t_kill = nstate.t_intr = nstate.t_esc =
		nstate.t_eof = nstate.t_replay = 0377;
	nstate.t_quit = BELL;			/* ^G */
	nstate.t_breaks = TB_ALL;		/* break on all */
	nstate.t_iflags &= ~TI_ECHO & ~TI_NOSPCL & ~TI_CRMOD;
				/* no echos, specials on, no CR -> LF*/
	nstate.t_iflags |= TI_CLR_MSB;			/* ignore parity */
	nstate.t_oflags &= ~TO_CRMOD & ~TO_AUTONL;	/* no CR -> NL */
	if (trm_flags & NOXONOFF)
		nstate.t_oflags &= ~TO_XONXOFF;
	else
		nstate.t_oflags |= TO_XONXOFF;   

	nstate.t_oflags |= TO_CLR_MSB;		/* no special high bits */
	nstate.t_pagelen = 0;			/* no paging of output */
	trm_ospeed = ostate.t_ospeed;
#endif /*BBN*/

#if MINIX
	tcgetattr(0, &origterm);	/* How things are now */
	newterm = origterm;		/* Save them for restore on exit */

	/* input flags */
	newterm.c_iflag |= IGNBRK;	/* Ignore break conditions.*/
	newterm.c_iflag &= ~INLCR;	/* Don't map NL to CR on input */
	newterm.c_iflag &= ~ICRNL;      /* Don't map CR to NL on input */
	newterm.c_iflag &= ~BRKINT;	/* Do not signal on break.*/
	newterm.c_iflag &= ~IXON;	/* Disable start/stop output control.*/
	newterm.c_iflag &= ~IXOFF;	/* Disable start/stop input control.*/

	/* output flags */
	newterm.c_oflag &= ~OPOST;	/* Disable output processing */

	/* line discipline */
	newterm.c_lflag &= ~ISIG;	/* Disable signals.*/
	newterm.c_lflag &= ~ICANON;	/* Want to disable canonical I/O */
	newterm.c_lflag &= ~ECHO;	/* Disable echo.*/
	newterm.c_lflag &= ~ECHONL;	/* Disable separate NL echo.*/
	newterm.c_lflag &= ~IEXTEN;	/* Disable input extensions.*/

	newterm.c_cc[VMIN] = 1;		/* Min. chars. on input (immed) */
	newterm.c_cc[VTIME] = 0;        /* Min. time delay on input (immed) */

	/* Make it stick */
	tcsetattr(0, TCSANOW, &newterm);
#endif /*MINIX*/

#if SYSV
	ioctl(0, TCGETA, &origterm);	/* How things are now */
	newterm = origterm;		/* Save them for restore on exit */

	/* input flags */
	newterm.c_iflag |= IGNBRK;	/* Ignore break conditions.*/
	newterm.c_iflag &= ~INLCR;	/* Don't map NL to CR on input */
	newterm.c_iflag &= ~ICRNL;      /* Don't map CR to NL on input */
	newterm.c_iflag &= ~BRKINT;	/* Do not signal on break.*/
	newterm.c_iflag &= ~IXON;	/* Disable start/stop output control.*/
	newterm.c_iflag &= ~IXOFF;	/* Disable start/stop input control.*/

	/* line discipline */
	newterm.c_lflag &= ~ISIG;	/* Disable signals.*/
	newterm.c_lflag &= ~ICANON;	/* Want to disable canonical I/O */
	newterm.c_lflag &= ~ECHO;	/* Disable echo.*/

	newterm.c_cc[4] = 1;		/* Min. chars. on input (immed) */
	newterm.c_cc[5] = 1;	        /* Min. time delay on input (immed) */

	/* Make it stick */
	ioctl(0, TCSETA, &newterm);
#endif /*SYSV*/

#if (UCB || TOPS20)
	{	int tldisc;
		ioctl(0, TIOCGETD, &tldisc);	/* Find line discipline */

/* The flag IGN_JOB_CONTROL has been introduced to allow job control haters
 * to simply ignore the whole thing.  When ELLE is compiled with
 * -DIGN_JOB_CONTROL, it will exit properly when the Return to Superior
 * command is executed.
*/
#if SIGTSTP
#ifndef IGN_JOB_CONTROL
		if(tldisc == NTTYDISC) tsf_pause = 1;
#endif
#endif /*SIGTSTP*/

	}
#endif /*UCB || TOPS20*/
}

/* TS_ENTER()
 *	Tell system to enter right terminal mode for editing.
 *	This is called before t_enter.
 */
ts_enter()
{
#if !(MINIX || SYSV || BBN)
	ioctl(1, TIOCSETP, &nstate);
#if IMAGEN && UCB
	ioctl(0, TIOCSETC, &ntchars);	/* Restore new tchars */
#endif /*IMAGEN && UCB*/
#endif /*!(SYSV||BBN)*/

#if BBN
	modtty (1, M_SET | M_MODES, &nstate, sizeof (nstate));
#endif /*BBN*/

#if MINIX
	/* Make it behave as previously defined in ts_init */
	tcsetattr(0, TCSANOW, &newterm);
#endif /*SYSV*/

#if SYSV
	/* Make it behave as previously defined in ts_init */
	ioctl(0, TCSETA, &newterm);
#endif /*SYSV*/

#if DNTTY	/* DN hackery!  Enable 8-bit input so as to read meta bit. */
	if(dbg_isw)
	  {	tpoke(TH_CSET,T_2FLGS2,EEI);	/* Enable ints */
		tpoke(TH_CSETB,T_QUIT, 0377);	/* Turn off QUIT intrpt */
	  }
	else if(trm_flags & TF_METAKEY)
		tpoke(TH_CSET,T_2FLGS2,T2_LITIN); /* Turn on 8-bit input! */
#endif /*DNTTY*/
}

/* TS_EXIT
 *	Tell system to restore old terminal mode (we are leaving edit mode).
 *	This is called after t_exit.
 */
ts_exit()
{
#if DNTTY
	if(dbg_isw)
		tpoke(TH_CCLR,T_2FLGS2,EEI);	/* Turn off EEI bit */
	else if(trm_flags & TF_METAKEY)
		tpoke(TH_CCLR,T_2FLGS2,T2_LITIN); /* Turn off 8-bit input */
#endif /*DNTTY*/

#if !(MINIX || SYSV || BBN)
	ioctl(1, TIOCSETP, &ostate);	/* SYSV and BBN don't use stty */
#if IMAGEN && UCB
	ioctl(0, TIOCSETC, &otchars);	/* Restore original tchars */
#endif /*IMAGEN && UCB*/
#endif /*!(SYSV || BBN)*/

#if BBN
	modtty (1, M_SET | M_MODES, &ostate, sizeof (ostate));
#endif /*BBN*/

#if MINIX
	tcsetattr(0, TCSANOW, &origterm);
#endif /*MINIX*/

#if SYSV
	ioctl(0, TCSETA, &origterm);
#endif /*SYSV*/
}

#if DNTTY
int thkcmd[] { 0, 0, -1 };
tpoke(cmd,bn,val)
int cmd, bn, val;
{
	thkcmd[0] = cmd|bn;
	thkcmd[1] = val;
	if(ttyhak(0,&thkcmd) < 0)
		return(-1);
	else return(thkcmd[1]);
}
#endif /*DNTTY*/


/* TS_PAUSE - Stop process and return control of TTY to superior.
 *	There is also a flag variable, TSF_PAUSE, which indicates
 *	whether or not this routine will actually do anything.
 */
#if TOPS20
#include <jsys.h>
#endif

ts_pause()
{
#if TOPS20
	int acs[5];
	jsys(HALTF, acs);
#endif

#if UCB
#if SIGTSTP
	signal(SIGTSTP, SIG_DFL);
#if BSD4_2
#define	mask(s)	(1 << ((s)-1))
	sigsetmask(sigblock(0) &~ mask(SIGTSTP));
#endif /*BSD4_2*/
	kill(0, SIGTSTP);
#if BSD4_2
	sigblock(mask(SIGTSTP));
#endif /*BSD4_2*/
#endif /*SIGTSTP*/
#endif /*UCB*/
}

ts_winsize()
{
#ifdef TIOCGWINSZ
	struct winsize winsize;

	if (ioctl(1, TIOCGWINSZ, &winsize) == 0) {
		if (winsize.ws_row != 0) scr_ht = winsize.ws_row;
		if (winsize.ws_col != 0) scr_wid = winsize.ws_col;
	}
#endif
}
