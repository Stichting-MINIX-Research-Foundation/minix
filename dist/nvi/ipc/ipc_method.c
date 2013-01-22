/*	$NetBSD: ipc_method.c,v 1.1.1.2 2008/05/18 14:31:25 aymeric Exp $ */

/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/uio.h>

#include "../common/common.h"
#include "ip.h"

static int vi_send_ __P((IPVIWIN   *, int));
static int vi_send_1 __P((IPVIWIN   *, int, u_int32_t  ));
static int vi_send_12 __P((IPVIWIN *ipvi, int code, u_int32_t val1, u_int32_t val2));
static int vi_send_ab1 __P((IPVIWIN *ipvi, int code, 
	    const char *str1, u_int32_t len1, 
	    const char *str2, u_int32_t len2, u_int32_t val));
static int vi_send_a1 __P((IPVIWIN *ipvi, int code, const char *str, u_int32_t len, 
	   u_int32_t val));
static int vi_send_a __P((IPVIWIN *ipvi, int code, const char *str, u_int32_t len));

#include "ipc_gen.c"

static int vi_set_ops __P((IPVIWIN *, IPSIOPS *));
static int vi_win_close __P((IPVIWIN *));

static int vi_close __P((IPVI *));
static int vi_new_window __P((IPVI *, IPVIWIN **, int));

/* 
 * vi_create
 *
 * PUBLIC: int vi_create __P((IPVI **, u_int32_t));
 */
int
vi_create(IPVI **ipvip, u_int32_t flags)
{
	IPVI	*ipvi;

	MALLOC_GOTO(NULL, ipvi, IPVI*, sizeof(IPVI));
	memset(ipvi, 0, sizeof(IPVI));

	ipvi->flags = flags;

	ipvi->run = vi_run;
	ipvi->new_window = vi_new_window;
	ipvi->close = vi_close;

	*ipvip = ipvi;

	return 0;

alloc_err:
	return 1;
}

static int 
vi_new_window (IPVI *ipvi, IPVIWIN **ipviwinp, int fd)
{
	IPVIWIN	*ipviwin;

	MALLOC_GOTO(NULL, ipviwin, IPVIWIN*, sizeof(IPVIWIN));
	memset(ipviwin, 0, sizeof(IPVIWIN));

	if (0) {
	ipviwin->ifd = ipvi->ifd;
	ipviwin->ofd = ipvi->ofd;
	} else {
	int sockets[2];
	struct msghdr   mh;
	IPCMSGHDR	    ch;
	char	    dummy;
	struct iovec    iov;

	socketpair(AF_LOCAL, SOCK_STREAM, 0, sockets);

	mh.msg_namelen = 0;
	mh.msg_iovlen = 1;
	mh.msg_iov = &iov;
	mh.msg_controllen = sizeof(ch);
	mh.msg_control = (void *)&ch;

	iov.iov_len = 1;
	iov.iov_base = &dummy;

	ch.header.cmsg_level = SOL_SOCKET;
	ch.header.cmsg_type = SCM_RIGHTS;
	ch.header.cmsg_len = sizeof(ch);

	*(int *)CMSG_DATA(&ch.header) = sockets[1];
	sendmsg(ipvi->ofd, &mh, 0);
	dummy = (fd == -1) ? ' ' : 'F';
	*(int *)CMSG_DATA(&ch.header) = sockets[1];
	sendmsg(sockets[0], &mh, 0);
	close(sockets[1]);

	if (fd != -1) {
		*(int *)CMSG_DATA(&ch.header) = fd;
		sendmsg(sockets[0], &mh, 0);
		close(fd);
	}

	ipviwin->ifd = sockets[0];
	ipviwin->ofd = sockets[0];
	}

#define IPVISET(func) \
	ipviwin->func = vi_##func;

	IPVISET(c_bol);
	IPVISET(c_bottom);
	IPVISET(c_del);
	IPVISET(c_eol);
	IPVISET(c_insert);
	IPVISET(c_left);
	IPVISET(c_right);
	IPVISET(c_top);
	IPVISET(c_settop);
	IPVISET(resize);
	IPVISET(string);
	IPVISET(quit);
	IPVISET(wq);

	IPVISET(input);
	/*
	IPVISET(close);
	*/
	ipviwin->close = vi_win_close;
	IPVISET(set_ops);

	*ipviwinp = ipviwin;

	return 0;

alloc_err:
	return 1;
}

static int 
vi_set_ops(IPVIWIN *ipvi, IPSIOPS *ops)
{
	ipvi->si_ops = ops;
	return 0;
}

static int  vi_close(IPVI *ipvi)
{
	memset(ipvi, 6, sizeof(IPVI));
	free(ipvi);
	return 0;
}

static int  vi_win_close(IPVIWIN *ipviwin)
{
	memset(ipviwin, 6, sizeof(IPVIWIN));
	free(ipviwin);
	return 0;
}


static int
vi_send_(IPVIWIN *ipvi, int code)
{
	IP_BUF	ipb;
	ipb.code = code;
	return vi_send(ipvi->ofd, NULL, &ipb);
}

static int
vi_send_1(IPVIWIN *ipvi, int code, u_int32_t val)
{
	IP_BUF	ipb;
	ipb.code = code;
	ipb.val1 = val;
	return vi_send(ipvi->ofd, "1", &ipb);
}

static int
vi_send_12(IPVIWIN *ipvi, int code, u_int32_t val1, u_int32_t val2)
{
	IP_BUF	ipb;

	ipb.val1 = val1;
	ipb.val2 = val2;
	ipb.code = code;
	return vi_send(ipvi->ofd, "12", &ipb);
}

static int
vi_send_a(IPVIWIN *ipvi, int code, const char *str, u_int32_t len)
{
	IP_BUF	ipb;

	ipb.str1 = str;
	ipb.len1 = len;
	ipb.code = code;
	return vi_send(ipvi->ofd, "a", &ipb);
}

static int
vi_send_a1(IPVIWIN *ipvi, int code, const char *str, u_int32_t len, 
	   u_int32_t val)
{
	IP_BUF	ipb;

	ipb.str1 = str;
	ipb.len1 = len;
	ipb.val1 = val;
	ipb.code = code;
	return vi_send(ipvi->ofd, "a1", &ipb);
}

static int
vi_send_ab1(IPVIWIN *ipvi, int code, const char *str1, u_int32_t len1, 
	    const char *str2, u_int32_t len2, u_int32_t val)
{
	IP_BUF	ipb;

	ipb.str1 = str1;
	ipb.len1 = len1;
	ipb.str2 = str2;
	ipb.len2 = len2;
	ipb.val1 = val;
	ipb.code = code;
	return vi_send(ipvi->ofd, "ab1", &ipb);
}

