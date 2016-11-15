/*	$NetBSD: mvxpsecreg.h,v 1.1 2015/06/03 04:20:02 hsuenaga Exp $	*/
/*
 * Copyright (c) 2015 Internet Initiative Japan Inc.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Cryptographic Engine and Security Accelerator(CESA)
 */

#ifndef __MVXPSECREG_H__
#define __MVXPSECREG_H__

/* Security Accelerator */
#define MV_ACC_COMMAND			0xDE00
#define MV_ACC_COMMAND_ACT		(0x01 << 0)
#define MV_ACC_COMMAND_STOP		(0x01 << 2)

#define MV_ACC_DESC			0xDE04
#define MV_ACC_DESC_MASK		0x0000ffff

#define MV_ACC_CONFIG			0xDE08
#define MV_ACC_CONFIG_STOP_ON_ERR	(0x01 << 0)
#define MV_ACC_CONFIG_WAIT_TDMA		(0x01 << 7)
#define MV_ACC_CONFIG_ACT_TDMA		(0x01 << 9)
#define MV_ACC_CONFIG_MULT_PKT		(0x01 << 11)

#define MV_ACC_STATUS			0xDE0C
#define MV_ACC_STATUS_ACC_ACT		(0x01 << 1)
#define MV_ACC_STATUS_MAC_ERR		(0x01 << 8)
#define MV_ACC_STATUS_ACT_STATUS_MASK	0x0007ffff
#define MV_ACC_STATUS_ACT_STATUS_SHIFT	13

/* Security Accelerator Algorithms */
/* XXX: simplify shift operation.... */
#define MV_ACC_CRYPTO_OP_MASK		0x03
#define MV_ACC_CRYPTO_OP_SHIFT		0
#define MV_ACC_CRYPTO_OP_MAC		(0x00 << MV_ACC_CRYPTO_OP_SHIFT)
#define MV_ACC_CRYPTO_OP_ENC		(0x01 << MV_ACC_CRYPTO_OP_SHIFT)
#define MV_ACC_CRYPTO_OP_MACENC		(0x02 << MV_ACC_CRYPTO_OP_SHIFT)
#define MV_ACC_CRYPTO_OP_ENCMAC		(0x03 << MV_ACC_CRYPTO_OP_SHIFT)
#define MV_ACC_CRYPTO_OP(x) \
    (((x) & (MV_ACC_CRYPTO_OP_MASK << MV_ACC_CRYPTO_OP_SHIFT)) \
     >> MV_ACC_CRYPTO_OP_SHIFT)

#define MV_ACC_CRYPTO_MAC_MASK		0x07
#define MV_ACC_CRYPTO_MAC_SHIFT		4
#define MV_ACC_CRYPTO_MAC_NONE		0
#define MV_ACC_CRYPTO_MAC_SHA2		(0x01 << MV_ACC_CRYPTO_MAC_SHIFT)
#define MV_ACC_CRYPTO_MAC_HMAC_SHA2	(0x03 << MV_ACC_CRYPTO_MAC_SHIFT)
#define MV_ACC_CRYPTO_MAC_MD5		(0x04 << MV_ACC_CRYPTO_MAC_SHIFT)
#define MV_ACC_CRYPTO_MAC_SHA1		(0x05 << MV_ACC_CRYPTO_MAC_SHIFT)
#define MV_ACC_CRYPTO_MAC_HMAC_MD5	(0x06 << MV_ACC_CRYPTO_MAC_SHIFT)
#define MV_ACC_CRYPTO_MAC_HMAC_SHA1	(0x07 << MV_ACC_CRYPTO_MAC_SHIFT)
#define MV_ACC_CRYPTO_MAC(x) \
    (((x) & (MV_ACC_CRYPTO_MAC_MASK << MV_ACC_CRYPTO_MAC_SHIFT)) \
     >> MV_ACC_CRYPTO_MAC_SHIFT)
#define MV_ACC_CRYPTO_MAC_SET(dst, x) \
    do { \
	    (dst) &= ~(MV_ACC_CRYPTO_MAC_MASK << MV_ACC_CRYPTO_MAC_SHIFT);\
	    (dst) |= \
	     ((x) & (MV_ACC_CRYPTO_MAC_MASK << MV_ACC_CRYPTO_MAC_SHIFT)); \
    } while(0);

