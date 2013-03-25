/*	$NetBSD: once.h,v 1.5 2008/10/09 10:48:21 pooka Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
 * Copyright (c)2008 Antti Kantee,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYS_ONCE_H_
#define	_SYS_ONCE_H_

typedef struct {
	unsigned o_status;
	int o_error;
#define	ONCE_VIRGIN	0
#define	ONCE_RUNNING	1
#define	ONCE_DONE	2
} once_t;

void once_init(void);
int _run_once(once_t *, int (*)(void));

#define	ONCE_DECL(o) \
	once_t (o) = { \
		.o_status = 0, \
	};

#define	RUN_ONCE(o, fn) \
    (__predict_true((o)->o_status == ONCE_DONE) ? \
      ((o)->o_error) : _run_once((o), (fn)))

#endif /* _SYS_ONCE_H_ */
