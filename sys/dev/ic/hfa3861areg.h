/* $NetBSD: hfa3861areg.h,v 1.4 2009/10/19 23:19:39 rmind Exp $ */
/* $Id: hfa3861areg.h,v 1.4 2009/10/19 23:19:39 rmind Exp $ */
/*
 * Copyright (c) 2007 David Young.  All rights reserved.
 *
 * Written by David Young.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID YOUNG ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DAVID
 * YOUNG BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
#ifndef _DEV_IC_HFA3861A_H_
#define _DEV_IC_HFA3861A_H_

/* Register set for the Intersil HFA3861A. */

#define	HFA3861A_CR5	0x0a	/* Tx signal field, read-write */
#define	HFA3861A_CR5_RSVD0	__BITS(7, 4)	/* reserved, set to 0 */
#define	HFA3861A_CR5_SHORTPRE	__BIT(3)	/* 0: long preamble,
						 * 1: short preamble
						 */
#define	HFA3861A_CR5_RSVD1	__BIT(2)	/* reserved, set to 0 */
#define	HFA3861A_CR5_RATE_MASK	__BITS(1, 0)	/* Tx data rate */
/* 1, 2, 5.5, and 11 Mbps */
#define	HFA3861A_CR5_RATE_1	__SHIFTIN(0, HFA3861A_CR5_RATE_MASK)
#define	HFA3861A_CR5_RATE_2	__SHIFTIN(1, HFA3861A_CR5_RATE_MASK)
#define	HFA3861A_CR5_RATE_5	__SHIFTIN(2, HFA3861A_CR5_RATE_MASK)
#define	HFA3861A_CR5_RATE_11	__SHIFTIN(3, HFA3861A_CR5_RATE_MASK)

#define	HFA3861A_CR6	0x0c	/* Tx service field: copied directly to 802.11
				 * PLCP header
				 */
#define	HFA3861A_CR7	0x0e	/* Tx length, hi: microseconds Tx duration */
#define	HFA3861A_CR8	0x10	/* Tx length, lo: microseconds Tx duration */

#define	HFA3861A_CR49	0x62	/* Read-only register mux control */
#define	HFA3861A_CR49_SEL	__BIT(7)	/* 0: read-only register set 'b'
						 * 1: read-only register set 'a'
						 */
#define	HFA3861A_CR49_RSVD	__BITS(6, 0)

#define	HFA3861A_CR61	0x7c	/* Rx status, read-only, sets 'a' & 'b' */

#define	HFA3861A_CR62	0x7e	/* RSSI, read-only */
#define	HFA3861A_CR62_RSSI	__BITS(7, 0)	/* RSSI, sets 'a' & 'b' */

#define	HFA3861A_CR63	0x80	/* Rx status, read-only */
#define	HFA3861A_CR63_SIGNAL_MASK	__BITS(7, 6)	/* signal field value,
							 * sets 'a' & 'b' */
/* 1 Mbps */
#define	HFA3861A_CR63_SIGNAL_1MBPS	__SHIFTIN(0, HFA3861A_CR63_SIGNAL)
/* 2 Mbps */
#define	HFA3861A_CR63_SIGNAL_2MBPS	__SHIFTIN(2, HFA3861A_CR63_SIGNAL)
/* 5.5 or 11 Mbps */
#define	HFA3861A_CR63_SIGNAL_OTHER_MBPS	__SHIFTIN(1, HFA3861A_CR63_SIGNAL)
#define	HFA3861A_CR63_SFD	__BIT(5)	/* SFD found, sets 'a' & 'b' */
#define	HFA3861A_CR63_SHORTPRE	__BIT(4)	/* short preamble detected,
						 * sets 'a' & 'b'
						 */
#define	HFA3861A_CR63_SIGNAL_OK	__BIT(3)	/* valid signal field,
						 * sets 'a' & 'b'
						 */
#define	HFA3861A_CR63_CRC16_OK	__BIT(2)	/* valid CRC 16,
						 * sets 'a' & 'b'
						 */
#define	HFA3861A_CR63_ANTENNA	__BIT(1)	/* antenna used,
						 * sets 'a' & 'b'
						 */
#define	HFA3861A_CR63_RSVD	__BIT(0)	/* reserved, sets 'a' & 'b' */

#endif /* _DEV_IC_HFA3861A_H_ */
