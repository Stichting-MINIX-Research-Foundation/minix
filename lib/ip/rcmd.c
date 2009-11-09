/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rcmd.c	5.22 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/hton.h>
#include <net/netlib.h>

#define MAXHOSTNAMELEN	256
#define MAXPATHLEN PATH_MAX

#ifdef __STDC__
#define CONST	const
#else
#define CONST
#endif

extern	errno;

int rcmd(ahost, rport, locuser, remuser, cmd, fd2p)
char **ahost;
int rport;
CONST char *locuser, *remuser, *cmd;
int *fd2p;
{
	int fd, fd2, result;
	struct hostent *hp;
	int n;
	static tcpport_t lport;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t tcpconnopt;
	pid_t pid;
	char num[8];
	char c;
	char *tcp_device;

	fd= -1;
	fd2= -1;

	if (lport == 0) {
		pid = getpid();
		lport = 1;
		do {
			lport = (lport << 1) | (pid & 1);

			pid >>= 1;
		} while (lport < TCPPORT_RESERVED/2);
	}

	tcp_device= getenv("TCP_DEVICE");
	if (tcp_device == NULL)
		tcp_device= TCP_DEVICE;
	hp= gethostbyname(*ahost);
	if (!hp)
	{
		fprintf(stderr, "%s: unknown host\n", *ahost);
		return -1;
	}
	*ahost= hp->h_name;
	n = TCPPORT_RESERVED/2;
	do
	{
		if (--lport < TCPPORT_RESERVED/2)
			lport = TCPPORT_RESERVED-1;
		fd= open (tcp_device, O_RDWR);
		if (fd<0)
		{
			fprintf(stderr, "unable to open %s: %s\n",
				tcp_device, strerror(errno));
			goto bad;
		}
		tcpconf.nwtc_flags= NWTC_LP_SET | NWTC_SET_RA | NWTC_SET_RP |
			NWTC_EXCL;
		tcpconf.nwtc_locport= htons(lport);
		tcpconf.nwtc_remport= rport;
		tcpconf.nwtc_remaddr= *(ipaddr_t *)hp->h_addr;

		result= ioctl(fd, NWIOSTCPCONF, &tcpconf);
		if (result<0)
		{
			if (errno == EADDRINUSE)
			{
				close(fd);
				continue;
			}
			fprintf(stderr, "unable to ioctl(NWIOSTCPCONF): %s\n",
				strerror(errno));
			goto bad;
		}
		tcpconf.nwtc_flags= NWTC_SHARED;
		result= ioctl(fd, NWIOSTCPCONF, &tcpconf);
		if (result<0)
		{
			fprintf(stderr, "unable to ioctl(NWIOSTCPCONF): %s\n",
				strerror(errno));
			goto bad;
		}
		tcpconnopt.nwtcl_flags= 0;

		do
		{
			result= ioctl (fd, NWIOTCPCONN, &tcpconnopt);
			if (result<0 && errno == EAGAIN)
			{
				sleep(2);
			}
		} while (result<0 && errno == EAGAIN);
		if (result<0 && errno != EADDRINUSE)
		{
			fprintf(stderr,
				"unable to ioctl(NWIOTCPCONN): %s\n",
				strerror(errno));
			goto bad;
		}
		if (result>=0)
			break;
	} while (--n > 0);
	if (n == 0)
	{
		fprintf(stderr, "can't get port\n");
		return -1;
	}
	if (!fd2p)
	{
		if (write(fd, "", 1) != 1)
		{
			fprintf(stderr, "unable to write: %s", strerror(errno));
			goto bad;
		}
	}
	else
	{
		fd2= open (tcp_device, O_RDWR);
		if (fd2<0)
		{
			fprintf(stderr, "unable to open %s: %s\n",
				tcp_device, strerror(errno));
			goto bad;
		}
		tcpconf.nwtc_flags= NWTC_LP_SET | NWTC_UNSET_RA | 
			NWTC_UNSET_RP | NWTC_SHARED;
		tcpconf.nwtc_locport= htons(lport);

		result= ioctl(fd2, NWIOSTCPCONF, &tcpconf);
		if (result<0)
		{
			fprintf(stderr,
				"unable to ioctl(NWIOSTCPCONF): %s\n",
				strerror(errno));
			goto bad;
		}
		pid= fork();
		if (pid<0)
		{
			fprintf(stderr, "unable to fork: %s\n",
				strerror(errno));
			goto bad;
		}
		if (!pid)
		{
			alarm(0);
			signal(SIGALRM, SIG_DFL);
			alarm(30); /* give up after half a minute */
			tcpconnopt.nwtcl_flags= 0;

			do
			{
				result= ioctl (fd2, NWIOTCPLISTEN,
					&tcpconnopt);
				if (result<0 && errno == EAGAIN)
				{
					sleep(2);
				}
			} while (result<0 && errno == EAGAIN);
			if (result<0 && errno != EADDRINUSE)
			{
				fprintf(stderr,
					"unable to ioctl(NWIOTCPLISTEN): %s\n",
					strerror(errno));
				exit(1);
			}
			if (result>=0)
				exit(0);
			else
				exit(1);
		}
		/*
		 * This sleep is a HACK.  The command that we are starting
		 * will try to connect to the fd2 port.  It seems that for
		 * this to succeed the child process must have already made
		 * the call to ioctl above (the NWIOTCPLISTEN) call.
		 * The sleep gives the child a chance to make the call
		 * before the parent sends the port number to the
		 * command being started.
		 */
		sleep(1);

		sprintf(num, "%d", lport);
		if (write(fd, num, strlen(num)+1) != strlen(num)+1)
		{
			fprintf(stderr, "unable to write: %s\n",
				strerror(errno));
			goto bad;
		}

	}
	write (fd, locuser, strlen(locuser)+1);
	write (fd, remuser, strlen(remuser)+1);
	write (fd, cmd, strlen(cmd)+1);
	if (read(fd, &c, 1) != 1)
	{
		fprintf(stderr, "unable to read: %s\n", strerror(errno) );
		goto bad;
	}
	if (c != 0)
	{
		while (read(fd, &c, 1) == 1)
		{
			write(2, &c, 1);
			if (c == '\n')
				break;
		}
		goto bad;
	}
	if (fd2p)
	{
		*fd2p= fd2;
		result= ioctl(fd2, NWIOGTCPCONF, &tcpconf);
		if (result<0)
		{
			fprintf(stderr, "unable to ioctl(NWIOGTCPCONF): %s\n",
				strerror(errno) );
			goto bad;
		}
		if (ntohs(tcpconf.nwtc_remport) >= TCPPORT_RESERVED)
		{
			fprintf(stderr, "unable to setup 2nd channel\n");
			goto bad;
		}
	}
	return fd;

bad:
	if (fd>=0)
		close(fd);
	if (fd2>=0)
		close(fd2);
	return -1;
}
