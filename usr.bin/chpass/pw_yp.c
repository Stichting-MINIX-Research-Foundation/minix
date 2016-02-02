/*	$NetBSD: pw_yp.c,v 1.23 2015/06/17 00:01:59 christos Exp $	*/

/*
 * Copyright (c) 1988 The Regents of the University of California.
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
static char sccsid[] = "@(#)pw_yp.c	1.0 2/2/93";
#else
__RCSID("$NetBSD: pw_yp.c,v 1.23 2015/06/17 00:01:59 christos Exp $");
#endif
#endif /* not lint */

#ifdef	YP

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#define passwd yp_passwd_rec
#include <rpcsvc/yppasswd.h>
#undef passwd

#include "chpass.h"

static char *domain;

/*
 * Check if rpc.yppasswdd is running on the master YP server.
 * XXX this duplicates some code, but is much less complex
 * than the alternative.
 */
int
check_yppasswdd(void)
{
	char *master;
	int rpcport;

	/*
	 * Get local domain
	 */
	if (!domain && yp_get_default_domain(&domain) != 0)
		return (1);

	/*
	 * Find the host for the passwd map; it should be running
	 * the daemon.
	 */
	master = NULL;
	if (yp_master(domain, "passwd.byname", &master) != 0) {
		if (master != NULL)
			free (master);
		return (1);
	}

	/*
	 * Ask the portmapper for the port of the daemon.
	 */
	if ((rpcport = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE,
	    IPPROTO_UDP)) == 0)
		return (1);

	/*
	 * Successful contact with rpc.yppasswdd.
	 */
	return (0);
}

int
pw_yp(struct passwd *pw, uid_t ypuid)
{
	char *master;
	int r, rpcport, status;
	struct yppasswd yppw;
	struct timeval tv;
	CLIENT *client;
	
	/*
	 * Get local domain
	 */
	if (!domain && (r = yp_get_default_domain(&domain)))
		errx(1, "can't get local YP domain.  Reason: %s",
		    yperr_string(r));

	/*
	 * Find the host for the passwd map; it should be running
	 * the daemon.
	 */
	master = NULL;
	if ((r = yp_master(domain, "passwd.byname", &master)) != 0) {
		if (master)
			free (master);
		warnx("can't find the master YP server.  Reason: %s",
		    yperr_string(r));
		return (1);
	}

	/*
	 * Ask the portmapper for the port of the daemon.
	 */
	if ((rpcport = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE,
	    IPPROTO_UDP)) == 0) {
		warnx("master YP server not running yppasswd daemon.\n\t%s",
		    "Can't change password.");
		return (1);
	}

	/*
	 * Be sure the port is privileged
	 */
	if (rpcport >= IPPORT_RESERVED) {
		warnx("yppasswd daemon is on an invalid port.");
		return (1);
	}

	/* prompt for old password */
	memset(&yppw, 0, sizeof yppw);
	yppw.oldpass = getpass("Old password:");
	if (!yppw.oldpass) {
		warnx("Cancelled.");
		return (1);
	}

	/* tell rpc.yppasswdd */
	yppw.newpw.pw_name	 = strdup(pw->pw_name);
	if (!yppw.newpw.pw_name) {
		err(1, "strdup");
		/*NOTREACHED*/
	}
	yppw.newpw.pw_passwd = strdup(pw->pw_passwd);
	if (!yppw.newpw.pw_passwd) {
		err(1, "strdup");
		/*NOTREACHED*/
	}
	yppw.newpw.pw_uid 	 = pw->pw_uid;
	yppw.newpw.pw_gid	 = pw->pw_gid;
	yppw.newpw.pw_gecos	 = strdup(pw->pw_gecos);
	if (!yppw.newpw.pw_gecos) {
		err(1, "strdup");
		/*NOTREACHED*/
	}
	yppw.newpw.pw_dir	 = strdup(pw->pw_dir);
	if (!yppw.newpw.pw_dir) {
		err(1, "strdup");
		/*NOTREACHED*/
	}
	yppw.newpw.pw_shell	 = strdup(pw->pw_shell);
	if (!yppw.newpw.pw_shell) {
		err(1, "strdup");
		/*NOTREACHED*/
	}
	
	client = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
	if (client == NULL) {
		warnx("cannot contact yppasswdd on %s:  Reason: %s",
		    master, yperr_string(YPERR_YPBIND));
		return (1);
	}
	client->cl_auth = authunix_create_default();
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	r = clnt_call(client, YPPASSWDPROC_UPDATE,
	    xdr_yppasswd, &yppw, xdr_int, &status, tv);
	if (r) {
		warnx("rpc to yppasswdd failed.");
		return (1);
	} else if (status)
		printf("Couldn't change YP password.\n");
	else
		printf("%s %s, %s\n",
		    "The YP password information has been changed on",
		    master, "the master YP passwd server.");
	return (0);
}

void
yppw_error(const char *name, int yperr, int eval)
{

	if (yperr) {
		if (name)
			warn("%s", name);
		else
			warn(NULL);
	}

	errx(eval, "YP passwd information unchanged");
}

void
yppw_prompt(void)
{
	int c;

	(void)printf("re-edit the password file? [y]: ");
	(void)fflush(stdout);
	c = getchar();
	if (c != EOF && c != '\n')
		while (getchar() != '\n');
	if (c == 'n')
		yppw_error(NULL, 0, 0);
}
#endif	/* YP */
