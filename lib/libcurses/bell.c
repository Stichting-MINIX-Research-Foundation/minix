/*	$NetBSD: bell.c,v 1.8 2010/02/03 15:34:40 roy Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
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
#ifndef lint
__RCSID("$NetBSD: bell.c,v 1.8 2010/02/03 15:34:40 roy Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * beep
 *	Ring the terminal bell
 */
int
beep(void)
{
	if (bell != NULL) {
#ifdef DEBUG
		__CTRACE(__CTRACE_MISC, "beep: bl\n");
#endif
		tputs(bell, 0, __cputchar);
	} else if (flash_screen != NULL) {
#ifdef DEBUG
		__CTRACE(__CTRACE_MISC, "beep: vb\n");
#endif
		tputs(flash_screen, 0, __cputchar);
	}
	return (1);
}

/*
 * flash
 *	Flash the terminal screen
 */
int
flash(void)
{
	if (flash_screen != NULL) {
#ifdef DEBUG
		__CTRACE(__CTRACE_MISC, "flash: vb\n");
#endif
		tputs(flash_screen, 0, __cputchar);
	} else if (bell != NULL) {
#ifdef DEBUG
		__CTRACE(__CTRACE_MISC, "flash: bl\n");
#endif
		tputs(bell, 0, __cputchar);
	}
	return (1);
}
