/*	$NetBSD: utility.c,v 1.32 2012/01/09 16:36:48 christos Exp $	*/

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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)utility.c	8.4 (Berkeley) 5/30/95";
#else
__RCSID("$NetBSD: utility.c,v 1.32 2012/01/09 16:36:48 christos Exp $");
#endif
#endif /* not lint */

#include <sys/utsname.h>
#include <ctype.h>
#define PRINTOPTIONS
#include "telnetd.h"

char *nextitem(char *);
void putstr(char *);

extern int not42;

/*
 * utility functions performing io related tasks
 */

/*
 * ttloop
 *
 *	A small subroutine to flush the network output buffer, get some data
 * from the network, and pass it through the telnet state machine.  We
 * also flush the pty input buffer (by dropping its data) if it becomes
 * too full.
 */

void
ttloop(void)
{

    DIAG(TD_REPORT, {output_data("td: ttloop\r\n");});
    if (nfrontp - nbackp) {
	netflush();
    }
    ncc = read(net, netibuf, sizeof netibuf);
    if (ncc < 0) {
	syslog(LOG_ERR, "ttloop:  read: %m");
	exit(1);
    } else if (ncc == 0) {
	syslog(LOG_INFO, "ttloop:  unexpected EOF from peer");
	exit(1);
    }
    DIAG(TD_REPORT, {output_data("td: ttloop read %d chars\r\n", ncc);});
    netip = netibuf;
    telrcv();			/* state machine */
    if (ncc > 0) {
	pfrontp = pbackp = ptyobuf;
	telrcv();
    }
}  /* end of ttloop */

/*
 * Check a descriptor to see if out of band data exists on it.
 */
int
stilloob(int s /* socket number */)
{
    struct pollfd set[1];
    int value;

    set[0].fd = s;
    set[0].events = POLLPRI;
    do {
	value = poll(set, 1, 0);
    } while ((value == -1) && (errno == EINTR));

    if (value < 0) {
	fatalperror(pty, "poll");
    }
    if (set[0].revents & POLLPRI) {
	return 1;
    } else {
	return 0;
    }
}

void
ptyflush(void)
{
	int n;

	if ((n = pfrontp - pbackp) > 0) {
		DIAG((TD_REPORT | TD_PTYDATA),
			{ output_data("td: ptyflush %d chars\r\n", n); });
		DIAG(TD_PTYDATA, printdata("pd", pbackp, n));
		n = write(pty, pbackp, n);
	}
	if (n < 0) {
		if (errno == EWOULDBLOCK || errno == EINTR)
			return;
		cleanup(0);
	}
	pbackp += n;
	if (pbackp == pfrontp)
		pbackp = pfrontp = ptyobuf;
}

/*
 * nextitem()
 *
 *	Return the address of the next "item" in the TELNET data
 * stream.  This will be the address of the next character if
 * the current address is a user data character, or it will
 * be the address of the character following the TELNET command
 * if the current address is a TELNET IAC ("I Am a Command")
 * character.
 */
char *
nextitem(char *current)
{
    if ((*current&0xff) != IAC) {
	return current+1;
    }
    switch (*(current+1)&0xff) {
    case DO:
    case DONT:
    case WILL:
    case WONT:
	return current+3;
    case SB:		/* loop forever looking for the SE */
	{
	    char *look = current+2;

	    for (;;) {
		if ((*look++&0xff) == IAC) {
		    if ((*look++&0xff) == SE) {
			return look;
		    }
		}
	    }
	}
    default:
	return current+2;
    }
}  /* end of nextitem */


/*
 * netclear()
 *
 *	We are about to do a TELNET SYNCH operation.  Clear
 * the path to the network.
 *
 *	Things are a bit tricky since we may have sent the first
 * byte or so of a previous TELNET command into the network.
 * So, we have to scan the network buffer from the beginning
 * until we are up to where we want to be.
 *
 *	A side effect of what we do, just to keep things
 * simple, is to clear the urgent data pointer.  The principal
 * caller should be setting the urgent data pointer AFTER calling
 * us in any case.
 */
