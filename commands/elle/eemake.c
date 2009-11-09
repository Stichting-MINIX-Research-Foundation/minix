/* ELLE - Copyright 1982, 1985, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/* EEMAKE - IMAGEN configuration functions for interfacing to "make".
 *	Written by (I think) Chris Ryland at IMAGEN, who also
 * wrote other functions scattered through ELLE.  These are either
 * conditionalized or are commented as being derived from the IMAGEN
 * configuration.
 *
 * KLH: An attempt has been made to keep these routines updated as ELLE
 * changed, but their workings cannot be guaranteed.
 */


/*
 * eemake: "make" (and other program) support
 *
 * Next-error depends on programs writing error messages of the form:
 *  "file", line n: message
 * which is a de facto standard, at least in some worlds.
 */

#include "elle.h"

#if !(IMAGEN)		/* Only with IMAGEN config for now */
f_xucmd() {}
f_make() {}
f_nxterr() {}
#else

#include <stdio.h>

struct buffer *exec_buf;	/* Ptr to "Execution" buffer */
				/* Only external ref is from e_buff.c */

#define MSGLENGTH (scr_wid - 11)	/* Max length of message */
int fresh_make = 1;			/* Fresh Execution buffer contents */
chroff last_error_BOL;			/* Dot for last-error's BOL */

/* EFUN: "Execute Unix Command" */
f_xucmd()
{
	make_or_unix_cmd(0);
}

/* EFUN: "Execute Make" */
f_make()
{
	make_or_unix_cmd(1);
}

/* EFUN: "Find Next Error" */
f_nxterr()
{
	register struct sbstr *sb;
	register char *cp;
	register int c;
	char file[64], line[32];
#ifdef ONEWINDOW
	char msg[512];
#endif
	chroff linedot;
	int lineno;
	register int len;

	sel_execbuf();
	if (! fresh_make)
	  {	e_go(last_error_BOL);
		e_gonl();
	  }
	else
	  {	fresh_make = 0;
		e_gobob();
		last_error_BOL = e_dot();
	  }

	/* Looking for `"file", line n: msg' */
	if (! e_search("\", line ", 8, 0))
		goto failed;
	linedot = e_dot();
	e_gobol();			/* Found something, get to BOL */
	if (e_getc() != '"')
		goto failed;		/* Insist on beginning "file" */
	cp = file;			/* Pick up filename */
	while ((c = e_getc()) != '"')
		*cp++ = c;
	*cp = 0;
	e_go(linedot);			/* Back to after "line " */
	cp = line;
	while ((c = e_getc()) >= '0' && c <= '9')
		*cp++ = c;
	*cp = 0;
	lineno = atoi(line);		/* Pick up line number */
#ifdef ONEWINDOW
	cp = msg;			/* Now get rest of line to msg */
	len = 0;			/* But keep length <= MSGLENGTH */
	e_getc();			/* Go past purported space */
	while ((c = e_getc()) != LF && c != EOF && len <= MSGLENGTH)
	  {	if (c == '\t')
			len = (len + 8) & ~07;
		else if (c < ' ' || c > 0176)
			len += 2;
		else
			++len;
		*cp++ = c;
	  }
	*cp = 0;
	if (len > MSGLENGTH)
		strcat(msg, "...");
#ifdef DEBUG
	say("file ");
	saytoo(file);
	saytoo(", line ");
	saytoo(line);
	saytoo(", msg: ");
	sayntoo(msg);
#else
	say(line);
	saytoo(": ");
	sayntoo(msg);
#endif /*DEBUG*/
#else /* -ONEWINDOW */
	f_begline();			/* Get to start of line */
	last_error_BOL = e_dot();	/* Remember this position */
	exp_p = 1;			/* Make this the new top line */
	exp = 0;
	f_newwin();
	upd_wind(0);
#endif /*ONEWINDOW*/

	/* Now, visit the given file and line */
#ifdef ONEWINDOW	
#else
	f_othwind();			/* To other window */
#endif
	find_file(file);
	f_gobeg();
	down_line(lineno - 1);
#ifdef READJUST				/* NAW */
	exp_p = 1;
	exp = cur_win->w_ht / 4;	/* Adjust how we look at "error" */
	f_newwin();
#endif /*READJUST*/
	return;

failed:	ding("No more errors!");
	fresh_make = 1;			/* Fake-out: pretend starting over */
	return;
}


