/*	$NetBSD: putter_sys.h,v 1.3 2007/11/20 18:35:22 pooka Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Research Foundation of Helsinki University of Technology
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

#ifndef _DEV_PUTTER_PUTTERSYS_H_
#define _DEV_PUTTER_PUTTERSYS_H_

#include <sys/param.h>
#include <sys/proc.h>

#include <dev/putter/putter.h>

/*
 * Configuration data.
 *
 * Users of putter currently must be registered statically.  This sucks,
 * I know, but what are you going to do since you need static allocation
 * for /dev nodes anyway.  Ok, we could be slightly more forgiving about
 * this, but let's just wait for devfs for now.
 */
#define	PUTTER_MINOR_WILDCARD	0
#define	PUTTER_MINOR_PUD	1
#define	PUTTER_MINOR_COMPAT	0x7ffff		/* will die sometime soon */

struct putter_ops {
	int	(*pop_getout)(void *, size_t, int, uint8_t **,size_t *,void **);
	void	(*pop_releaseout)(void *, void *, int);
	size_t	(*pop_waitcount)(void *);
	int	(*pop_dispatch)(void *, struct putter_hdr *);
	int	(*pop_close)(void *);
};

typedef	int (*putter_config_fn)(int, int, int);

struct putter_instance;
struct putter_instance	*putter_attach(pid_t, int, void *,
				       struct putter_ops *);
void			putter_detach(struct putter_instance *);
void			putter_notify(struct putter_instance *);
int			putter_register(putter_config_fn, int);

#endif /* _DEV_PUTTER_PUTTERSYS_H_ */