void
netclear(void)
{
    char *thisitem, *next;
    char *good;
#define	wewant(p)	((nfrontp > p) && ((*p&0xff) == IAC) && \
				((*(p+1)&0xff) != EC) && ((*(p+1)&0xff) != EL))

#ifdef	ENCRYPTION
    thisitem = nclearto > netobuf ? nclearto : netobuf;
#else /* ENCRYPTION */
    thisitem = netobuf;
#endif	/* ENCRYPTION */

    while ((next = nextitem(thisitem)) <= nbackp) {
	thisitem = next;
    }

    /* Now, thisitem is first before/at boundary. */

#ifdef	ENCRYPTION
    good = nclearto > netobuf ? nclearto : netobuf;
#else /* ENCRYPTION */
    good = netobuf;	/* where the good bytes go */
#endif	/* ENCRYPTION */

    while (nfrontp > thisitem) {
	if (wewant(thisitem)) {
	    int length;

	    next = thisitem;
	    do {
		next = nextitem(next);
	    } while (wewant(next) && (nfrontp > next));
	    length = next-thisitem;
	    memmove(good, thisitem, length);
	    good += length;
	    thisitem = next;
	} else {
	    thisitem = nextitem(thisitem);
	}
    }

    nbackp = netobuf;
    nfrontp = good;		/* next byte to be sent */
    neturg = 0;
}  /* end of netclear */

/*
 *  netflush
 *		Send as much data as possible to the network,
 *	handling requests for urgent data.
 */
void
netflush(void)
{
    int n;

    if ((n = nfrontp - nbackp) > 0) {
	DIAG(TD_REPORT,
	    { output_data("td: netflush %d chars\r\n", n);
	      n = nfrontp - nbackp;	/* re-compute count */
	    });
#ifdef	ENCRYPTION
	if (encrypt_output) {
		char *s = nclearto ? nclearto : nbackp;
		if (nfrontp - s > 0) {
			(*encrypt_output)((unsigned char *)s, nfrontp - s);
			nclearto = nfrontp;
		}
	}
#endif	/* ENCRYPTION */
	/*
	 * if no urgent data, or if the other side appears to be an
	 * old 4.2 client (and thus unable to survive TCP urgent data),
	 * write the entire buffer in non-OOB mode.
	 */
	if ((neturg == 0) || (not42 == 0)) {
	    n = write(net, nbackp, n);	/* normal write */
	} else {
	    n = neturg - nbackp;
	    /*
	     * In 4.2 (and 4.3) systems, there is some question about
	     * what byte in a sendOOB operation is the "OOB" data.
	     * To make ourselves compatible, we only send ONE byte
	     * out of band, the one WE THINK should be OOB (though
	     * we really have more the TCP philosophy of urgent data
	     * rather than the Unix philosophy of OOB data).
	     */
	    if (n > 1) {
		n = send(net, nbackp, n-1, 0);	/* send URGENT all by itself */
	    } else {
		n = send(net, nbackp, n, MSG_OOB);	/* URGENT data */
	    }
	}
    }
    if (n < 0) {
	if (errno == EWOULDBLOCK || errno == EINTR)
		return;
	cleanup(0);
    }
    nbackp += n;
#ifdef	ENCRYPTION
    if (nbackp > nclearto)
	nclearto = 0;
#endif	/* ENCRYPTION */
    if (nbackp >= neturg) {
	neturg = 0;
    }
    if (nbackp == nfrontp) {
	nbackp = nfrontp = netobuf;
#ifdef	ENCRYPTION
	nclearto = 0;
#endif	/* ENCRYPTION */
    }
    return;
}  /* end of netflush */


/*
 * writenet
 *
 * Just a handy little function to write a bit of raw data to the net.
 * It will force a transmit of the buffer if necessary
 *
 * arguments
 *    ptr - A pointer to a character string to write
 *    len - How many bytes to write
 */
void
writenet(unsigned char *ptr, int len)
{
	/* flush buffer if no room for new data) */
	if ((&netobuf[BUFSIZ] - nfrontp) < len) {
		/* if this fails, don't worry, buffer is a little big */
		netflush();
	}

	memmove(nfrontp, ptr, len);
	nfrontp += len;

}  /* end of writenet */


