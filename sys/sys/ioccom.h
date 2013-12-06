/*	$NetBSD: ioccom.h,v 1.11 2011/10/19 10:53:12 yamt Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	@(#)ioccom.h	8.3 (Berkeley) 1/9/95
 */

#ifndef	_SYS_IOCCOM_H_
#define	_SYS_IOCCOM_H_

#if !defined(__minix)
/*
 * Ioctl's have the command encoded in the lower word, and the size of
 * any in or out parameters in the upper word.  The high 3 bits of the
 * upper word are used to encode the in/out status of the parameter.
 *
 *	 31 29 28                     16 15            8 7             0
 *	+---------------------------------------------------------------+
 *	| I/O | Parameter Length        | Command Group | Command       |
 *	+---------------------------------------------------------------+
 */
#define	IOCPARM_MASK	0x1fff		/* parameter length, at most 13 bits */
#else
/* For Minix, reserve one extra flag bit: the 'big' size flag. 
 * We have big ioctls and can't help it.
 *
 *	 31   28 27                     16 15            8 7             0
 *	+----------------------------------------------------------------+
 *	| I/O/B | Parameter Length       | Command Group | Command       |
 *	+----------------------------------------------------------------+
 */
#define IOC_BIG		(unsigned long)0x10000000
#define	IOCPARM_MASK	0xfff		/* parameter length, at most 12 bits */
#define IOCPARM_MASK_BIG       0xFFFFF	/* or 20 bits, if IOC_BIG is set */
#define	IOCPARM_SHIFT_BIG	8	
#endif /* !defined(__minix) */

#define	IOCPARM_SHIFT	16
#define	IOCGROUP_SHIFT	8
#define	IOCPARM_LEN(x)	(((x) >> IOCPARM_SHIFT) & IOCPARM_MASK)
#define	IOCBASECMD(x)	((x) & ~(IOCPARM_MASK << IOCPARM_SHIFT))
#define	IOCGROUP(x)	(((x) >> IOCGROUP_SHIFT) & 0xff)

#define	IOCPARM_MAX	NBPG	/* max size of ioctl args, mult. of NBPG */
				/* no parameters */
#define	IOC_VOID	(unsigned long)0x20000000
				/* copy parameters out */
#define	IOC_OUT		(unsigned long)0x40000000
				/* copy parameters in */
#define	IOC_IN		(unsigned long)0x80000000
				/* copy parameters in and out */
#define	IOC_INOUT	(IOC_IN|IOC_OUT)
				/* mask for IN/OUT/VOID */
#define	IOC_DIRMASK	(unsigned long)0xe0000000

#define	_IOC(inout, group, num, len) \
    ((inout) | (((len) & IOCPARM_MASK) << IOCPARM_SHIFT) | \
    ((group) << IOCGROUP_SHIFT) | (num))
#define	_IO(g,n)	_IOC(IOC_VOID,	(g), (n), 0)
#define	_IOR(g,n,t)	_IOC(IOC_OUT,	(g), (n), sizeof(t))
#define	_IOW(g,n,t)	_IOC(IOC_IN,	(g), (n), sizeof(t))
/* this should be _IORW, but stdio got there first */
#define	_IOWR(g,n,t)	_IOC(IOC_INOUT,	(g), (n), sizeof(t))

#if defined(__minix)
#define _IOW_BIG(y,t)  (y | ((sizeof(t) & IOCPARM_MASK_BIG) << IOCPARM_SHIFT_BIG) \
        | IOC_IN | IOC_BIG)
#define _IOR_BIG(y,t)  (y | ((sizeof(t) & IOCPARM_MASK_BIG) << IOCPARM_SHIFT_BIG) \
        | IOC_OUT | IOC_BIG)
#define _IORW_BIG(y,t) (y | ((sizeof(t) & IOCPARM_MASK_BIG) << IOCPARM_SHIFT_BIG) \
        | IOC_INOUT | IOC_BIG)

/* Decode an ioctl call. */
#define _MINIX_IOCTL_SIZE(i)            (((i) >> IOCPARM_SHIFT) & IOCPARM_MASK)
#define _MINIX_IOCTL_IOR(i)             ((i) & IOC_OUT)
#define _MINIX_IOCTL_IORW(i)            ((i) & IOC_INOUT)
#define _MINIX_IOCTL_IOW(i)             ((i) & IOC_IN)

/* Recognize and decode size of a 'big' ioctl call. */
#define _MINIX_IOCTL_BIG(i)             ((i) & IOC_BIG)
#define _MINIX_IOCTL_SIZE_BIG(i)        (((i) >> IOCPARM_SHIFT_BIG) & IOCPARM_MASK_BIG)
#endif /* defined(__minix) */

#endif /* !_SYS_IOCCOM_H_ */
