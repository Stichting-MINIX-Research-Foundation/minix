/*	$NetBSD: ext.h,v 1.22 2012/01/10 23:39:11 joerg Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)ext.h	8.2 (Berkeley) 12/15/93
 */

/*
 * Telnet server variable declarations
 */
extern char	options[256];
extern char	do_dont_resp[256];
extern char	will_wont_resp[256];
extern int	linemode;	/* linemode on/off */
#ifdef	LINEMODE
extern int	uselinemode;	/* what linemode to use (on/off) */
extern int	editmode;	/* edit modes in use */
extern int	useeditmode;	/* edit modes to use */
extern int	alwayslinemode;	/* command line option */
# ifdef	KLUDGELINEMODE
extern int	lmodetype;	/* Client support for linemode */
# endif	/* KLUDGELINEMODE */
#endif	/* LINEMODE */
extern int	flowmode;	/* current flow control state */
extern int	restartany;	/* restart output on any character state */
#ifdef DIAGNOSTICS
extern int	diagnostic;	/* telnet diagnostic capabilities */
#endif /* DIAGNOSTICS */
#ifdef SECURELOGIN
extern int	require_secure_login;
#endif
#ifdef AUTHENTICATION
extern int	auth_level;
#endif

extern slcfun	slctab[NSLC + 1];	/* slc mapping table */
extern char	terminaltype[41];

/*
 * I/O data buffers, pointers, and counters.
 */
extern char	ptyobuf[BUFSIZ+NETSLOP], *pfrontp, *pbackp;
extern char	netibuf[BUFSIZ], *netip;
extern char	netobuf[BUFSIZ+NETSLOP], *nfrontp, *nbackp;
extern char	*neturg;		/* one past last bye of urgent data */
extern int	pcc, ncc;
extern int	pty, net;
extern char	*line;
extern int	SYNCHing;		/* we are in TELNET SYNCH mode */

#include <sys/cdefs.h>

extern void
	_termstat(void),
	add_slc(char, char, cc_t),
	check_slc(void),
	change_slc(int, int, cc_t),
	cleanup(int) __dead,
	clientstat(int, int, int),
	copy_termbuf(char *, int),
	deferslc(void),
	defer_terminit(void),
	do_opt_slc(unsigned char *, int),
	doeof(void),
	dooption(int),
	dontoption(int),
	edithost(const char *, const char *),
	fatal(int, const char *) __dead,
	fatalperror(int, const char *) __dead,
	get_slc_defaults(void),
	init_env(void),
	init_termbuf(void),
	interrupt(void),
	localstat(void),
	flowstat(void),
	netclear(void),
	netflush(void),
#ifdef DIAGNOSTICS
	printoption(const char *, int),
	printdata(const char *, char *, int),
#ifndef ENCRYPTION
	printsub(int, unsigned char *, int),
#endif
#endif
	ptyflush(void),
	putchr(int),
	recv_ayt(void),
	send_do(int, int),
	send_dont(int, int),
	send_slc(void),
	send_status(void),
	send_will(int, int),
	send_wont(int, int),
	sendbrk(void),
	sendsusp(void),
	set_termbuf(void),
	start_login(char *, int, char *) __dead,
	start_slc(int),
	startslave(char *, int, char *),
	suboption(void),
	telrcv(void),
	ttloop(void),
	tty_binaryin(int),
	tty_binaryout(int);

extern char *
	putf(const char *, char *);

extern int
	end_slc(unsigned char **),
	getnpty(void),
	getpty(int *),
	spcset(int, cc_t *, cc_t **),
	stilloob(int),
	terminit(void),
	termstat(void),
	tty_flowmode(void),
	tty_restartany(void),
	tty_isbinaryin(void),
	tty_isbinaryout(void),
	tty_iscrnl(void),
	tty_isecho(void),
	tty_isediting(void),
	tty_islitecho(void),
	tty_isnewmap(void),
	tty_israw(void),
	tty_issofttab(void),
	tty_istrapsig(void),
	tty_linemode(void);

extern void
	tty_rspeed(int),
	tty_setecho(int),
	tty_setedit(int),
	tty_setlinemode(int),
	tty_setlitecho(int),
	tty_setsig(int),
	tty_setsofttab(int),
	tty_tspeed(int),
	willoption(int),
	wontoption(int),
	writenet(unsigned char *, int);

extern int output_data(const char *, ...)
	__attribute__((__format__(__printf__, 1, 2)));
extern int output_datalen(const char *, size_t);

#ifdef	ENCRYPTION
extern char	*nclearto;
#endif	/* ENCRYPTION */

/*
 * The following are some clocks used to decide how to interpret
 * the relationship between various variables.
 */
extern struct {
    int
	system,			/* what the current time is */
	echotoggle,		/* last time user entered echo character */
	modenegotiated,		/* last time operating mode negotiated */
	didnetreceive,		/* last time we read data from network */
	ttypesubopt,		/* ttype subopt is received */
	tspeedsubopt,		/* tspeed subopt is received */
	environsubopt,		/* environ subopt is received */
	oenvironsubopt,		/* old environ subopt is received */
	xdisplocsubopt,		/* xdisploc subopt is received */
	baseline,		/* time started to do timed action */
	gotDM;			/* when did we last see a data mark */
} clocks;

#ifndef	DEFAULT_IM
# define DEFAULT_IM	"\r\n\r\n4.4 BSD UNIX (%h) (%t)\r\n\r\r\n\r"
#endif

#ifdef AUTHENTICATION
#include <libtelnet/auth.h>
#endif

#ifdef ENCRYPTION
#include <libtelnet/encrypt.h>
#endif