/*
 * miscellaneous functions doing a variety of little jobs follow ...
 */
void
fatal(int f, const char *msg)
{
	char buf[BUFSIZ];

	(void)snprintf(buf, sizeof buf, "telnetd: %s.\r\n", msg);
#ifdef	ENCRYPTION
	if (encrypt_output) {
		/*
		 * Better turn off encryption first....
		 * Hope it flushes...
		 */
		encrypt_send_end();
		netflush();
	}
#endif	/* ENCRYPTION */
	(void)write(f, buf, (int)strlen(buf));
	sleep(1);	/*XXX*/
	exit(1);
}

void
fatalperror(f, msg)
	int f;
	const char *msg;
{
	char buf[BUFSIZ];

	(void)snprintf(buf, sizeof buf, "%s: %s", msg, strerror(errno));
	fatal(f, buf);
}

char editedhost[MAXHOSTNAMELEN];

void
edithost(const char *pat, const char *host)
{
	char *res = editedhost;

	if (!pat)
		pat = "";
	while (*pat) {
		switch (*pat) {

		case '#':
			if (*host)
				host++;
			break;

		case '@':
			if (*host)
				*res++ = *host++;
			break;

		default:
			*res++ = *pat;
			break;
		}
		if (res == &editedhost[sizeof editedhost - 1]) {
			*res = '\0';
			return;
		}
		pat++;
	}
	if (*host)
		(void) strncpy(res, host,
		    sizeof editedhost - (res - editedhost) -1);
	else
		*res = '\0';
	editedhost[sizeof editedhost - 1] = '\0';
}

static char *putlocation;

void
putstr(char *s)
{

	while (*s)
		putchr(*s++);
}

void
putchr(int cc)
{
	*putlocation++ = cc;
}

/*
 * This is split on two lines so that SCCS will not see the M
 * between two % signs and expand it...
 */
static const char fmtstr[] = { "%l:%M\
%p on %A, %d %B %Y" };

char *
putf(const char *cp, char *where)
{
	char *slash;
	time_t t;
	char db[100];
	struct utsname utsinfo;

	uname(&utsinfo);

	putlocation = where;

	while (*cp) {
		if (*cp != '%') {
			putchr(*cp++);
			continue;
		}
		switch (*++cp) {

		case 't':
			if ((slash = strstr(line, "/pts/")) == NULL)
				slash = strrchr(line, '/');
			if (slash == (char *) 0)
				putstr(line);
			else
				putstr(&slash[1]);
			break;

		case 'h':
			putstr(editedhost);
			break;

		case 'd':
			(void)time(&t);
			(void)strftime(db, sizeof(db), fmtstr, localtime(&t));
			putstr(db);
			break;

		case '%':
			putchr('%');
			break;

		case 's':
			putstr(utsinfo.sysname);
			break;

		case 'm':
			putstr(utsinfo.machine);
			break;

		case 'r':
			putstr(utsinfo.release);
			break;

		case 'v':
			putstr(utsinfo.version);
                        break;
		}
		cp++;
	}
	
	return (putlocation);
}

#ifdef DIAGNOSTICS
/*
 * Print telnet options and commands in plain text, if possible.
 */
void
printoption(const char *fmt, int option)
{
	if (TELOPT_OK(option))
		output_data("%s %s\r\n", fmt, TELOPT(option));
	else if (TELCMD_OK(option))
		output_data("%s %s\r\n", fmt, TELCMD(option));
	else
		output_data("%s %d\r\n", fmt, option);
	return;
}

