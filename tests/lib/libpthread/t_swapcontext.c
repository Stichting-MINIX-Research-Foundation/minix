/* $NetBSD: t_swapcontext.c,v 1.1 2012/09/12 02:00:55 manu Exp $ */

/*
 * Copyright (c) 2012 Emmanuel Dreyfus. All rights reserved.
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

#include <sys/cdefs.h>
__RCSID("$NetBSD");

#include <pthread.h>
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

#include "h_common.h"

#define STACKSIZE 65536

char stack[STACKSIZE];
ucontext_t nctx;
ucontext_t octx;
void *oself;
void *nself;
int val1, val2;

/* ARGSUSED0 */
static void
swapfunc(void *arg)
{
	/*
	 * If the test fails, we are very likely to crash 
	 * without the opportunity to report
	 */ 
	nself = (void *)pthread_self();
	printf("after swapcontext self = %p\n", nself);

	ATF_REQUIRE_EQ(oself, nself);
	printf("Test succeeded\n");

	/* NOTREACHED */
	return;
}

/* ARGSUSED0 */
static void *
threadfunc(void *arg)
{
	nctx.uc_stack.ss_sp = stack;
	nctx.uc_stack.ss_size = sizeof(stack);
       
	makecontext(&nctx, (void *)*swapfunc, 0);
       
	oself = (void *)pthread_self();
	printf("before swapcontext self = %p\n", oself);
	PTHREAD_REQUIRE(swapcontext(&octx, &nctx));

	/* NOTREACHED */
	return NULL;
}


ATF_TC(swapcontext1);
ATF_TC_HEAD(swapcontext1, tc)
{
	atf_tc_set_md_var(tc, "descr", "Testing if swapcontext() "
	    "alters pthread_self()");
}
ATF_TC_BODY(swapcontext1, tc)
{
	pthread_t thread;

	oself = (void *)&val1;
	nself = (void *)&val2;

	printf("Testing if swapcontext() alters pthread_self()\n");

	PTHREAD_REQUIRE(getcontext(&nctx));
	PTHREAD_REQUIRE(pthread_create(&thread, NULL, threadfunc, NULL));
	
	return;
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, swapcontext1);

	return atf_no_error();
}
