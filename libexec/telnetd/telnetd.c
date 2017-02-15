/*	$NetBSD: telnetd.c,v 1.55 2014/02/27 18:20:21 joerg Exp $	*/

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "@(#)telnetd.c	8.4 (Berkeley) 5/30/95";
#else
__RCSID("$NetBSD: telnetd.c,v 1.55 2014/02/27 18:20:21 joerg Exp $");
#endif
#endif /* not lint */

#include "telnetd.h"
#include "pathnames.h"

#include <arpa/inet.h>

#include <err.h>
#include <termcap.h>

#include <limits.h>

#ifdef KRB5
#define	Authenticator	k5_Authenticator
#include <krb5.h>
#undef	Authenticator
#include <krb5/com_err.h>
#endif

#ifdef AUTHENTICATION
int	auth_level = 0;
#endif

#if	defined(AUTHENTICATION) || defined(ENCRYPTION)
#include <libtelnet/misc.h>
#endif

#ifdef SECURELOGIN
int	require_secure_login = 0;
#endif

extern int require_hwpreauth;
#ifdef KRB5
extern krb5_context telnet_context;
#endif
int	registerd_host_only = 0;


/*
 * I/O data buffers,
 * pointers, and counters.
 */
char	ptyibuf[BUFSIZ], *ptyip = ptyibuf;
char	ptyibuf2[BUFSIZ];


int	hostinfo = 1;			/* do we print login banner? */


static int debug = 0;
int keepalive = 1;
const char *gettyname = "default";
char *progname;

void usage(void) __dead;
int getterminaltype(char *, size_t);
int getent(char *, const char *);
static void doit(struct sockaddr *) __dead;
void _gettermname(void);
int terminaltypeok(char *);
char *getstr(const char *, char **);

/*
 * The string to pass to getopt().  We do it this way so
 * that only the actual options that we support will be
 * passed off to getopt().
 */
char valid_opts[] = {
	'd', ':', 'g', ':', 'h', 'k', 'n', 'S', ':', 'u', ':', 'U',
	'4', '6',
#ifdef	AUTHENTICATION
	'a', ':', 'X', ':',
#endif
#ifdef	ENCRYPTION
	'e', ':',
#endif
#ifdef DIAGNOSTICS
	'D', ':',
#endif
#ifdef	LINEMODE
	'l',
#endif
#ifdef	SECURELOGIN
	's',
#endif
#ifdef	KRB5
	'R', ':', 'H',
#endif
	'\0'
};

int family = AF_INET;
struct sockaddr_storage from;