void
printsub(
    int	   direction,	/* '<' or '>' */
    unsigned char *pointer,	/* where suboption data sits */
    int		   length)	/* length of suboption data */
{
    int i = 0;
#if	defined(AUTHENTICATION) || defined(ENCRYPTION)
    u_char buf[512];
#endif

	if (!(diagnostic & TD_OPTIONS))
		return;

	if (direction) {
	    output_data("td: %s suboption ",
	        direction == '<' ? "recv" : "send");
	    if (length >= 3) {
		int j;

		i = pointer[length - 2];
		j = pointer[length - 1];

		if (i != IAC || j != SE) {
		    output_data("(terminated by ");
		    if (TELOPT_OK(i))
			output_data("%s ", TELOPT(i));
		    else if (TELCMD_OK(i))
			output_data("%s ", TELCMD(i));
		    else
			output_data("%d ", i);
		    if (TELOPT_OK(j))
			output_data("%s", TELOPT(j));
		    else if (TELCMD_OK(j))
			output_data("%s", TELCMD(j));
		    else
			output_data("%d", j);
		    output_data(", not IAC SE!) ");
		}
	    }
	    length -= 2;
	}
	if (length < 1) {
	    output_data("(Empty suboption??\?)");
	    return;
	}
	switch (pointer[0]) {
	case TELOPT_TTYPE:
	    output_data("TERMINAL-TYPE ");
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		output_data("IS \"%.*s\"", length-2, (char *)pointer+2);
		break;
	    case TELQUAL_SEND:
		output_data("SEND");
		break;
	    default:
		output_data("- unknown qualifier %d (0x%x).",
		    pointer[1], pointer[1]);
	    }
	    break;
	case TELOPT_TSPEED:
	    output_data("TERMINAL-SPEED");
	    if (length < 2) {
		output_data(" (empty suboption??\?)");
		break;
	    }
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		output_data(" IS %.*s", length-2, (char *)pointer+2);
		break;
	    default:
		if (pointer[1] == 1)
		    output_data(" SEND");
		else
		    output_data(" %d (unknown)", pointer[1]);
		for (i = 2; i < length; i++) {
		    output_data(" ?%d?", pointer[i]);
		}
		break;
	    }
	    break;

	case TELOPT_LFLOW:
	    output_data("TOGGLE-FLOW-CONTROL");
	    if (length < 2) {
		output_data(" (empty suboption??\?)");
		break;
	    }
	    switch (pointer[1]) {
	    case LFLOW_OFF:
		output_data(" OFF"); break;
	    case LFLOW_ON:
		output_data(" ON"); break;
	    case LFLOW_RESTART_ANY:
		output_data(" RESTART-ANY"); break;
	    case LFLOW_RESTART_XON:
		output_data(" RESTART-XON"); break;
	    default:
		output_data(" %d (unknown)", pointer[1]);
	    }
	    for (i = 2; i < length; i++)
		output_data(" ?%d?", pointer[i]);
	    break;

	case TELOPT_NAWS:
	    output_data("NAWS");
	    if (length < 2) {
		output_data(" (empty suboption??\?)");
		break;
	    }
	    if (length == 2) {
		output_data(" ?%d?", pointer[1]);
		break;
	    }
	    output_data(" %d %d (%d)",
		pointer[1], pointer[2],
		(int)((((unsigned int)pointer[1])<<8)|((unsigned int)pointer[2])));
	    if (length == 4) {
		output_data(" ?%d?", pointer[3]);
		break;
	    }
	    output_data(" %d %d (%d)", pointer[3], pointer[4],
		(int)((((unsigned int)pointer[3])<<8)|((unsigned int)pointer[4])));
	    for (i = 5; i < length; i++) {
		output_data(" ?%d?", pointer[i]);
	    }
	    break;

	case TELOPT_LINEMODE:
	    output_data("LINEMODE ");
	    if (length < 2) {
		output_data(" (empty suboption??\?)");
		break;
	    }
	    switch (pointer[1]) {
	    case WILL:
		output_data("WILL ");
		goto common;
	    case WONT:
		output_data("WONT ");
		goto common;
	    case DO:
		output_data("DO ");
		goto common;
	    case DONT:
		output_data("DONT ");
	    common:
		if (length < 3) {
		    output_data("(no option??\?)");
		    break;
		}
		switch (pointer[2]) {
		case LM_FORWARDMASK:
		    output_data("Forward Mask");
		    for (i = 3; i < length; i++)
			output_data(" %x", pointer[i]);
		    break;
		default:
		    output_data("%d (unknown)", pointer[2]);
		    for (i = 3; i < length; i++)
			output_data(" %d", pointer[i]);
		    break;
		}
		break;

	    case LM_SLC:
		output_data("SLC");
		for (i = 2; i < length - 2; i += 3) {
		    if (SLC_NAME_OK(pointer[i+SLC_FUNC]))
			output_data(" %s", SLC_NAME(pointer[i+SLC_FUNC]));
		    else
			output_data(" %d", pointer[i+SLC_FUNC]);
		    switch (pointer[i+SLC_FLAGS]&SLC_LEVELBITS) {
		    case SLC_NOSUPPORT:
			output_data(" NOSUPPORT"); break;
		    case SLC_CANTCHANGE:
			output_data(" CANTCHANGE"); break;
		    case SLC_VARIABLE:
			output_data(" VARIABLE"); break;
		    case SLC_DEFAULT:
			output_data(" DEFAULT"); break;
		    }
		    output_data("%s%s%s",
			pointer[i+SLC_FLAGS]&SLC_ACK ? "|ACK" : "",
			pointer[i+SLC_FLAGS]&SLC_FLUSHIN ? "|FLUSHIN" : "",
			pointer[i+SLC_FLAGS]&SLC_FLUSHOUT ? "|FLUSHOUT" : "");
		    if (pointer[i+SLC_FLAGS]& ~(SLC_ACK|SLC_FLUSHIN|
						SLC_FLUSHOUT| SLC_LEVELBITS)) {
			output_data("(0x%x)", pointer[i+SLC_FLAGS]);
		    }
		    output_data(" %d;", pointer[i+SLC_VALUE]);
		    if ((pointer[i+SLC_VALUE] == IAC) &&
			(pointer[i+SLC_VALUE+1] == IAC))
				i++;
		}
		for (; i < length; i++)
		    output_data(" ?%d?", pointer[i]);
		break;

	    case LM_MODE:
		output_data("MODE ");
		if (length < 3) {
		    output_data("(no mode??\?)");
		    break;
		}
		{
		    char tbuf[32];

		    (void)snprintf(tbuf, sizeof tbuf, "%s%s%s%s%s",
			pointer[2]&MODE_EDIT ? "|EDIT" : "",
			pointer[2]&MODE_TRAPSIG ? "|TRAPSIG" : "",
			pointer[2]&MODE_SOFT_TAB ? "|SOFT_TAB" : "",
			pointer[2]&MODE_LIT_ECHO ? "|LIT_ECHO" : "",
			pointer[2]&MODE_ACK ? "|ACK" : "");
		    output_data("%s", tbuf[1] ? &tbuf[1] : "0");
		}
		if (pointer[2]&~(MODE_EDIT|MODE_TRAPSIG|MODE_ACK))
		    output_data(" (0x%x)", pointer[2]);
		for (i = 3; i < length; i++)
		    output_data(" ?0x%x?", pointer[i]);
		break;
	    default:
		output_data("%d (unknown)", pointer[1]);
		for (i = 2; i < length; i++)
		    output_data(" %d", pointer[i]);
	    }
	    break;

	case TELOPT_STATUS: {
	    const char *cp;
	    int j, k;

	    output_data("STATUS");

	    switch (pointer[1]) {
	    default:
		if (pointer[1] == TELQUAL_SEND)
		    output_data(" SEND");
		else
		    output_data(" %d (unknown)", pointer[1]);
		for (i = 2; i < length; i++)
		    output_data(" ?%d?", pointer[i]);
		break;
	    case TELQUAL_IS:
		output_data(" IS\r\n");

		for (i = 2; i < length; i++) {
		    switch(pointer[i]) {
		    case DO:	cp = "DO"; goto common2;
		    case DONT:	cp = "DONT"; goto common2;
		    case WILL:	cp = "WILL"; goto common2;
		    case WONT:	cp = "WONT"; goto common2;
		    common2:
			i++;
			if (TELOPT_OK(pointer[i]))
			    output_data(" %s %s", cp, TELOPT(pointer[i]));
			else
			    output_data(" %s %d", cp, pointer[i]);

			output_data("\r\n");
			break;

		    case SB:
			output_data(" SB ");
			i++;
			j = k = i;
			while (j < length) {
			    if (pointer[j] == SE) {
				if (j+1 == length)
				    break;
				if (pointer[j+1] == SE)
				    j++;
				else
				    break;
			    }
			    pointer[k++] = pointer[j++];
			}
			printsub(0, &pointer[i], k - i);
			if (i < length) {
			    output_data(" SE");
			    i = j;
			} else
			    i = j - 1;

			output_data("\r\n");

			break;

		    default:
			output_data(" %d", pointer[i]);
			break;
		    }
		}
		break;
	    }
	    break;
	  }

	case TELOPT_XDISPLOC:
	    output_data("X-DISPLAY-LOCATION ");
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		output_data("IS \"%.*s\"", length - 2, (char *)pointer + 2);
		break;
	    case TELQUAL_SEND:
		output_data("SEND");
		break;
	    default:
		output_data("- unknown qualifier %d (0x%x).",
		    pointer[1], pointer[1]);
	    }
	    break;

	case TELOPT_NEW_ENVIRON:
	    output_data("NEW-ENVIRON ");
	    goto env_common1;
	case TELOPT_OLD_ENVIRON:
	    output_data("OLD-ENVIRON");
	env_common1:
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		output_data("IS ");
		goto env_common;
	    case TELQUAL_SEND:
		output_data("SEND ");
		goto env_common;
	    case TELQUAL_INFO:
		output_data("INFO ");
	    env_common:
		{
		    static const char NQ[] = "\" ";
		    const char *noquote = NQ;
		    for (i = 2; i < length; i++ ) {
			switch (pointer[i]) {
			case NEW_ENV_VAR:
			    output_data("%sVAR ", noquote);
			    noquote = NQ;
			    break;

			case NEW_ENV_VALUE:
			    output_data("%sVALUE ", noquote);
			    noquote = NQ;
			    break;

			case ENV_ESC:
			    output_data("%sESC ", noquote);
			    noquote = NQ;
			    break;

			case ENV_USERVAR:
			    output_data("%sUSERVAR ", noquote);
			    noquote = NQ;
			    break;

			default:
			    if (isprint(pointer[i]) && pointer[i] != '"') {
				if (*noquote) {
				    output_data("\"");
				    noquote = "";
				}
				output_data("%c", pointer[i]);
			    } else {
				output_data("%s%03o ", noquote, pointer[i]);
				noquote = NQ;
			    }
			    break;
			}
		    }
		    if (!noquote)
			output_data("\"");
		    break;
		}
	    }
	    break;

