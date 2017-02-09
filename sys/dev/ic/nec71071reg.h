/* $NetBSD: nec71071reg.h,v 1.1 2006/10/01 12:39:35 bjh21 Exp $ */

/*
 * Ben Harris 2006
 *
 * This file is in the public domain.
 */

/*
 * NEC uPD71071 DMA Controller
 * register definitions
 */

/*
 * This chip is suspiciously much like the Intel 8237, but not actually
 * compatible with it.
 */

/* Register offsets */

#define NEC71071_INIT		0x0  /* Initialize */
#define		 INIT_RES	0x01 /* Reset */
#define		 INIT_16B	0x02 /* 16-bit data bus */
#define NEC71071_CHANNEL	0x1  /* Channel Register Read/Write */
#define		 CHANNEL_SEL0	0x01 /* Channel 0 selected (R) */
#define		 CHANNEL_SEL1	0x02 /* Channel 1 selected (R) */
#define		 CHANNEL_SEL2	0x04 /* Channel 2 selected (R) */
#define		 CHANNEL_SEL3	0x08 /* Channel 3 selected (R) */
#define		 CHANNEL_RBASE	0x10 /* Only base registers may be accessed */
#define		 CHANNEL_SELCH	0x03 /* Channel to select (W) */
#define		 CHANNEL_WBASE	0x04 /* Only base registers may be accessed */
#define NEC71071_COUNTLO	0x2  /* Count register, low byte */
#define NEC71071_COUNTHI	0x3  /* Count register, high byte */
#define NEC71071_ADDRLO		0x4  /* Address register, low byte */
#define NEC71071_ADDRMID	0x5  /* Address register, middle byte */
#define NEC71071_ADDRHI		0x6  /* Address register, high byte */
#define NEC71071_DCTRL1		0x8  /* Device control register, low byte */
#define		 DCTRL1_MTM	0x01 /* Memory-to-Memory */
#define		 DCTRL1_AHLD	0x02 /* Fixed Address */
#define		 DCTRL1_DDMA	0x04 /* Disable DMA Operation */
#define		 DCTRL1_CMP	0x08 /* Compressed Timing */
#define		 DCTRL1_ROT	0x10 /* Rotational Priority */
#define		 DCTRL1_EXW	0x20 /* Extended Writing */
#define		 DCTRL1_RQL	0x40 /* DMARQ active low */
#define		 DCTRL1_AKL	0x80 /* DMAAK active high */
#define NEC71071_DCTRL2		0x9  /* Device control register, high byte */
#define		 DCTRL2_BHLD	0x01 /* Bus Hold mode */
#define		 DCTRL2_WEV	0x02 /* Write Enable During Verify */
#define NEC71071_MODE		0xA  /* Mode control register */
#define		 MODE_WNB	0x01 /* Word (not byte) transfer */
#define		 MODE_TDIR	0x0c /* Transfer direction */
#define		 MODE_TDIR_VRFY	0x00 /* Verify */
#define		 MODE_TDIR_IOTM	0x04 /* I/O to memory */
#define		 MODE_TDIR_MTIO	0x08 /* memory to I/O */
#define		 MODE_AUTI	0x10 /* Autoinitialize */
#define		 MODE_ADIR	0x20 /* Address direction (decrement) */
#define		 MODE_TMODE	0xc0 /* Transfer mode */
#define		 MODE_TMODE_DMD	0x00 /* Demand mode */
#define		 MODE_TMODE_SGL 0x40 /* Single mode */
#define		 MODE_TMODE_BLK	0x80 /* Block mode */
#define		 MODE_TMODE_CAS 0xc0 /* Cascade mode */
#define NEC71071_STATUS		0xB  /* Status register */
#define		 STATUS_TC	0x0f /* Terminal count (one per channel) */
#define		 STATUS_RQ	0xf0 /* DMA Request active (one per channel) */
#define NEC71071_TEMPLO		0xC  /* Temporary register (low byte) */
#define NEC71071_TEMPHI		0xD  /* Temporary register (high byte) */
#define NEC71071_REQUEST	0xE  /* Request register (one bit/channel) */
#define NEC71071_MASK		0xF  /* Mask register (one bit/channel) */

