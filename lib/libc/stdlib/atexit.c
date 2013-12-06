/*	$NetBSD: atexit.c,v 1.26 2013/08/19 22:14:37 matt Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: atexit.c,v 1.26 2013/08/19 22:14:37 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "reentrant.h"

#include <assert.h>
#include <stdlib.h>

#include "atexit.h"

struct atexit_handler {
	struct atexit_handler *ah_next;
	union {
		void (*fun_atexit)(void);
		void (*fun_cxa_atexit)(void *);
	} ah_fun;
#define	ah_atexit	ah_fun.fun_atexit
#define	ah_cxa_atexit	ah_fun.fun_cxa_atexit

	void *ah_arg;	/* argument for cxa_atexit handlers */
	void *ah_dso;	/* home DSO for cxa_atexit handlers */
};

/*
 * There must be at least 32 to guarantee ANSI conformance, plus
 * 3 additional ones for the benefit of the startup code, which
 * may use them to register the dynamic loader's cleanup routine,
 * the profiling cleanup routine, and the global destructor routine.
 */
#define	NSTATIC_HANDLERS	(32 + 3)
static struct atexit_handler atexit_handler0[NSTATIC_HANDLERS];

#define	STATIC_HANDLER_P(ah)						\
	(ah >= &atexit_handler0[0] && ah < &atexit_handler0[NSTATIC_HANDLERS])

/*
 * Stack of atexit handlers.  Handlers must be called in the opposite
 * order they were registered.
 */
static struct atexit_handler *atexit_handler_stack;

#ifdef _REENTRANT
/* ..and a mutex to protect it all. */
mutex_t __atexit_mutex;
#endif /* _REENTRANT */

void	__libc_atexit_init(void) __attribute__ ((visibility("hidden")));

/*
 * Allocate an atexit handler descriptor.  If "dso" is NULL, it indicates
 * a normal atexit handler, which must be allocated from the static pool,
 * if possible. cxa_atexit handlers are never allocated from the static
 * pool.
 *
 * __atexit_mutex must be held.
 */
static struct atexit_handler *
atexit_handler_alloc(void *dso)
{
	struct atexit_handler *ah;
	int i;

	if (dso == NULL) {
		for (i = 0; i < NSTATIC_HANDLERS; i++) {
			ah = &atexit_handler0[i];
			if (ah->ah_atexit == NULL && ah->ah_next == NULL) {
				/* Slot is free. */
				return (ah);
			}
		}
	}

	/*
	 * Either no static slot was free, or this is a cxa_atexit
	 * handler.  Allocate a new one.  We keep the __atexit_mutex
	 * held to prevent handlers from being run while we (potentially)
	 * block in malloc().
	 */
	ah = malloc(sizeof(*ah));
	return (ah);
}

/*
 * Initialize __atexit_mutex with the PTHREAD_MUTEX_RECURSIVE attribute.
 * Note that __cxa_finalize may generate calls to __cxa_atexit.
 */
void __section(".text.startup")
__libc_atexit_init(void)
{
#ifdef _REENTRANT /* !__minix */
	mutexattr_t atexit_mutex_attr;
	mutexattr_init(&atexit_mutex_attr);
	mutexattr_settype(&atexit_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	mutex_init(&__atexit_mutex, &atexit_mutex_attr);
#endif /* _REENTRANT */
}

/*
 * Register an atexit routine.  This is suitable either for a cxa_atexit
 * or normal atexit type handler.  The __cxa_atexit() name and arguments
 * are specified by the C++ ABI.  See:
 *
 *	http://www.codesourcery.com/cxx-abi/abi.html#dso-dtor
 */
int
__cxa_atexit(void (*func)(void *), void *arg, void *dso)
{
	struct atexit_handler *ah;

	_DIAGASSERT(func != NULL);

	mutex_lock(&__atexit_mutex);

	ah = atexit_handler_alloc(dso);
	if (ah == NULL) {
		mutex_unlock(&__atexit_mutex);
		return (-1);
	}

	ah->ah_cxa_atexit = func;
	ah->ah_arg = arg;
	ah->ah_dso = dso;

	ah->ah_next = atexit_handler_stack;
	atexit_handler_stack = ah;

	mutex_unlock(&__atexit_mutex);
	return (0);
}

/*
 * Run the list of atexit handlers.  If dso is NULL, run all of them,
 * otherwise run only those matching the specified dso.
 *
 * Note that we can be recursively invoked; rtld cleanup is via an
 * atexit handler, and rtld cleanup invokes _fini() for DSOs, which
 * in turn invokes __cxa_finalize() for the DSO.
 */
void
__cxa_finalize(void *dso)
{
	static u_int call_depth;
	struct atexit_handler *ah, *dead_handlers = NULL, **prevp;
	void (*cxa_func)(void *);
	void (*atexit_func)(void);

	mutex_lock(&__atexit_mutex);
	call_depth++;

	/*
	 * If we are at call depth 1 (which is usually the "do everything"
	 * call from exit(3)), we go ahead and remove elements from the
	 * list as we call them.  This will prevent any nested calls from
	 * having to traverse elements we've already processed.  If we are
	 * at call depth > 1, we simply mark elements we process as unused.
	 * When the depth 1 caller sees those, it will simply unlink them
	 * for us.
	 */
again:
	for (prevp = &atexit_handler_stack; (ah = (*prevp)) != NULL;) {
		if (dso == NULL || dso == ah->ah_dso || ah->ah_atexit == NULL) {
			if (ah->ah_atexit != NULL) {
				void *p = atexit_handler_stack;
				if (ah->ah_dso != NULL) {
					cxa_func = ah->ah_cxa_atexit;
					ah->ah_cxa_atexit = NULL;
					(*cxa_func)(ah->ah_arg);
				} else {
					atexit_func = ah->ah_atexit;
					ah->ah_atexit = NULL;
					(*atexit_func)();
				}
				/* Restart if new atexit handler was added. */
				if (p != atexit_handler_stack)
					goto again;
			}

			if (call_depth == 1) {
				*prevp = ah->ah_next;
				if (STATIC_HANDLER_P(ah))
					ah->ah_next = NULL;
				else {
					ah->ah_next = dead_handlers;
					dead_handlers = ah;
				}
			} else
				prevp = &ah->ah_next;
		} else
			prevp = &ah->ah_next;
	}
	call_depth--;
	mutex_unlock(&__atexit_mutex);

	if (call_depth > 0)
		return;

	/*
	 * Now free any dead handlers.  Do this even if we're about to
	 * exit, in case a leak-detecting malloc is being used.
	 */
	while ((ah = dead_handlers) != NULL) {
		dead_handlers = ah->ah_next;
		free(ah);
	}
}

/*
 * Register a function to be performed at exit.
 */
int
atexit(void (*func)(void))
{

	return (__cxa_atexit((void (*)(void *))func, NULL, NULL));
}
