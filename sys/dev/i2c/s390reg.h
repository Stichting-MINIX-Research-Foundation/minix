/*	$NetBSD: s390reg.h,v 1.1 2011/04/04 17:58:40 phx Exp $	*/

/*-
 * Copyright (c) 2011 Frank Wille.
 * All rights reserved.
 *
 * Written by Frank Wille for The NetBSD Project.
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

#ifndef _DEV_I2C_S390REG_H_
#define _DEV_I2C_S390REG_H_

/*
 * S-xx390A Real-Time Clock
 */
  
#define S390_ADDR		0x30	/* Fixed I2C Slave Address */

/* Registers are encoded into the slave address */
#define S390_STATUS1		0
#define S390_STATUS2		1
#define S390_REALTIME1		2
#define S390_REALTIME2		3
#define S390_INT1_1		4
#define S390_INT1_2		5
#define S390_CLOCKADJ		6
#define S390_FREE		7

/* Status1 bits */
#define S390_ST1_POC		(1 << 7)
#define S390_ST1_BLD		(1 << 6)
#define S390_ST1_24H		(1 << 1)
#define S390_ST1_RESET		(1 << 0)

/* Status2 bits */
#define S390_ST2_TEST		(1 << 7)

/* Realtime1 data bytes */
#define S390_RT1_NBYTES		7
#define S390_RT1_YEAR		0
#define S390_RT1_MONTH		1
#define S390_RT1_DAY		2
#define S390_RT1_WDAY		3
#define S390_RT1_HOUR		4
#define S390_RT1_MINUTE		5
#define S390_RT1_SECOND		6

#endif /* _DEV_I2C_S390REG_H_ */
