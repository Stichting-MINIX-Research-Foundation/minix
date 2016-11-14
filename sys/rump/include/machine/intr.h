/*	$NetBSD: intr.h,v 1.20 2014/01/19 07:01:55 martin Exp $	*/

/*
 * Copyright (c) 2009, 2010 Antti Kantee.  All Rights Reserved.
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

#ifndef _SYS_RUMP_INTR_H_
#define _SYS_RUMP_INTR_H_

#ifndef _LOCORE

typedef uint8_t ipl_t;
typedef struct {
        ipl_t _ipl;
} ipl_cookie_t;

static inline ipl_cookie_t
makeiplcookie(ipl_t ipl)
{
	ipl_cookie_t c;
	c._ipl = ipl;
	return c;
}

#endif /* !_LOCORE */

#define spllower(x) ((void)x)
#define splraise(x) 0
#define splsoftserial() 0
#define splsoftnet() 0
#define splsoftclock() 0
#define splhigh() 0
#define splsched() 0
#define splvm() 0
#define splx(x) ((void)x)
#define spl0() ((void)0)

/*
 * IPL_* does not mean anything to a run-to-completition rump kernel,
 * but we sometimes assert a "not higher than" condition, so we assign
 * different values (following spl(9)).
 */
#define IPL_NONE 0
#define	IPL_SOFTCLOCK 1
#define	IPL_SOFTBIO 2
#define	IPL_SOFTNET 3
#define IPL_SOFTSERIAL 4
#define IPL_VM 5
#define IPL_SCHED 6
#define IPL_HIGH 7

#endif /* _SYS_RUMP_INTR_H_ */