#ifdef AUTHENTICATION
	case TELOPT_AUTHENTICATION:
	    output_data("AUTHENTICATION");

	    if (length < 2) {
		output_data(" (empty suboption??\?)");
		break;
	    }
	    switch (pointer[1]) {
	    case TELQUAL_REPLY:
	    case TELQUAL_IS:
		output_data(" %s ", (pointer[1] == TELQUAL_IS) ?
		    "IS" : "REPLY");
		if (AUTHTYPE_NAME_OK(pointer[2]))
		    output_data("%s ", AUTHTYPE_NAME(pointer[2]));
		else
		    output_data("%d ", pointer[2]);
		if (length < 3) {
		    output_data("(partial suboption??\?)");
		    break;
		}
		output_data("%s|%s",
			((pointer[3] & AUTH_WHO_MASK) == AUTH_WHO_CLIENT) ?
			"CLIENT" : "SERVER",
			((pointer[3] & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) ?
			"MUTUAL" : "ONE-WAY");

		auth_printsub(&pointer[1], length - 1, buf, sizeof(buf));
		output_data("%s", buf);
		break;

	    case TELQUAL_SEND:
		i = 2;
		output_data(" SEND ");
		while (i < length) {
		    if (AUTHTYPE_NAME_OK(pointer[i]))
			output_data("%s ", AUTHTYPE_NAME(pointer[i]));
		    else
			output_data("%d ", pointer[i]);
		    if (++i >= length) {
			output_data("(partial suboption??\?)");
			break;
		    }
		    output_data("%s|%s ",
			((pointer[i] & AUTH_WHO_MASK) == AUTH_WHO_CLIENT) ?
							"CLIENT" : "SERVER",
			((pointer[i] & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) ?
							"MUTUAL" : "ONE-WAY");
		    ++i;
		}
		break;

	    case TELQUAL_NAME:
		i = 2;
		output_data(" NAME \"");
		while (i < length) {
		    if (isprint(pointer[i]))
			output_data("%c", pointer[i++]);
		    else
			output_data("\"%03o\"",pointer[i++]);
		}
		output_data("\"");
		break;

	    default:
		for (i = 2; i < length; i++)
		    output_data(" ?%d?", pointer[i]);
		break;
	    }
	    break;
#endif

#ifdef	ENCRYPTION
	case TELOPT_ENCRYPT:
	    output_data("ENCRYPT");
	    if (length < 2) {
		output_data(" (empty suboption??\?)");
		break;
	    }
	    switch (pointer[1]) {
	    case ENCRYPT_START:
		output_data(" START");
		break;

	    case ENCRYPT_END:
		output_data(" END");
		break;

	    case ENCRYPT_REQSTART:
		output_data(" REQUEST-START");
		break;

	    case ENCRYPT_REQEND:
		output_data(" REQUEST-END");
		break;

	    case ENCRYPT_IS:
	    case ENCRYPT_REPLY:
		output_data(" %s ", (pointer[1] == ENCRYPT_IS) ?
		    "IS" : "REPLY");
		if (length < 3) {
			output_data(" (partial suboption??\?)");
			break;
		}
		if (ENCTYPE_NAME_OK(pointer[2]))
			output_data("%s ", ENCTYPE_NAME(pointer[2]));
		else
			output_data(" %d (unknown)", pointer[2]);

		encrypt_printsub(&pointer[1], length - 1, buf, sizeof(buf));
		output_data("%s", buf);
		break;

	    case ENCRYPT_SUPPORT:
		i = 2;
		output_data(" SUPPORT ");
		while (i < length) {
			if (ENCTYPE_NAME_OK(pointer[i]))
				output_data("%s ", ENCTYPE_NAME(pointer[i]));
			else
				output_data("%d ", pointer[i]);
			i++;
		}
		break;

	    case ENCRYPT_ENC_KEYID:
		output_data(" ENC_KEYID");
		goto encommon;

	    case ENCRYPT_DEC_KEYID:
		output_data(" DEC_KEYID");
		goto encommon;

	    default:
		output_data(" %d (unknown)", pointer[1]);
	    encommon:
		for (i = 2; i < length; i++)
			output_data(" %d", pointer[i]);
		break;
	    }
	    break;
#endif	/* ENCRYPTION */

	default:
	    if (TELOPT_OK(pointer[0]))
		output_data("%s (unknown)", TELOPT(pointer[0]));
	    else
		output_data("%d (unknown)", pointer[i]);
	    for (i = 1; i < length; i++)
		output_data(" %d", pointer[i]);
	    break;
	}
	output_data("\r\n");
}

/*
 * Dump a data buffer in hex and ascii to the output data stream.
 */
void
printdata(const char *tag, char *ptr, int cnt)
{
	int i;
	char xbuf[30];

	while (cnt) {
		/* flush net output buffer if no room for new data) */
		if ((&netobuf[BUFSIZ] - nfrontp) < 80) {
			netflush();
		}

		/* add a line of output */
		output_data("%s: ", tag);
		for (i = 0; i < 20 && cnt; i++) {
			output_data("%02x", *ptr);
			if (isprint((unsigned char)*ptr)) {
				xbuf[i] = *ptr;
			} else {
				xbuf[i] = '.';
			}
			if (i % 2)
				output_data(" ");
			cnt--;
			ptr++;
		}
		xbuf[i] = '\0';
		output_data(" %s\r\n", xbuf);
	}
}
#endif /* DIAGNOSTICS */
