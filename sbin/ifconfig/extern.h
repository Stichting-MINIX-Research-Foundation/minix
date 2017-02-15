/*	$NetBSD: extern.h,v 1.14 2009/08/07 18:53:37 dyoung Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */
#ifndef	_IFCONFIG_EXTERN_H
#define	_IFCONFIG_EXTERN_H

#include <prop/proplib.h>
#include "util.h"

#define	RIDADDR 0  
#define	ADDR    1
#define	MASK    2
#define	DSTADDR 3

typedef void (*usage_cb_t)(prop_dictionary_t);
typedef void (*status_cb_t)(prop_dictionary_t, prop_dictionary_t);
typedef void (*statistics_cb_t)(prop_dictionary_t);

enum flag_type {
	  FLAG_T_MOD = 0
	, FLAG_T_CMD = 1
};

typedef enum flag_type flag_type_t;

struct statistics_func {
	SIMPLEQ_ENTRY(statistics_func)	f_next;
	statistics_cb_t			f_func;
};

struct usage_func {
	SIMPLEQ_ENTRY(usage_func)	f_next;
	usage_cb_t			f_func;
};

struct status_func {
	SIMPLEQ_ENTRY(status_func)	f_next;
	status_cb_t			f_func;
};

struct cmdloop_branch {
	SIMPLEQ_ENTRY(cmdloop_branch)	b_next;
	struct parser			*b_parser;
};


typedef struct statistics_func statistics_func_t;
typedef struct status_func status_func_t;
typedef struct usage_func usage_func_t;
typedef struct cmdloop_branch cmdloop_branch_t;

void cmdloop_branch_init(cmdloop_branch_t *, struct parser *);
int register_family(struct afswtch *);
int register_cmdloop_branch(cmdloop_branch_t *);
void statistics_func_init(statistics_func_t *, statistics_cb_t);
void status_func_init(status_func_t *, status_cb_t);
void usage_func_init(usage_func_t *, usage_cb_t);
int register_statistics(statistics_func_t *);
int register_status(status_func_t *);
int register_usage(usage_func_t *);
int register_flag(int);
bool get_flag(int);

extern bool lflag, Nflag, vflag, zflag;

#endif	/* _IFCONFIG_EXTERN_H */