/* Do the "cmd" and put its output in the Execution buffer */
do_exec(cmd, nicely)
char *cmd;
int nicely;
{
	register int n;
	int status, res, pid, fd[2];
	char nicecmd[256];
	char pipebuff[512];
	struct buffer *b;

	b = cur_buf;
	sel_execbuf();			/* Get our execution buffer up */
	ed_reset();			/* Clear it out */
	fresh_make = 1;
	upd_wind(0);
	if (nicely)
		sayntoo(" ...starting up...");
	else
		sayntoo(" ...starting up (nasty person)...");
	pipe(fd);
	switch (pid = fork())
	  {
	case -1:
		/* Fork failed, in parent */
		ding("Cannot fork!?!");
		break;

	case 0: /* In child */
		for (n = 0; n < 20; ++n)
			if (n != fd[0] && n != fd[1])
				close(n);
		open("/dev/null", 0);	/* Give ourselves empty stdin */
		dup(fd[1]);
		dup(fd[1]);		/* stdout, stderr out to pipe */
		close(fd[1]);		/* Close the pipe itself */
		close(fd[0]);
		if (nicely)
		  {	strcpy(nicecmd, "nice -4 ");
			strcat(nicecmd, cmd);
			execl("/bin/sh", "sh", "-c", nicecmd, 0);
		  }
		else
			execl("/bin/sh", "sh", "-c", cmd, 0);
		write(1, "Cannot execute!", 15);
		_exit(-1);
		break;

	default:
		/* Parent */
		close(fd[1]);		/* Close the output direction */
		while ((n = read(fd[0], pipebuff, sizeof(pipebuff))) > 0)
		  {	ed_nsins(pipebuff, n);
			upd_wind(0);
			saynow("Chugging along...");
		  }
		close(fd[0]);
		while ((res = wait(&status)) != pid && res != -1)
			;		/* Wait for this fork to die */
		f_bufnotmod();		/* Buffer is fresh */
		saynow("Done!");
		break;
	  }
	f_othwind();			/* Back to other window */
	chg_buf(b);			/* Back to original buffer */
}

char last_make_cmd[256];		/* Last Unix/make command */
int have_last_make_cmd = 0;

make_or_unix_cmd(domake)
int domake;
{
#if APOLLO
	register int nicely = exp == 16;	/* modification for apollo */
#else
	register int nicely = exp != 16;
#endif /*-APOLLO*/
	register char *reply, *cmd = 0;

	if (domake)			/* If "make"-style, */
	  {	int savp = exp_p;
		exp_p = 1;
		f_savefiles();		/*  write modified files quietly */
		exp_p = savp;
	  }
	if (exp_p || ! domake)
	  {		/* Arg given make, or Unix command */
		reply = ask((! domake) ? "Unix command: " : "Command: ");
		if (! reply)
			return;
		if (*reply == 0)
		  {	if (have_last_make_cmd)
				cmd = &last_make_cmd[0];
			else
			  {	chkfree(reply);
				ding("No previous command!");
				return;
			  }
		  }
		else
			cmd = reply;
		if (cmd != &last_make_cmd[0])	/* Update last command */
			strcpy(last_make_cmd, cmd);
		have_last_make_cmd = 1;
		say("Command: ");
		sayntoo(cmd);
		do_exec(cmd, nicely);
		chkfree(reply);
	  }
	else if (have_last_make_cmd)
	  {	say("Command: ");
		sayntoo(last_make_cmd);
		do_exec(last_make_cmd, nicely);
	  }
	else
	  {	saynow("Command: make");
		do_exec("make", nicely);
	  }
}

sel_execbuf()
{	if(!exec_buf)
	  {	/* Make execution buffer; don't let anyone kill it */
		exec_buf = make_buf("Execution");
		exec_buf->b_flags |= B_PERMANENT;
	  }
	popto_buf(exec_buf);
}

/* Utility: pop the given buffer to a window, getting into 2-window mode */
popto_buf(b)
register struct buffer *b;
{
	/* See if we already have this buffer in a visible window */
	if (b == cur_win->w_buf)
	  {	if (oth_win == 0)
		  {	f_2winds();
			f_othwind();		/* Get back to our window */
		  }
	  }
	else if (oth_win != 0 && b == oth_win->w_buf)
		f_othwind();
	else if (oth_win == 0)
	  {		/* One window mode */
		f_2winds();			/* Get two, get into second */
		sel_buf(b);			/* Select our new buffer */
	  }
	else
	  {	f_othwind();			/* Get to other window */
		sel_buf(b);			/*  and select our buffer */
	  }
}

#endif /*IMAGEN*/
