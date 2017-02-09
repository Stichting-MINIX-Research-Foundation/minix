/*	$NetBSD: pcf8584reg.h,v 1.1 2007/04/14 19:33:30 tnn Exp $ */

/* Written by Tobias Nygren. Released into the public domain. */

/* SCL clock frequency */
#define	PCF8584_SCL_90		0 	/* 90 kHz */
#define	PCF8584_SCL_45		1 	/* 45 kHz */
#define	PCF8584_SCL_11		2	/* 11 kHz */
#define	PCF8584_SCL_1_5		3	/* 1.5 kHz */

/* Internal clock frequency */
#define	PCF8584_CLK_3		0	/* 3 MHz */
#define	PCF8584_CLK_4_43	0x10	/* 4.43 MHz */
#define	PCF8584_CLK_6		0x14	/* 6 MHz */
#define	PCF8584_CLK_8		0x18	/* 8 MHz */
#define	PCF8584_CLK_12		0x1C	/* 12 MHz */

/* Control register bits (write only) */
#define	PCF8584_CTRL_ACK	(1<<0)	/* send ACK */
#define	PCF8584_CTRL_STO	(1<<1)	/* send STOP */
#define	PCF8584_CTRL_STA	(1<<2)	/* send START */
#define	PCF8584_CTRL_ENI	(1<<3)	/* Enable Interrupt */
#define	PCF8584_CTRL_ES2	(1<<4)	/* alternate register selection */
#define	PCF8584_CTRL_ES1	(1<<5)	/* alternate register selection */
#define	PCF8584_CTRL_ESO	(1<<6)	/* Enable Serial Output */
#define	PCF8584_CTRL_PIN	(1<<7)	/* Pending Interrupt Not */

/* Status register bits (read only) */
#define	PCF8584_STATUS_BBN	(1<<0)	/* Bus Busy Not */
#define	PCF8584_STATUS_LAB	(1<<1)	/* Lost Arbitration */
#define	PCF8584_STATUS_AAS	(1<<2)	/* Adressed As Slave */
#define	PCF8584_STATUS_LRB	(1<<3)	/* Last Received Bit (NAK+bcast det.) */
#define	PCF8584_STATUS_BER	(1<<4)	/* Bus error */
#define	PCF8584_STATUS_STS	(1<<5)	/* external STOP condition detected */
#define	PCF8584_STATUS_INI	(1<<6)	/* 0 if initialized */
#define	PCF8584_STATUS_PIN	(1<<7)	/* Pending Interrupt Not */

#define	PCF8584_REG_S0_		0			/* S0' own address */
#define	PCF8584_REG_S2		PCF8584_CTRL_ES1	/* clock register */
#define	PCF8584_REG_S3		PCF8584_CTRL_ES2	/* Interrupt vector */

#define PCF8584_CMD_START (PCF8584_CTRL_PIN | PCF8584_CTRL_ESO | \
    PCF8584_CTRL_STA | PCF8584_CTRL_ACK)
#define PCF8584_CMD_STOP (PCF8584_CTRL_PIN | PCF8584_CTRL_ESO | \
    PCF8584_CTRL_STO | PCF8584_CTRL_ACK)
#define PCF8584_CMD_NAK (PCF8584_CTRL_ESO)
