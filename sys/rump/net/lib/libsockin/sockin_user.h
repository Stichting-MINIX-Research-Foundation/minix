/*	$NetBSD: sockin_user.h,v 1.1 2014/03/13 01:40:30 pooka Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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

enum rumpcomp_sockin_getnametype {
	RUMPCOMP_SOCKIN_SOCKNAME,
	RUMPCOMP_SOCKIN_PEERNAME
};

int  rumpcomp_sockin_socket(int, int, int, int *);
int  rumpcomp_sockin_sendmsg(int, const struct msghdr *, int, size_t *);
int  rumpcomp_sockin_recvmsg(int, struct msghdr *, int, size_t *);
int  rumpcomp_sockin_connect(int, const struct sockaddr *, int);
int  rumpcomp_sockin_bind(int, const struct sockaddr *, int);
int  rumpcomp_sockin_accept(int, struct sockaddr *, int *, int *);
int  rumpcomp_sockin_listen(int, int);
int  rumpcomp_sockin_getname(int, struct sockaddr *, int *,
			      enum rumpcomp_sockin_getnametype);
int  rumpcomp_sockin_setsockopt(int, int, int, const void *, int);
int  rumpcomp_sockin_poll(struct pollfd *, int, int, int *);
