/*	$NetBSD: g760areg.h,v 1.1 2010/10/02 06:07:37 kiyohara Exp $	*/

/*-
 * Copyright (C) 2008 A.Leo.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _DEV_I2C_G760aREG_H_
#define _DEV_I2C_G760aREG_H_

#define	G760A_ADDR			0x3e

#define	G760A_REG_SET_CNT		0x00	/* RW, Programmed fan speed
						   register, it contains the
						   count number of the
						   desired fan speed. */

#define	G760A_REG_ACT_CNT		0x01	/* RO, Actual fan speed. */
#define	G760A_REG_FAN_STA		0x02	/* RO, Fan status. */
#define G760A_REG_FAN_STA_MASK		0x03u	/* 2 lower bits */

#define G760A_CLK			32768
#define G760A_P				2



#define G760A_N2RPM(a)			(G760A_CLK*30/G760A_P/(a)) 
#define G760A_RPM2N(a)			(G760A_CLK*30/G760A_P/(a))     /* wow */


#endif /* !_DEV_I2C_G760AREG_H_ */
