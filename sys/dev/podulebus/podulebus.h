/* $NetBSD: podulebus.h,v 1.8 2005/12/11 12:23:28 christos Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * podulebus.h
 *
 * Podule bus header file
 *
 * Created      : 26/04/95
 */

#ifndef _DEV_PODULEBUS_PODULEBUS_H_
#define _DEV_PODULEBUS_PODULEBUS_H_

/* Define the structures used to describe the "known" podules */

struct podule_description {
	int product_id;
	const char *description;
};

struct manufacturer_description {
	int manufacturer_id;
	const char *description;
};

#include <machine/podulebus_machdep.h>

extern void podulebus_readcmos(struct podulebus_attach_args *, u_int8_t *);

/* Podule loader functions. */
extern int podulebus_initloader(struct podulebus_attach_args *);
extern int podloader_readbyte(struct podulebus_attach_args *, u_int);
extern void podloader_writebyte(struct podulebus_attach_args *, u_int, int);
void podloader_reset(struct podulebus_attach_args *);
int podloader_callloader(struct podulebus_attach_args *, u_int, u_int);

#endif