#define MV_ACC_CRYPTO_ENC_MASK		0x03
#define MV_ACC_CRYPTO_ENC_SHIFT		8
#define MV_ACC_CRYPTO_ENC_NOP		(0x00 << MV_ACC_CRYPTO_ENC_SHIFT)
#define MV_ACC_CRYPTO_ENC_DES		(0x01 << MV_ACC_CRYPTO_ENC_SHIFT)
#define MV_ACC_CRYPTO_ENC_3DES		(0x02 << MV_ACC_CRYPTO_ENC_SHIFT)
#define MV_ACC_CRYPTO_ENC_AES		(0x03 << MV_ACC_CRYPTO_ENC_SHIFT)
#define MV_ACC_CRYPTO_ENC(x) \
    (((x) & (MV_ACC_CRYPTO_ENC_MASK << MV_ACC_CRYPTO_ENC_SHIFT)) \
     >> MV_ACC_CRYPTO_ENC_SHIFT)
#define MV_ACC_CRYPTO_ENC_SET(dst, x) \
    do { \
	    (dst) &= ~(MV_ACC_CRYPTO_ENC_MASK << MV_ACC_CRYPTO_ENC_SHIFT);\
	    (dst) |= \
	      ((x) & (MV_ACC_CRYPTO_ENC_MASK << MV_ACC_CRYPTO_ENC_SHIFT));\
    } while(0);

/* this is not described in the document.... FUUUUUUUUUUUUCK! */
#define MV_ACC_CRYPTO_AES_KLEN_MASK	0x03
#define MV_ACC_CRYPTO_AES_KLEN_SHIFT	24
#define MV_ACC_CRYPTO_AES_KLEN_128 \
    (0x00 << MV_ACC_CRYPTO_AES_KLEN_SHIFT)
#define MV_ACC_CRYPTO_AES_KLEN_192 \
    (0x01 << MV_ACC_CRYPTO_AES_KLEN_SHIFT)
#define MV_ACC_CRYPTO_AES_KLEN_256 \
    (0x02 << MV_ACC_CRYPTO_AES_KLEN_SHIFT)
#define MV_ACC_CRYPTO_AES_KLEN(x) \
    (((x) & (MV_ACC_CRYPTO_AES_KLEN_MASK << MV_ACC_CRYPTO_AES_KLEN_SHIFT)) \
     >> MV_ACC_CRYPTO_AES_KLEN_SHIFT)
#define MV_ACC_CRYPTO_AES_KLEN_SET(dst, x) \
    do { \
	(dst) &= \
	  ~(MV_ACC_CRYPTO_AES_KLEN_MASK << MV_ACC_CRYPTO_AES_KLEN_SHIFT); \
	(dst) |= \
	  ((x) & \
	  (MV_ACC_CRYPTO_AES_KLEN_MASK << MV_ACC_CRYPTO_AES_KLEN_SHIFT)); \
    } while(0);

#define MV_ACC_CRYPTO_MAC_96		__BIT(7)
#define MV_ACC_CRYPTO_DECRYPT		__BIT(12)
#define MV_ACC_CRYPTO_CBC		__BIT(16)
#define MV_ACC_CRYPTO_3DES_EDE		__BIT(20)

/* Security Accelerator Descriptors */
/* Algorithm names are defined in mviicesa.h */
#define MV_ACC_CRYPTO_FRAG_MASK		0x03
#define MV_ACC_CRYPTO_FRAG_SHIFT	30
#define MV_ACC_CRYPTO_NOFRAG		(0x00 << MV_ACC_CRYPTO_FRAG_SHIFT)
#define MV_ACC_CRYPTO_FRAG_FIRST	(0x01 << MV_ACC_CRYPTO_FRAG_SHIFT)
#define MV_ACC_CRYPTO_FRAG_LAST		(0x02 << MV_ACC_CRYPTO_FRAG_SHIFT)
#define MV_ACC_CRYPTO_FRAG_MID		(0x03 << MV_ACC_CRYPTO_FRAG_SHIFT)
#define MV_ACC_CRYPTO_FRAG(x) \
    (((x) & (MV_ACC_CRYPTO_FRAG_MASK << MV_ACC_CRYPTO_FRAG_SHIFT)) \
     >> MV_ACC_CRYPTO_FRAG_SHIFT)

