/*	$NetBSD: util.c,v 1.5 2012/03/06 16:55:18 mbalmer Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* util.c - utility routines. */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: util.c,v 1.5 2012/03/06 16:55:18 mbalmer Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "defs.h"

/* Error routine */
void
yyerror(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	printf("%s:%d: ", src_name, line_no);
	vfprintf(stdout, fmt, args);
	printf("\n");
	va_end(args);
	had_errors = TRUE;
}

/* Buffer routines */
static char *mc_buff = NULL;
static int mc_size = 0;
static int mc_loc = 0;

void
buff_add_ch(char ch)
{
	char *t;

	if (mc_loc >= mc_size-1) {
		if (mc_size == 0)
			mc_size = 80;
		else
			mc_size *= 2;
		t = (char *)malloc(mc_size);
		if (t == NULL) {
			(void)fprintf(stderr, "%s:%d: Malloc error\n",
					 src_name, line_no);
			exit(1);
		}
		if (mc_buff != NULL) {
			strcpy(t, mc_buff);
			free(mc_buff);
		}
		mc_buff = t;
	}
	mc_buff[mc_loc++] = ch;
	mc_buff[mc_loc] = '\0';
}

/* get a copy of the string ! */
char *
buff_copy(void)
{
	char *res = strdup(mc_buff);

	mc_loc = 0;
	mc_buff[0] = '\0';
	return res;
}
