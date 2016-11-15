/*	$NetBSD: spkrio.h,v 1.2 2015/09/06 06:01:00 dholland Exp $	*/

/*
 * spkr.h -- interface definitions for speaker ioctl()
 */

#ifndef _DEV_ISA_SPKR_H_
#define _DEV_ISA_SPKR_H_

#include <sys/ioccom.h>

#define SPKRTONE        _IOW('S', 1, tone_t)    /* emit tone */
#define SPKRTUNE        _IO('S', 2)             /* emit tone sequence */

typedef struct {
	int	frequency;	/* in hertz */
	int	duration;	/* in 1/100ths of a second */
} tone_t;

#endif /* _DEV_ISA_SPKR_H_ */
