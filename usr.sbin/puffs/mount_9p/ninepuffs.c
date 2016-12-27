/*	$NetBSD: ninepuffs.c,v 1.24 2011/08/31 13:32:39 joerg Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * 9puffs: access a 9P file server as a vfs via puffs
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: ninepuffs.c,v 1.24 2011/08/31 13:32:39 joerg Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ninepuffs.h"

#define DEFPORT_9P 564

__dead static void
usage(void)
{

	fprintf(stderr, "usage: %s [-s] [-o mntopts] [-p port] "
	    "[user@]server[:path] mountpoint\n", getprogname());
	exit(1);
}

/*
 * TCPv4 connection to 9P file server, forget IL and IPv6 for now.
 * Return connected socket or exit with error.
 */
static int
serverconnect(const char *addr, unsigned short port)
{
	struct sockaddr_in mysin;
	struct hostent *he;
	int s;

	he = gethostbyname2(addr, AF_INET);
	if (he == NULL) {
		herror("gethostbyname");
		exit(1);
	}

	s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1)
		err(1, "socket");

	memset(&mysin, 0, sizeof(struct sockaddr_in));
	mysin.sin_family = AF_INET;
	mysin.sin_port = htons(port);
	memcpy(&mysin.sin_addr, he->h_addr, sizeof(struct in_addr));

	if (connect(s, (struct sockaddr *)&mysin, sizeof(mysin)) == -1)
		err(1, "connect");

	return s;
}

int
main(int argc, char *argv[])
{
	struct puffs9p p9p;
	struct puffs_usermount *pu;
	struct puffs_ops *pops;
	struct puffs_node *pn_root;
	mntoptparse_t mp;
	const char *user, *srvhost, *srvpath;
	char *p;
	unsigned short port;
	int mntflags, pflags, ch;
	int detach;

	setprogname(argv[0]);

	if (argc < 2)
		usage();

	mntflags = pflags = 0;
	detach = 1;
	port = DEFPORT_9P;

	while ((ch = getopt(argc, argv, "o:p:s")) != -1) {
		switch (ch) {
		case 'o':
			mp = getmntopts(optarg, puffsmopts, &mntflags, &pflags);
			if (mp == NULL)
				err(1, "getmntopts");
			freemntopts(mp);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 's':
			detach = 0;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	if (pflags & PUFFS_FLAG_OPDUMP)
		detach = 0;
	pflags |= PUFFS_KFLAG_WTCACHE | PUFFS_KFLAG_IAONDEMAND;

	PUFFSOP_INIT(pops);

	PUFFSOP_SET(pops, puffs9p, fs, unmount);
	PUFFSOP_SETFSNOP(pops, sync);
	PUFFSOP_SETFSNOP(pops, statvfs);

	PUFFSOP_SET(pops, puffs9p, node, lookup);
	PUFFSOP_SET(pops, puffs9p, node, readdir);
	PUFFSOP_SET(pops, puffs9p, node, getattr);
	PUFFSOP_SET(pops, puffs9p, node, setattr);
	PUFFSOP_SET(pops, puffs9p, node, create);
	PUFFSOP_SET(pops, puffs9p, node, open);
	PUFFSOP_SET(pops, puffs9p, node, mkdir);
	PUFFSOP_SET(pops, puffs9p, node, remove);
	PUFFSOP_SET(pops, puffs9p, node, rmdir);
	PUFFSOP_SET(pops, puffs9p, node, read);
	PUFFSOP_SET(pops, puffs9p, node, write);
	PUFFSOP_SET(pops, puffs9p, node, rename);
	PUFFSOP_SET(pops, puffs9p, node, inactive);
	PUFFSOP_SET(pops, puffs9p, node, reclaim);
#if 0
	PUFFSOP_SET(pops, puffs9p, node, mknod);
#endif

	pu = puffs_init(pops, argv[0], "9p", &p9p, pflags);
	if (pu == NULL)
		err(1, "puffs_init");

	memset(&p9p, 0, sizeof(p9p));
	p9p.maxreq = P9P_DEFREQLEN;
	p9p.nextfid = 1;

	/* user@ */
	if ((p = strchr(argv[0], '@')) != NULL) {
		*p = '\0';
		srvhost = p+1;
		user = argv[0];
	} else {
		struct passwd *pw;

		srvhost = argv[0];
		pw = getpwuid(getuid());
		if (pw == NULL)
			err(1, "getpwuid");
		user = pw->pw_name;
	}

	/* :/mountpath */
	if ((p = strchr(srvhost, ':')) != NULL) {
		*p = '\0';
		srvpath = p+1;
		if (*srvpath != '/')
			errx(1, "%s is not an absolute path", srvpath);
	} else {
		srvpath = "/";
	}

	p9p.servsock = serverconnect(srvhost, port);
	if ((pn_root = p9p_handshake(pu, user, srvpath)) == NULL) {
		puffs_exit(pu, 1);
		exit(1);
	}

	puffs_framev_init(pu, p9pbuf_read, p9pbuf_write, p9pbuf_cmp, NULL,
	    puffs_framev_unmountonclose);
	if (puffs_framev_addfd(pu, p9p.servsock,
	    PUFFS_FBIO_READ | PUFFS_FBIO_WRITE) == -1)
		err(1, "puffs_framebuf_addfd");

	if (detach)
		if (puffs_daemon(pu, 1, 1) == -1)
			err(1, "puffs_daemon");

	if (puffs_mount(pu, argv[1], mntflags, pn_root) == -1)
		err(1, "puffs_mount");
	if (puffs_setblockingmode(pu, PUFFSDEV_NONBLOCK) == -1)
		err(1, "setblockingmode");

	if (puffs_mainloop(pu) == -1)
		err(1, "mainloop");

	return 0;
}