#define MV_ACC_DESC_VAL_1(x)		((x) & 0x7ff)
#define MV_ACC_DESC_VAL_2(x)		(((x) & 0x7ff) << 16)
#define MV_ACC_DESC_VAL_3(x)		(((x) & 0xffff) << 16)
#define MV_ACC_DESC_GET_VAL_1(x)	((x) & 0x7ff)
#define MV_ACC_DESC_GET_VAL_2(x)	(((x) & (0x7ff << 16)) >> 16)
#define MV_ACC_DESC_GET_VAL_3(x)	(((x) & (0xffff << 16)) >> 16)

#define MV_ACC_DESC_ENC_DATA(src, dst) \
	(MV_ACC_DESC_VAL_1(src) | MV_ACC_DESC_VAL_2(dst))
#define MV_ACC_DESC_ENC_LEN(len) \
	(MV_ACC_DESC_VAL_1(len))
#define MV_ACC_DESC_ENC_KEY(key) \
	(MV_ACC_DESC_VAL_1(key))
#define MV_ACC_DESC_ENC_IV(iv_e, iv_d) \
	(MV_ACC_DESC_VAL_1(iv_e) | MV_ACC_DESC_VAL_2(iv_d))

#define MV_ACC_DESC_MAC_SRC(src, len) \
	(MV_ACC_DESC_VAL_1(src) | MV_ACC_DESC_VAL_3(len))
#define MV_ACC_DESC_MAC_DST(dst, len) \
	(MV_ACC_DESC_VAL_1(dst) | MV_ACC_DESC_VAL_2(len))
#define MV_ACC_DESC_MAC_IV(iv_in, iv_out) \
	(MV_ACC_DESC_VAL_1(iv_in) | MV_ACC_DESC_VAL_2(iv_out))

#define MV_ACC_SRAM_SIZE 2048

/* Interrupt Cause */
#define MVXPSEC_INT_CAUSE		0xDE20
#define MVXPSEC_INT_MASK		0xDE24

/* ENGINE interrupts */
#define MVXPSEC_INT_AUTH		__BIT(0)
#define MVXPSEC_INT_DES			__BIT(1)
#define MVXPSEC_INT_AES_ENC		__BIT(2)
#define MVXPSEC_INT_AES_DEC		__BIT(3)
#define MVXPSEC_INT_ENC			__BIT(4)
#define MVXPSEC_INT_ENGINE \
    (MVXPSEC_INT_AUTH | MVXPSEC_INT_ENC | \
     MVXPSEC_INT_DES | MVXPSEC_INT_AES_ENC | MVXPSEC_INT_AES_DEC)

/* Security Accelerator interrupts */
#define MVXPSEC_INT_SA			__BIT(5)
#define MVXPSEC_INT_ACCTDMA		__BIT(7)
#define MVXPSEC_INT_ACCTDMA_CONT	__BIT(11)
#define MVXPSEC_INT_COAL		__BIT(14)

/* TDMA interrupts */
#define MVXPSEC_INT_TDMA_COMP		__BIT(9)
#define MVXPSEC_INT_TDMA_OWN		__BIT(10)

#define MVXPSEC_INT_ACC \
    (MVXPSEC_INT_SA | MVXPSEC_INT_ACCTDMA | MVXPSEC_INT_ACCTDMA_CONT)

#define MVXPSEC_INT_TDMA \
    (MVXPSEC_INT_TDMA_COMP | MVXPSEC_INT_TDMA_OWN)

#define MVXPSEC_INT_ALL \
    (MVXPSEC_INT_ENGINE | MVXPSEC_INT_ACC | MVXPSEC_INT_TDMA)

/*
 * TDMA Controllers
 */