int
main(int argc, char *argv[])
{
	socklen_t fromlen;
	int on = 1;
	int ch;
#if	defined(IPPROTO_IP) && defined(IP_TOS)
	int tos = -1;
#endif

	pfrontp = pbackp = ptyobuf;
	netip = netibuf;
	nfrontp = nbackp = netobuf;
#ifdef	ENCRYPTION
	nclearto = 0;
#endif	/* ENCRYPTION */

	progname = *argv;


	while ((ch = getopt(argc, argv, valid_opts)) != -1) {
		switch (ch) {

#ifdef	AUTHENTICATION
		case 'a':
			/*
			 * Check for required authentication level
			 */
			if (strcmp(optarg, "debug") == 0) {
				auth_debug_mode = 1;
			} else if (strcasecmp(optarg, "none") == 0) {
				auth_level = 0;
			} else if (strcasecmp(optarg, "other") == 0) {
				auth_level = AUTH_OTHER;
			} else if (strcasecmp(optarg, "user") == 0) {
				auth_level = AUTH_USER;
			} else if (strcasecmp(optarg, "valid") == 0) {
				auth_level = AUTH_VALID;
			} else if (strcasecmp(optarg, "off") == 0) {
				/*
				 * This hack turns off authentication
				 */
				auth_level = -1;
			} else {
				fprintf(stderr,
			    "telnetd: unknown authorization level for -a\n");
			}
			break;
#endif	/* AUTHENTICATION */


		case 'd':
			if (strcmp(optarg, "ebug") == 0) {
				debug++;
				break;
			}
			usage();
			/* NOTREACHED */
			break;

#ifdef DIAGNOSTICS
		case 'D':
			/*
			 * Check for desired diagnostics capabilities.
			 */
			if (!strcmp(optarg, "report")) {
				diagnostic |= TD_REPORT|TD_OPTIONS;
			} else if (!strcmp(optarg, "exercise")) {
				diagnostic |= TD_EXERCISE;
			} else if (!strcmp(optarg, "netdata")) {
				diagnostic |= TD_NETDATA;
			} else if (!strcmp(optarg, "ptydata")) {
				diagnostic |= TD_PTYDATA;
			} else if (!strcmp(optarg, "options")) {
				diagnostic |= TD_OPTIONS;
			} else {
				usage();
				/* NOT REACHED */
			}
			break;
#endif /* DIAGNOSTICS */

#ifdef	ENCRYPTION
		case 'e':
			if (strcmp(optarg, "debug") == 0) {
				encrypt_debug_mode = 1;
				break;
			}
			usage();
			/* NOTREACHED */
			break;
#endif	/* ENCRYPTION */

		case 'g':
			gettyname = optarg;
			break;

		case 'h':
			hostinfo = 0;
			break;

#ifdef	KRB5
		case 'H':
		    {
			require_hwpreauth = 1;
			break;
		    }
#endif	/* KRB5 */


#ifdef	LINEMODE
		case 'l':
			alwayslinemode = 1;
			break;
#endif	/* LINEMODE */

		case 'k':
#if	defined(LINEMODE) && defined(KLUDGELINEMODE)
			lmodetype = NO_AUTOKLUDGE;
#else
			/* ignore -k option if built without kludge linemode */
#endif	/* defined(LINEMODE) && defined(KLUDGELINEMODE) */
			break;

		case 'n':
			keepalive = 0;
			break;


#ifdef	KRB5
		case 'R':
		    {
			krb5_error_code retval;

			if (telnet_context == 0) {
				retval = krb5_init_context(&telnet_context);
				if (retval) {
					com_err("telnetd", retval,
					    "while initializing krb5");
					exit(1);
				}
			}
			krb5_set_default_realm(telnet_context, optarg);
			break;
		    }
#endif	/* KRB5 */

#ifdef	SECURELOGIN
		case 's':
			/* Secure login required */
			require_secure_login = 1;
			break;
#endif	/* SECURELOGIN */
		case 'S':
			fprintf(stderr, "%s%s\n", "TOS option unavailable; ",
						"-S flag not supported\n");
			break;

		case 'u':
			fprintf(stderr, "telnetd: -u option unneeded\n");
			break;

		case 'U':
			registerd_host_only = 1;
			break;

#ifdef	AUTHENTICATION
		case 'X':
			/*
			 * Check for invalid authentication types
			 */
			auth_disable_name(optarg);
			break;
#endif	/* AUTHENTICATION */

		case '4':
			family = AF_INET;
			break;

		case '6':
			family = AF_INET6;
			break;

		default:
			fprintf(stderr, "telnetd: %c: unknown option\n", ch);
			/* FALLTHROUGH */
		case '?':
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (debug) {
	    int s, ns, error;
	    socklen_t foo;
	    const char *service = "telnet";
	    struct addrinfo hints, *res;

	    if (argc > 1) {
		usage();
		/* NOT REACHED */
	    } else if (argc == 1)
		service = *argv;

	    memset(&hints, 0, sizeof(hints));
	    hints.ai_flags = AI_PASSIVE;
	    hints.ai_family = family;
	    hints.ai_socktype = SOCK_STREAM;
	    hints.ai_protocol = 0;
	    error = getaddrinfo(NULL, service, &hints, &res);

	    if (error) {
		fprintf(stderr, "tcp/%s: %s\n", service, gai_strerror(error));
		exit(1);
	    }

	    s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	    if (s < 0) {
		perror("telnetd: socket");
		exit(1);
	    }
	    (void) setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
				(char *)&on, sizeof(on));
	    if (bind(s, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		exit(1);
	    }
	    if (listen(s, 1) < 0) {
		perror("listen");
		exit(1);
	    }
	    foo = res->ai_addrlen;
	    ns = accept(s, res->ai_addr, &foo);
	    if (ns < 0) {
		perror("accept");
		exit(1);
	    }
	    (void) dup2(ns, 0);
	    (void) close(ns);
	    (void) close(s);
	} else if (argc > 0) {
		usage();
		/* NOT REACHED */
	}

	openlog("telnetd", LOG_PID, LOG_DAEMON);
	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror("getpeername");
		_exit(1);
	}
	if (keepalive &&
	    setsockopt(0, SOL_SOCKET, SO_KEEPALIVE,
			(char *)&on, sizeof (on)) < 0) {
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	}

#if	defined(IPPROTO_IP) && defined(IP_TOS)
	if (((struct sockaddr *)&from)->sa_family == AF_INET) {
		if (tos < 0)
			tos = 020;	/* Low Delay bit */
		if (tos
		   && (setsockopt(0, IPPROTO_IP, IP_TOS,
				  (char *)&tos, sizeof(tos)) < 0)
		   && (errno != ENOPROTOOPT) )
			syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
	}
#endif	/* defined(IPPROTO_IP) && defined(IP_TOS) */

	net = 0;
	doit((struct sockaddr *)&from);
	/* NOTREACHED */
#ifdef __GNUC__
	exit(0);
#endif
}  /* end of main */

void
usage(void)
{
	fprintf(stderr, "Usage: telnetd");
#ifdef	AUTHENTICATION
	fprintf(stderr, " [-a (debug|other|user|valid|off|none)]\n\t");
#endif
	fprintf(stderr, " [-debug]");
#ifdef DIAGNOSTICS
	fprintf(stderr, " [-D (options|report|exercise|netdata|ptydata)]\n\t");
#endif
#ifdef	ENCRYPTION
	fprintf(stderr, " [-edebug]");
#endif
	fprintf(stderr, " [-h]");
#if	defined(LINEMODE) && defined(KLUDGELINEMODE)
	fprintf(stderr, " [-k]");
#endif
#ifdef LINEMODE
	fprintf(stderr, " [-l]");
#endif
	fprintf(stderr, " [-n]");
	fprintf(stderr, "\n\t");
#ifdef	SECURELOGIN
	fprintf(stderr, " [-s]");
#endif
#ifdef	AUTHENTICATION
	fprintf(stderr, " [-X auth-type]");
#endif
	fprintf(stderr, " [-u utmp_hostname_length] [-U]");
	fprintf(stderr, " [port]\n");
	exit(1);
}

/*
 * getterminaltype
 *
 *	Ask the other end to send along its terminal type and speed.
 * Output is the variable terminaltype filled in.
 */
static unsigned char ttytype_sbbuf[] = {
	IAC, SB, TELOPT_TTYPE, TELQUAL_SEND, IAC, SE
};

int
getterminaltype(char *name, size_t l)
{
    int retval = -1;

    settimer(baseline);
#ifdef AUTHENTICATION
    /*
     * Handle the Authentication option before we do anything else.
     */
    send_do(TELOPT_AUTHENTICATION, 1);
    while (his_will_wont_is_changing(TELOPT_AUTHENTICATION))
	ttloop();
    if (his_state_is_will(TELOPT_AUTHENTICATION)) {
	retval = auth_wait(name, l);
    }
#endif

#ifdef	ENCRYPTION
    send_will(TELOPT_ENCRYPT, 1);
#endif	/* ENCRYPTION */
    send_do(TELOPT_TTYPE, 1);
    send_do(TELOPT_TSPEED, 1);
    send_do(TELOPT_XDISPLOC, 1);
    send_do(TELOPT_NEW_ENVIRON, 1);
    send_do(TELOPT_OLD_ENVIRON, 1);
    while (
#ifdef	ENCRYPTION
	   his_do_dont_is_changing(TELOPT_ENCRYPT) ||
#endif	/* ENCRYPTION */
	   his_will_wont_is_changing(TELOPT_TTYPE) ||
	   his_will_wont_is_changing(TELOPT_TSPEED) ||
	   his_will_wont_is_changing(TELOPT_XDISPLOC) ||
	   his_will_wont_is_changing(TELOPT_NEW_ENVIRON) ||
	   his_will_wont_is_changing(TELOPT_OLD_ENVIRON)) {
	ttloop();
    }
    if (his_state_is_will(TELOPT_TSPEED)) {
	static unsigned char sb[] =
			{ IAC, SB, TELOPT_TSPEED, TELQUAL_SEND, IAC, SE };

	output_datalen((const char *)sb, sizeof sb);
	DIAG(TD_OPTIONS, printsub('>', sb + 2, sizeof sb - 2););
    }
#ifdef	ENCRYPTION
    /*
     * Wait for the negotiation of what type of encryption we can
     * send with.  If autoencrypt is not set, this will just return.
     */
    if (his_state_is_will(TELOPT_ENCRYPT)) {
	encrypt_wait();
    }
#endif	/* ENCRYPTION */
    if (his_state_is_will(TELOPT_XDISPLOC)) {
	static unsigned char sb[] =
			{ IAC, SB, TELOPT_XDISPLOC, TELQUAL_SEND, IAC, SE };

	output_datalen((const char *)sb, sizeof sb);
	DIAG(TD_OPTIONS, printsub('>', sb + 2, sizeof sb - 2););
    }
    if (his_state_is_will(TELOPT_NEW_ENVIRON)) {
	static unsigned char sb[] =
			{ IAC, SB, TELOPT_NEW_ENVIRON, TELQUAL_SEND, IAC, SE };

	output_datalen((const char *)sb, sizeof sb);
	DIAG(TD_OPTIONS, printsub('>', sb + 2, sizeof sb - 2););
    }
    else if (his_state_is_will(TELOPT_OLD_ENVIRON)) {
	static unsigned char sb[] =
			{ IAC, SB, TELOPT_OLD_ENVIRON, TELQUAL_SEND, IAC, SE };

	output_datalen((const char *)sb, sizeof sb);
	DIAG(TD_OPTIONS, printsub('>', sb + 2, sizeof sb - 2););
    }
    if (his_state_is_will(TELOPT_TTYPE)) {

	output_datalen((const char *)ttytype_sbbuf, sizeof ttytype_sbbuf);
	DIAG(TD_OPTIONS, printsub('>', ttytype_sbbuf + 2,
					sizeof ttytype_sbbuf - 2););
    }
    if (his_state_is_will(TELOPT_TSPEED)) {
	while (sequenceIs(tspeedsubopt, baseline))
	    ttloop();
    }
    if (his_state_is_will(TELOPT_XDISPLOC)) {
	while (sequenceIs(xdisplocsubopt, baseline))
	    ttloop();
    }
    if (his_state_is_will(TELOPT_NEW_ENVIRON)) {
	while (sequenceIs(environsubopt, baseline))
	    ttloop();
    }
    if (his_state_is_will(TELOPT_OLD_ENVIRON)) {
	while (sequenceIs(oenvironsubopt, baseline))
	    ttloop();
    }
    if (his_state_is_will(TELOPT_TTYPE)) {
	char first[256], last[256];

	while (sequenceIs(ttypesubopt, baseline))
	    ttloop();

	/*
	 * If the other side has already disabled the option, then
	 * we have to just go with what we (might) have already gotten.
	 */
	if (his_state_is_will(TELOPT_TTYPE) && !terminaltypeok(terminaltype)) {
	    (void) strlcpy(first, terminaltype, sizeof(first));
	    for(;;) {
		/*
		 * Save the unknown name, and request the next name.
		 */
		(void) strlcpy(last, terminaltype, sizeof(last));
		_gettermname();
		if (terminaltypeok(terminaltype))
		    break;
		if ((strncmp(last, terminaltype, sizeof(last)) == 0) ||
		    his_state_is_wont(TELOPT_TTYPE)) {
		    /*
		     * We've hit the end.  If this is the same as
		     * the first name, just go with it.
		     */
		    if (strncmp(first, terminaltype, sizeof(first)) == 0)
			break;
		    /*
		     * Get the terminal name one more time, so that
		     * RFC1091 compliant telnets will cycle back to
		     * the start of the list.
		     */
		     _gettermname();
		    if (strncmp(first, terminaltype, sizeof(first)) != 0) {
			(void) strlcpy(terminaltype, first, sizeof(terminaltype));
		    }
		    break;
		}
	    }
	}
    }
    return(retval);
}  /* end of getterminaltype */

void
_gettermname(void)
{
    /*
     * If the client turned off the option,
     * we can't send another request, so we
     * just return.
     */
    if (his_state_is_wont(TELOPT_TTYPE))
	return;
    settimer(baseline);
    output_datalen((const char *)ttytype_sbbuf, sizeof ttytype_sbbuf);
    DIAG(TD_OPTIONS, printsub('>', ttytype_sbbuf + 2,
					sizeof ttytype_sbbuf - 2););
    while (sequenceIs(ttypesubopt, baseline))
	ttloop();
}

int
terminaltypeok(char *s)
{
    char buf[1024];

    /*
     * tgetent() will return 1 if the type is known, and
     * 0 if it is not known.  If it returns -1, it couldn't
     * open the database.  But if we can't open the database,
     * it won't help to say we failed, because we won't be
     * able to verify anything else.  So, we treat -1 like 1.
     */
    if (tgetent(buf, s) == 0)
	return(0);
    return(1);
}

char *hostname;
char host_name[MAXHOSTNAMELEN + 1];
char remote_host_name[MAXHOSTNAMELEN + 1];

static void telnet(int, int) __dead;

/*
 * Get a pty, scan input lines.
 */
static void
doit(struct sockaddr *who)
{
	char *host;
	int error;
	int level;
	int ptynum;
	int flags;
	char user_name[256];

	/*
	 * Find an available pty to use.
	 */
	pty = getpty(&ptynum);
	if (pty < 0)
		fatal(net, "All network ports in use");

	flags = registerd_host_only ? NI_NAMEREQD : 0;

	/* get name of connected client */
	error = getnameinfo(who, who->sa_len, remote_host_name,
	    sizeof(remote_host_name), NULL, 0, flags);

	if (error) {
		fatal(net, "Couldn't resolve your address into a host name.\r\n\
	 Please contact your net administrator");
#ifdef __GNUC__
		host = NULL;	/* XXX gcc */
#endif
	}

	remote_host_name[sizeof(remote_host_name)-1] = 0;
	host = remote_host_name;

	(void)gethostname(host_name, sizeof(host_name));
	host_name[sizeof(host_name) - 1] = '\0';
	hostname = host_name;

#if	defined(AUTHENTICATION) || defined(ENCRYPTION)
	auth_encrypt_init(hostname, host, "TELNETD", 1);
#endif

	init_env();
	/*
	 * get terminal type.
	 */
	*user_name = 0;
	level = getterminaltype(user_name, sizeof(user_name));
	setenv("TERM", terminaltype[0] ? terminaltype : "network", 1);

	/*
	 * Start up the login process on the slave side of the terminal
	 */
	startslave(host, level, user_name);

	telnet(net, pty);  /* begin server processing */
	/*NOTREACHED*/
}  /* end of doit */


/*
 * Main loop.  Select from pty and network, and
 * hand data to telnet receiver finite state machine.
 */
static void
telnet(int f, int p)
{
	int on = 1;
#define	TABBUFSIZ	512
	char	defent[TABBUFSIZ];
	char	defstrs[TABBUFSIZ];
#undef	TABBUFSIZ
	char *HE, *HN, *IF, *ptyibuf2ptr;
	const char *IM;
	struct pollfd set[2];

	/*
	 * Initialize the slc mapping table.
	 */
	get_slc_defaults();

	/*
	 * Do some tests where it is desireable to wait for a response.
	 * Rather than doing them slowly, one at a time, do them all
	 * at once.
	 */
	if (my_state_is_wont(TELOPT_SGA))
		send_will(TELOPT_SGA, 1);
	/*
	 * Is the client side a 4.2 (NOT 4.3) system?  We need to know this
	 * because 4.2 clients are unable to deal with TCP urgent data.
	 *
	 * To find out, we send out a "DO ECHO".  If the remote system
	 * answers "WILL ECHO" it is probably a 4.2 client, and we note
	 * that fact ("WILL ECHO" ==> that the client will echo what
	 * WE, the server, sends it; it does NOT mean that the client will
	 * echo the terminal input).
	 */
	send_do(TELOPT_ECHO, 1);

#ifdef	LINEMODE
	if (his_state_is_wont(TELOPT_LINEMODE)) {
		/* Query the peer for linemode support by trying to negotiate
		 * the linemode option.
		 */
		linemode = 0;
		editmode = 0;
		send_do(TELOPT_LINEMODE, 1);  /* send do linemode */
	}
#endif	/* LINEMODE */

	/*
	 * Send along a couple of other options that we wish to negotiate.
	 */
	send_do(TELOPT_NAWS, 1);
	send_will(TELOPT_STATUS, 1);
	flowmode = 1;		/* default flow control state */
	restartany = -1;	/* uninitialized... */
	send_do(TELOPT_LFLOW, 1);

	/*
	 * Spin, waiting for a response from the DO ECHO.  However,
	 * some REALLY DUMB telnets out there might not respond
	 * to the DO ECHO.  So, we spin looking for NAWS, (most dumb
	 * telnets so far seem to respond with WONT for a DO that
	 * they don't understand...) because by the time we get the
	 * response, it will already have processed the DO ECHO.
	 * Kludge upon kludge.
	 */
	while (his_will_wont_is_changing(TELOPT_NAWS))
		ttloop();

	/*
	 * But...
	 * The client might have sent a WILL NAWS as part of its
	 * startup code; if so, we'll be here before we get the
	 * response to the DO ECHO.  We'll make the assumption
	 * that any implementation that understands about NAWS
	 * is a modern enough implementation that it will respond
	 * to our DO ECHO request; hence we'll do another spin
	 * waiting for the ECHO option to settle down, which is
	 * what we wanted to do in the first place...
	 */
	if (his_want_state_is_will(TELOPT_ECHO) &&
	    his_state_is_will(TELOPT_NAWS)) {
		while (his_will_wont_is_changing(TELOPT_ECHO))
			ttloop();
	}
	/*
	 * On the off chance that the telnet client is broken and does not
	 * respond to the DO ECHO we sent, (after all, we did send the
	 * DO NAWS negotiation after the DO ECHO, and we won't get here
	 * until a response to the DO NAWS comes back) simulate the
	 * receipt of a will echo.  This will also send a WONT ECHO
	 * to the client, since we assume that the client failed to
	 * respond because it believes that it is already in DO ECHO
	 * mode, which we do not want.
	 */
	if (his_want_state_is_will(TELOPT_ECHO)) {
		DIAG(TD_OPTIONS,
			{output_data("td: simulating recv\r\n");});
		willoption(TELOPT_ECHO);
	}

	/*
	 * Finally, to clean things up, we turn on our echo.  This
	 * will break stupid 4.2 telnets out of local terminal echo.
	 */

	if (my_state_is_wont(TELOPT_ECHO))
		send_will(TELOPT_ECHO, 1);

	/*
	 * Turn on packet mode
	 */
	(void) ioctl(p, TIOCPKT, (char *)&on);

#if	defined(LINEMODE) && defined(KLUDGELINEMODE)
	/*
	 * Continuing line mode support.  If client does not support
	 * real linemode, attempt to negotiate kludge linemode by sending
	 * the do timing mark sequence.
	 */
	if (lmodetype < REAL_LINEMODE)
		send_do(TELOPT_TM, 1);
#endif	/* defined(LINEMODE) && defined(KLUDGELINEMODE) */

	/*
	 * Call telrcv() once to pick up anything received during
	 * terminal type negotiation, 4.2/4.3 determination, and
	 * linemode negotiation.
	 */
	telrcv();

	(void) ioctl(f, FIONBIO, (char *)&on);
	(void) ioctl(p, FIONBIO, (char *)&on);

	(void) setsockopt(f, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof on);

	(void) signal(SIGTSTP, SIG_IGN);
	/*
	 * Ignoring SIGTTOU keeps the kernel from blocking us
	 * in ttioct() in /sys/tty.c.
	 */
	(void) signal(SIGTTOU, SIG_IGN);

	(void) signal(SIGCHLD, cleanup);


	{
		int t;
		t = open(_PATH_TTY, O_RDWR);
		if (t >= 0) {
			(void) ioctl(t, TIOCNOTTY, (char *)0);
			(void) close(t);
		}
	}


	/*
	 * Show banner that getty never gave.
	 *
	 * We put the banner in the pty input buffer.  This way, it
	 * gets carriage return null processing, etc., just like all
	 * other pty --> client data.
	 */

	if (getent(defent, gettyname) == 1) {
		char *cp=defstrs;

		HE = getstr("he", &cp);
		HN = getstr("hn", &cp);
		IM = getstr("im", &cp);
		IF = getstr("if", &cp);
		if (HN && *HN)
			(void)strlcpy(host_name, HN, sizeof(host_name));
		if (IM == 0)
			IM = "";
	} else {
		IM = DEFAULT_IM;
		HE = 0;
		IF = NULL;
	}
	edithost(HE, host_name);
	ptyibuf2ptr = ptyibuf2;
	if (hostinfo) {
		if (IF)	{
			char buf[_POSIX2_LINE_MAX];
			FILE *fd;

			if ((fd = fopen(IF, "r")) != NULL) {
				while (fgets(buf, sizeof(buf) - 1, fd) != NULL)
					ptyibuf2ptr = putf(buf, ptyibuf2ptr);
				fclose(fd);
			}
		}
		if (*IM)
			ptyibuf2ptr = putf(IM, ptyibuf2ptr);
	}

	if (pcc)
		strncpy(ptyibuf2ptr, ptyip, pcc+1);
	ptyip = ptyibuf2;
	pcc = strlen(ptyip);
#ifdef	LINEMODE
	/*
	 * Last check to make sure all our states are correct.
	 */
	init_termbuf();
	localstat();
#endif	/* LINEMODE */

	DIAG(TD_REPORT,
		{output_data("td: Entering processing loop\r\n");});


	set[0].fd = f;
	set[1].fd = p;
	for (;;) {
		int c;

		if (ncc < 0 && pcc < 0)
			break;

		/*
		 * Never look for input if there's still
		 * stuff in the corresponding output buffer
		 */
		set[0].events = 0;
		set[1].events = 0;
		if (nfrontp - nbackp || pcc > 0)
			set[0].events |= POLLOUT;
		else
			set[1].events |= POLLIN;
		if (pfrontp - pbackp || ncc > 0)
			set[1].events |= POLLOUT;
		else
			set[0].events |= POLLIN;
		if (!SYNCHing)
			set[0].events |= POLLPRI;

		if ((c = poll(set, 2, INFTIM)) < 1) {
			if (c == -1) {
				if (errno == EINTR) {
					continue;
				}
			}
			sleep(5);
			continue;
		}

		/*
		 * Any urgent data?
		 */
		if (set[0].revents & POLLPRI) {
		    SYNCHing = 1;
		}

		/*
		 * Something to read from the network...
		 */
		if (set[0].revents & POLLIN) {
		    ncc = read(f, netibuf, sizeof (netibuf));
		    if (ncc < 0 && errno == EWOULDBLOCK)
			ncc = 0;
		    else {
			if (ncc <= 0) {
			    break;
			}
			netip = netibuf;
		    }
		    DIAG((TD_REPORT | TD_NETDATA),
			    {output_data("td: netread %d chars\r\n", ncc);});
		    DIAG(TD_NETDATA, printdata("nd", netip, ncc));
		}

		/*
		 * Something to read from the pty...
		 */
		if (set[1].revents & POLLIN) {
			pcc = read(p, ptyibuf, BUFSIZ);
			/*
			 * On some systems, if we try to read something
			 * off the master side before the slave side is
			 * opened, we get EIO.
			 */
			if (pcc < 0 && (errno == EWOULDBLOCK ||
					errno == EAGAIN ||
					errno == EIO)) {
				pcc = 0;
			} else {
				if (pcc <= 0)
					break;
#ifdef	LINEMODE
				/*
				 * If ioctl from pty, pass it through net
				 */
				if (ptyibuf[0] & TIOCPKT_IOCTL) {
					copy_termbuf(ptyibuf+1, pcc-1);
					localstat();
					pcc = 1;
				}
#endif	/* LINEMODE */
				if (ptyibuf[0] & TIOCPKT_FLUSHWRITE) {
					netclear();	/* clear buffer back */
					/*
					 * There are client telnets on some
					 * operating systems get screwed up
					 * royally if we send them urgent
					 * mode data.
					 */
					output_data("%c%c", IAC, DM);
					neturg = nfrontp - 1; /* off by one XXX */
					DIAG(TD_OPTIONS,
					    printoption("td: send IAC", DM));
				}
				if (his_state_is_will(TELOPT_LFLOW) &&
				    (ptyibuf[0] &
				     (TIOCPKT_NOSTOP|TIOCPKT_DOSTOP))) {
					int newflow =
					    ptyibuf[0] & TIOCPKT_DOSTOP ? 1 : 0;
					if (newflow != flowmode) {
						flowmode = newflow;
						(void) output_data(
							"%c%c%c%c%c%c",
							IAC, SB, TELOPT_LFLOW,
							flowmode ? LFLOW_ON
								 : LFLOW_OFF,
							IAC, SE);
						DIAG(TD_OPTIONS, printsub('>',
						    (unsigned char *)nfrontp - 4,
						    4););
					}
				}
				pcc--;
				ptyip = ptyibuf+1;
			}
		}

		while (pcc > 0) {
			if ((&netobuf[BUFSIZ] - nfrontp) < 2)
				break;
			c = *ptyip++ & 0377, pcc--;
			if (c == IAC)
				output_data("%c", c);
			output_data("%c", c);
			if ((c == '\r') && (my_state_is_wont(TELOPT_BINARY))) {
				if (pcc > 0 && ((*ptyip & 0377) == '\n')) {
					output_data("%c", *ptyip++ & 0377);
					pcc--;
				} else
					output_datalen("\0", 1);
			}
		}

		if ((set[0].revents & POLLOUT) && (nfrontp - nbackp) > 0)
			netflush();
		if (ncc > 0)
			telrcv();
		if ((set[1].revents & POLLOUT) && (pfrontp - pbackp) > 0)
			ptyflush();
	}
	cleanup(0);
}  /* end of telnet */

/*
 * Send interrupt to process on other side of pty.
 * If it is in raw mode, just write NULL;
 * otherwise, write intr char.
 */
void
interrupt(void)
{
	ptyflush();	/* half-hearted */

	(void) ioctl(pty, TIOCSIG, (char *)SIGINT);
}

/*
 * Send quit to process on other side of pty.
 * If it is in raw mode, just write NULL;
 * otherwise, write quit char.
 */
void
sendbrk(void)
{
	ptyflush();	/* half-hearted */
	(void) ioctl(pty, TIOCSIG, (char *)SIGQUIT);
}

void
sendsusp(void)
{
	ptyflush();	/* half-hearted */
	(void) ioctl(pty, TIOCSIG, (char *)SIGTSTP);
}

/*
 * When we get an AYT, if ^T is enabled, use that.  Otherwise,
 * just send back "[Yes]".
 */
void
recv_ayt(void)
{
	if (slctab[SLC_AYT].sptr && *slctab[SLC_AYT].sptr != _POSIX_VDISABLE) {
		(void) ioctl(pty, TIOCSIG, (char *)SIGINFO);
		return;
	}
	(void) output_data("\r\n[Yes]\r\n");
}

void
doeof(void)
{
	init_termbuf();

#if	defined(LINEMODE) && (VEOF == VMIN)
	if (!tty_isediting()) {
		extern char oldeofc;
		*pfrontp++ = oldeofc;
		return;
	}
#endif
	*pfrontp++ = slctab[SLC_EOF].sptr ?
			(unsigned char)*slctab[SLC_EOF].sptr : '\004';
}
