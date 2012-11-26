/*	$NetBSD: cdefs.h,v 1.7 2012/08/05 04:13:19 matt Exp $	*/

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#if defined (__ARM_ARCH_7__) || defined (__ARM_ARCH_7A__) || \
    defined (__ARM_ARCH_7R__) || defined (__ARM_ARCH_7M__) || \
    defined (__ARM_ARCH_7EM__) /* 7R, 7M, 7EM are for non MMU arms */
#define _ARM_ARCH_7
#endif

#if defined (_ARM_ARCH_7) || defined (__ARM_ARCH_6__) || \
    defined (__ARM_ARCH_6J__) || defined (__ARM_ARCH_6K__) || \
    defined (__ARM_ARCH_6Z__) || defined (__ARM_ARCH_6ZK__) || \
    defined (__ARM_ARCH_6T2__) || defined (__ARM_ARCH_6ZM__)
#define _ARM_ARCH_6
#endif

#if defined (_ARM_ARCH_6) || defined (__ARM_ARCH_5__) || \
    defined (__ARM_ARCH_5T__) || defined (__ARM_ARCH_5TE__) || \
    defined (__ARM_ARCH_5TEJ__)
#define _ARM_ARCH_5
#endif

#if defined (_ARM_ARCH_5) || defined (__ARM_ARCH_4T__)
#define _ARM_ARCH_4T
#endif

#if defined(_ARM_ARCH_6) || defined (__ARM_ARCH_5TE__) || \
    defined (__ARM_ARCH_5TEJ__)
#define	_ARM_ARCH_DWORD_OK
#endif

#ifdef __ARM_EABI__
#define __ALIGNBYTES		(8 - 1)
#else
#define __ALIGNBYTES		(sizeof(int) - 1)
#endif

#endif /* !_MACHINE_CDEFS_H_ */