/* TDMA Address */
#define MV_TDMA_NWINDOW			4
#define MV_TDMA_BAR(window)		(0x0A00 + (window) * 8)
#define MV_TDMA_BAR_BASE_MASK		__BITS(31,16)
#define MV_TDMA_BAR_BASE(base)		((base) & MV_TDMA_BAR_BASE_MASK)
#define MV_TDMA_ATTR(window)		(0x0A04 + (window) * 8)
#define MV_TDMA_ATTR_SIZE_MASK		__BITS(31,16)
#define MV_TDMA_ATTR_ATTR_MASK		__BITS(31,16)
#define MV_TDMA_ATTR_ENABLE		__BIT(0)
#define MV_TDMA_ATTR_SIZE(size)		((((size - 1) >> 16) & 0xffff) << 16)
#define MV_TDMA_ATTR_ATTR(attr)		(((attr) & 0xff) << 8)
#define MV_TDMA_ATTR_TARGET(target)	(((target) & 0xf) << 4)
#define MV_TDMA_ATTR_GET_SIZE(reg)	(((reg) >> 16) & 0xffff)
#define MV_TDMA_ATTR_GET_ATTR(reg)	(((reg) >> 8) & 0xff)
#define MV_TDMA_ATTR_GET_TARGET(reg)	(((reg) >> 4) & 0xf)

/* TDMA Control */
#define MV_TDMA_CONTROL			0x0840

#define MV_TDMA_CONTROL_DST_BURST_MASK	__BITS(2,0)
#define MV_TDMA_CONTROL_DST_BURST_32	0x3
#define MV_TDMA_CONTROL_DST_BURST_128	0x4
#define MV_TDMA_CONTROL_GET_DST_BURST(reg) \
    ((uint32_t)(((reg) & MV_TDMA_CONTROL_DST_BURST_MASK) >> 0))
#define MV_TDMA_CONTROL_OUTS_EN		__BIT(4)
#define MV_TDMA_CONTROL_SRC_BURST_MASK	__BITS(8,6)
#define MV_TDMA_CONTROL_SRC_BURST_32	(0x3 << 6)
#define MV_TDMA_CONTROL_SRC_BURST_128	(0x4 << 6)
#define MV_TDMA_CONTROL_GET_SRC_BURST(reg) \
    ((uint32_t)(((reg) & MV_TDMA_CONTROL_SRC_BURST_MASK) >> 6))
#define MV_TDMA_CONTROL_CHAIN_DIS	__BIT(9)
#define MV_TDMA_CONTROL_BSWAP_DIS	__BIT(11)
#define MV_TDMA_CONTROL_ENABLE		__BIT(12)
#define MV_TDMA_CONTROL_FETCH		__BIT(13)
#define MV_TDMA_CONTROL_ACT		__BIT(14)
#define MV_TDMA_CONTROL_OUTS_MODE_MASK	__BITS(17,16)
#define MV_TDMA_CONTROL_OUTS_MODE_4OUTS	(3 << 16)

/* TDMA Descriptor Registers */
#define MV_TDMA_CNT			0x0800
#define MV_TDMA_SRC			0x0810
#define MV_TDMA_DST			0x0820
#define MV_TDMA_NXT			0x0830
#define MV_TDMA_CUR			0x0870

#define MV_TDMA_CNT_OWN			(1 << 31)

/* TDMA Interrupt */
#define MV_TDMA_ERR_CAUSE		0x08C8
#define MV_TDMA_ERR_MASK		0x08CC

#define MV_TDMA_ERRC_MISS		0x01
#define	MV_TDMA_ERRC_DHIT		0x02
#define MV_TDMA_ERRC_BHIT		0x04
#define MV_TDMA_ERRC_DERR		0x08
#define MV_TDMA_ERRC_ALL \
    (MV_TDMA_ERRC_MISS | MV_TDMA_ERRC_DHIT | MV_TDMA_ERRC_BHIT | \
    MV_TDMA_ERRC_DERR)

/* Crypto Engine Registers (just for debug) */
#define MV_CE_DES_KEY0L			0xdd48
#define MV_CE_DES_KEY0H			0xdd4c
#define MV_CE_DES_KEY1L			0xdd50
#define MV_CE_DES_KEY1H			0xdd54
#define MV_CE_DES_KEY2L			0xdd60
#define MV_CE_DES_KEY2H			0xdd64

#define MV_CE_AES_EKEY(n)		(0xdd80 + (4 * (7 - (n))))
#define MV_CE_AES_DKEY(n)		(0xddc0 + (4 * (7 - (n))))

#endif /* __MVXPSECREG_H__ */
