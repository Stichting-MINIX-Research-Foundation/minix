/*      $NetBSD: prog_ops.h,v 1.3 2011/03/11 09:59:56 pooka Exp $	*/

/*
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PROG_OPS_H_
#define _PROG_OPS_H_

#include <sys/types.h>

#ifndef CRUNCHOPS
struct prog_ops {
	int (*op_init)(void);

	int (*op_socket)(int, int, int);
	int (*op_setsockopt)(int, int, int, const void *, socklen_t);
	int (*op_shutdown)(int, int);

	int (*op_poll)(struct pollfd *, nfds_t, int);

	ssize_t (*op_recvfrom)(int, void *, size_t, int,
			       struct sockaddr *, socklen_t *);
	ssize_t (*op_sendto)(int, const void *, size_t, int,
			       const struct sockaddr *, socklen_t);

	int (*op_close)(int);

	uid_t (*op_getuid)(void);
	int (*op_setuid)(uid_t);
};
extern const struct prog_ops prog_ops;

#define prog_init prog_ops.op_init
#define prog_socket prog_ops.op_socket
#define prog_setsockopt prog_ops.op_setsockopt
#define prog_shutdown prog_ops.op_shutdown
#define prog_poll prog_ops.op_poll
#define prog_recvfrom prog_ops.op_recvfrom
#define prog_sendto prog_ops.op_sendto
#define prog_close prog_ops.op_close
#define prog_getuid prog_ops.op_getuid
#define prog_setuid prog_ops.op_setuid
#else
#define prog_init ((int (*)(void))NULL)
#define prog_socket socket
#define prog_setsockopt setsockopt
#define prog_shutdown shutdown
#define prog_poll poll
#define prog_recvfrom recvfrom
#define prog_sendto sendto
#define prog_close close
#define prog_getuid getuid
#define prog_setuid setuid
#endif

#endif /* _PROG_OPS_H_ */
