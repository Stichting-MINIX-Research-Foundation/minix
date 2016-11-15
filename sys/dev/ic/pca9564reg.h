/*	$NetBSD: pca9564reg.h,v 1.1 2010/04/09 10:09:50 nonaka Exp $	*/

/*
 * Copyright (c) 2010 NONAKA Kimihiro <nonaka@netbsd.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DEV_IC_PCA9564REG_H_
#define	_DEV_IC_PCA9564REG_H_

#define	PCA9564_I2CSTA		0x00	/* R: Status */
#define	PCA9564_I2CCTO		0x00	/* W: Time-out */
#define	PCA9564_I2CDAT		0x01	/* R/W: Data */
#define	PCA9564_I2CADR		0x02	/* R/W: Own address */
#define	PCA9564_I2CCON		0x03	/* R/W: Control */

/* Control */
#define	I2CCON_CR0		(1 << 0)	/* Clock Rate Bit0 */
#define	I2CCON_CR1		(1 << 1)	/* Clock Rate Bit1 */
#define	I2CCON_CR2		(1 << 2)	/* Clock Rate Bit2 */
#define	 I2CCON_CR_330KHZ	(0x0)
#define	 I2CCON_CR_288KHZ	(0x1)
#define	 I2CCON_CR_217KHZ	(0x2)
#define	 I2CCON_CR_146KHZ	(0x3)
#define	 I2CCON_CR_88KHZ	(0x4)
#define	 I2CCON_CR_59KHZ	(0x5)
#define	 I2CCON_CR_44KHZ	(0x6)
#define	 I2CCON_CR_36KHZ	(0x7)
#define	 I2CCON_CR_MASK		(0x7)
#define	I2CCON_SI		(1 << 3)	/* Serial Interrupt Flag */
#define	I2CCON_STO		(1 << 4)	/* Stop Flag */
#define	I2CCON_STA		(1 << 5)	/* Start Flag */
#define	I2CCON_ENSIO		(1 << 6)	/* SIO Enable Bit */
#define	I2CCON_AA		(1 << 7)	/* Assert Acknowledge Flag */

#endif	/* _DEV_IC_PCA9564REG_H_ */
