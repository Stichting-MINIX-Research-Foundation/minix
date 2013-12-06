/*	$NetBSD: sdt.h,v 1.4 2013/10/07 07:11:40 dholland Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by CoyotePoint Systems, Inc. It was developed under contract to 
 * CoyotePoint by Darran Hunt.
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

#ifndef _SDT_H_
#define _SDT_H_

/* should be stdint.h, but this works transparently for both user and kernel */
#include <sys/stdint.h>

#if defined(_KERNEL_OPT)
#include "opt_dtrace.h"
#endif

#define SDT_MAX_PROVIDER	1024	/* max number of SDT providers */
#define SDT_MAX_ARGS		5	/* max number of probe arguments */
#define SDT_MAX_NAME_SIZE	64	/* max size of provider name */

typedef struct {
    int		created;	/* boolean: probe created? */
    int		enabled;	/* boolean: probe enabled? */
    int		id;		/* dtrace provided probe id */
    const char	*provider;	/* provider name */
    const char	*module;	/* module name */
    const char	*function;	/* function name */
    const char	*name;		/* probe name */
    const char	*argv[SDT_MAX_ARGS];	/* probe argument types */
    const char	*argx[SDT_MAX_ARGS];	/* probe argument xlate types */
} sdt_probe_t;


/*
 * This type definition must match that of dtrace_probe. It is defined this
 * way to avoid having to rely on CDDL code.
 */
typedef	void (*sdt_probe_func_t)(uint32_t, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);

/*
 * The hook for the probe function. See kern_sdt.c which defaults this to
 * it's own stub. The 'sdt' provider will set it to dtrace_probe when it
 * loads.
 */
extern sdt_probe_func_t	sdt_probe_func;


#define SDT_NAME(prov, mod, func, name) \
    	prov##_##mod##_##func##_##name

#ifdef KDTRACE_HOOKS
/*
 * SDT_PROBE_DEFINE(prov, mod, func, name, sname,
 *		    arg0, argx0, arg1, argx1,
 *		    arg2, argx2, arg3, argx3, arg4, argx4)
 *
 * 	prov	- provider name
 * 	mod	- module name
 * 	func	- function name
 * 	name	- probe name
 * 	sname	- probe name as exposed to userland
 * 	arg0 - arg4, argument types as strings, or NULL.
 * 	argx0 - argx4, translation types for arg0 - arg4
 *
 * 	e.g. SDT_PROBE_DEFINE(l7, l7lb, l7lb_create_node, 
 * 				"void *", NULL, 
 * 				NULL, NULL, NULL, NULL,
 * 				NULL, NULL, NULL NULL, )
 *
 *	This is used in the target module to define probes to be used.
 *	The translation type should be set to NULL if not used.
 */
#define SDT_PROBE_DEFINE(prov, mod, func, name, sname, \
	    arg0, argx0, arg1, argx1, arg2, argx2, \
	    arg3, argx3, arg4, argx4) \
    	sdt_probe_t SDT_NAME(prov, mod, func, name) = { \
	    0, 0, 0, #prov, #mod, #func, #sname, \
	    { arg0, arg1, arg2, arg3, arg4 }, \
	    { NULL, NULL, NULL, NULL, NULL } \
	}

/* Use this in this module to declare probes defined in the kernel. */
#define SDT_PROBE_DECLARE(prov, mod, func, name) \
    	extern sdt_probe_t SDT_NAME(prov, mod, func, name);

/* Use this in the target modules to provide instrumentation points */
#define SDT_PROBE(prov, mod, func, name, arg0, arg1, arg2, arg3, arg4) \
    	if (__predict_false(SDT_NAME(prov, mod, func, name).enabled)) { \
	    	(*sdt_probe_func)(SDT_NAME(prov, mod, func, name).id, \
		    (uintptr_t)(arg0), (uintptr_t)(arg1), (uintptr_t)(arg2), \
		    (uintptr_t)(arg3), (uintptr_t)(arg4)); \
	}
#else
#define SDT_PROBE_DEFINE(prov, mod, func, name, sname, \
	    arg0, argx0, arg1, argx1, arg2, argx2, \
	    arg3, argx3, arg4, argx4)
#define SDT_PROBE_DECLARE(prov, mod, func, name)
#define SDT_PROBE(prov, mod, func, name, arg0, arg1, arg2, arg3, arg4) 
#endif

void sdt_init(void *);
void sdt_exit(void);

#endif
