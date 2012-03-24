/*
 * hermes.h
 *
 * Constants, structures and prototypes needed for the low level access of 
 * Prism cards. The hermes.h file was used as the basis of this file
 *
 * Adjusted to Minix by Stevens Le Blond <slblond@few.vu.nl> 
 *  	            and Michael Valkering <mjvalker@cs.vu.nl>
 */

/* Original copyright notices from hermes.h of the Linux kernel 
 *
 * Copyright (C) 2000, David Gibson, Linuxcare Australia 
 * <hermes@gibson.dropbear.id.au>
 * Portions taken from hfa384x.h, Copyright (C) 1999 AbsoluteValue Systems, Inc. 
 * All Rights Reserved.
 * This file distributed under the GPL, version 2.
 */
#ifndef _HERMES_H
#define _HERMES_H

#include <minix/drivers.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <net/hton.h>
#include <stdarg.h>

/*****************************************************************************
 *              HERMES CONSTANTS                                             *
 *****************************************************************************/
#define		HERMES_ALLOC_LEN_MIN		(4)
#define		HERMES_ALLOC_LEN_MAX		(2400)
#define		HERMES_LTV_LEN_MAX		(34)
#define		HERMES_BAP_DATALEN_MAX		(4096)
#define		HERMES_BAP_OFFSET_MAX		(4096)
#define		HERMES_PORTID_MAX		(7)
#define		HERMES_NUMPORTS_MAX		(HERMES_PORTID_MAX+1)
#define		HERMES_PDR_LEN_MAX		(260)	
#define		HERMES_PDA_RECS_MAX		(200)	
#define		HERMES_PDA_LEN_MAX		(1024)	
#define		HERMES_SCANRESULT_MAX		(35)
#define		HERMES_CHINFORESULT_MAX		(8)
#define		HERMES_MAX_MULTICAST		(16)
#define		HERMES_MAGIC			(0x69ff)

/* 
 * Hermes register offsets
 */
#define		HERMES_CMD			(0x00)
#define		HERMES_PARAM0			(0x02)
#define		HERMES_PARAM1			(0x04)
#define		HERMES_PARAM2			(0x06)
#define		HERMES_STATUS			(0x08)
#define		HERMES_RESP0			(0x0A)
#define		HERMES_RESP1			(0x0C)
#define		HERMES_RESP2			(0x0E)
#define		HERMES_INFOFID			(0x10)
#define		HERMES_RXFID			(0x20)
#define		HERMES_ALLOCFID			(0x22)
#define		HERMES_TXCOMPLFID		(0x24)
#define		HERMES_SELECT0			(0x18)
#define		HERMES_OFFSET0			(0x1C)
#define		HERMES_DATA0			(0x36)
#define		HERMES_SELECT1			(0x1A)
#define		HERMES_OFFSET1			(0x1E)
#define		HERMES_DATA1			(0x38)
#define		HERMES_EVSTAT			(0x30)
#define		HERMES_INTEN			(0x32)
#define		HERMES_EVACK			(0x34)
#define		HERMES_CONTROL			(0x14)
#define		HERMES_SWSUPPORT0		(0x28)
#define		HERMES_SWSUPPORT1		(0x2A)
#define		HERMES_SWSUPPORT2		(0x2C)
#define		HERMES_AUXPAGE			(0x3A)
#define		HERMES_AUXOFFSET		(0x3C)
#define		HERMES_AUXDATA			(0x3E)

/* 
 * CMD register bitmasks
 */
#define		HERMES_CMD_BUSY			(0x8000)
#define		HERMES_CMD_AINFO		(0x7f00)
#define		HERMES_CMD_MACPORT		(0x0700)
#define		HERMES_CMD_RECL			(0x0100)
#define		HERMES_CMD_WRITE		(0x0100)
#define		HERMES_CMD_PROGMODE		(0x0300)
#define		HERMES_CMD_CMDCODE		(0x003f)

/* 
 * STATUS register bitmasks
 */
#define		HERMES_STATUS_RESULT		(0x7f00)
#define		HERMES_STATUS_CMDCODE		(0x003f)

/* 
 * OFFSET register bitmasks
 */
#define		HERMES_OFFSET_BUSY		(0x8000)
#define		HERMES_OFFSET_ERR		(0x4000)
#define		HERMES_OFFSET_DATAOFF		(0x0ffe)

/* 
 * Event register bitmasks (INTEN, EVSTAT, EVACK)
 */
#define		HERMES_EV_TICK			(0x8000)
#define		HERMES_EV_WTERR			(0x4000)
#define		HERMES_EV_INFDROP		(0x2000)
#define		HERMES_EV_INFO			(0x0080)
#define		HERMES_EV_DTIM			(0x0020)
#define		HERMES_EV_CMD			(0x0010)
#define		HERMES_EV_ALLOC			(0x0008)
#define		HERMES_EV_TXEXC			(0x0004)
#define		HERMES_EV_TX			(0x0002)
#define		HERMES_EV_RX			(0x0001)

/*
 * COR reset options 
 */
#define		HERMES_PCI_COR			(0x26)
#define		HERMES_PCI_COR_MASK		(0x0080)
/* It appears that the card needs quite some time to recover: */
#define		HERMES_PCI_COR_ONT		(250) /* ms */
#define		HERMES_PCI_COR_OFFT		(500) /* ms */
#define		HERMES_PCI_COR_BUSYT		(500) /* ms */
/* 
 * Command codes
 */
/*--- Controller Commands --------------------------*/
#define		HERMES_CMD_INIT			(0x0000)
#define		HERMES_CMD_ENABLE		(0x0001)
#define		HERMES_CMD_DISABLE		(0x0002)
#define		HERMES_CMD_DIAG			(0x0003)

/*--- Buffer Mgmt Commands --------------------------*/
#define		HERMES_CMD_ALLOC		(0x000A)
#define		HERMES_CMD_TX			(0x000B)
#define		HERMES_CMD_CLRPRST		(0x0012)

/*--- Regulate Commands --------------------------*/
#define		HERMES_CMD_NOTIFY		(0x0010)
#define		HERMES_CMD_INQUIRE		(0x0011)

/*--- Configure Commands --------------------------*/
#define		HERMES_CMD_ACCESS		(0x0021)
#define		HERMES_CMD_DOWNLD		(0x0022)

/*--- Debugging Commands -----------------------------*/
#define 	HERMES_CMD_MONITOR		(0x0038)
#define		HERMES_MONITOR_ENABLE		(0x000b)
#define		HERMES_MONITOR_DISABLE		(0x000f)

/* 
 * Frame structures and constants
 */

#define 	HERMES_DESCRIPTOR_OFFSET	(0)
#define 	HERMES_802_11_OFFSET		(14)
#define 	HERMES_802_3_OFFSET		(14+32)
#define 	HERMES_802_2_OFFSET		(14+32+14)

struct hermes_rx_descriptor
{
	u16_t status;
	u16_t time_lefthalf;
	u16_t time_righthalf;
	u8_t silence;
	u8_t signal;
	u8_t rate;
	u8_t rxflow;
	u16_t reserved1;
	u16_t reserved2;
};

#define HERMES_RXSTAT_ERR		(0x0003)
#define	HERMES_RXSTAT_BADCRC		(0x0001)
#define	HERMES_RXSTAT_UNDECRYPTABLE	(0x0002)
#define	HERMES_RXSTAT_MACPORT		(0x0700)
#define HERMES_RXSTAT_PCF		(0x1000)	
#define	HERMES_RXSTAT_MSGTYPE		(0xE000)
#define	HERMES_RXSTAT_1042		(0x2000)	/* RFC-1042 frame */
#define	HERMES_RXSTAT_TUNNEL		(0x4000)	/* bridge-tunnel
							 * encoded frame */
#define	HERMES_RXSTAT_WMP		(0x6000)	/* Wavelan-II
							 * Management
							 * Protocol frame */

struct hermes_tx_descriptor
{
	u16_t status;
	u16_t reserved1;
	u16_t reserved2;
	u16_t sw_support_lefthalf;
	u16_t sw_support_righthalf;
	u8_t retry_count;
	u8_t tx_rate;
	u16_t tx_control;
};

#define HERMES_TXSTAT_RETRYERR		(0x0001)
#define HERMES_TXSTAT_AGEDERR		(0x0002)
#define HERMES_TXSTAT_DISCON		(0x0004)
#define HERMES_TXSTAT_FORMERR		(0x0008)

#define HERMES_TXCTRL_TX_OK		(0x0002)	
#define HERMES_TXCTRL_TX_EX		(0x0004)	
#define HERMES_TXCTRL_802_11		(0x0008)	
#define HERMES_TXCTRL_ALT_RTRY		(0x0020)

/* Inquiry constants and data types */

#define HERMES_INQ_TALLIES		(0xF100)
#define HERMES_INQ_SCAN			(0xF101)
#define HERMES_INQ_LINKSTATUS		(0xF200)


/* The tallies are retrieved, but these fields are not processed until now */
struct hermes_tallies_frame
{
	u16_t TxUnicastFrames;
	u16_t TxMulticastFrames;
	u16_t TxFragments;
	u16_t TxUnicastOctets;
	u16_t TxMulticastOctets;
	u16_t TxDeferredTransmissions;
	u16_t TxSingleRetryFrames;
	u16_t TxMultipleRetryFrames;
	u16_t TxRetryLimitExceeded;
	u16_t TxDiscards;
	u16_t RxUnicastFrames;
	u16_t RxMulticastFrames;
	u16_t RxFragments;
	u16_t RxUnicastOctets;
	u16_t RxMulticastOctets;
	u16_t RxFCSErrors;
	u16_t RxDiscards_NoBuffer;
	u16_t TxDiscardsWrongSA;
	u16_t RxWEPUndecryptable;
	u16_t RxMsgInMsgFragments;
	u16_t RxMsgInBadMsgFragments;
	u16_t RxDiscards_WEPICVError;
	u16_t RxDiscards_WEPExcluded;
};

#define HERMES_LINKSTATUS_NOT_CONNECTED   (0x0000)
#define HERMES_LINKSTATUS_CONNECTED       (0x0001)
#define HERMES_LINKSTATUS_DISCONNECTED    (0x0002)
#define HERMES_LINKSTATUS_AP_CHANGE       (0x0003)
#define HERMES_LINKSTATUS_AP_OUT_OF_RANGE (0x0004)
#define HERMES_LINKSTATUS_AP_IN_RANGE     (0x0005)
#define HERMES_LINKSTATUS_ASSOC_FAILED    (0x0006)

struct hermes_linkstatus
{
	u16_t linkstatus;	/* Link status */
};

/* Timeouts. These are maximum timeouts. Most often, card wil react 
 * much faster */
#define HERMES_BAP_BUSY_TIMEOUT 	(10000)	/* In iterations of ~1us */
#define HERMES_CMD_BUSY_TIMEOUT 	(100)	/* In iterations of ~1us */
#define HERMES_CMD_INIT_TIMEOUT 	(50000)	/* in iterations of ~10us */
#define HERMES_CMD_COMPL_TIMEOUT	(20000)	/* in iterations of ~10us */
#define HERMES_ALLOC_COMPL_TIMEOUT 	(1000)	/* in iterations of ~10us */

/* WEP settings */
#define HERMES_AUTH_OPEN		(1)
#define HERMES_AUTH_SHARED_KEY		(2)
#define HERMES_WEP_PRIVACY_INVOKED	(0x0001)
#define HERMES_WEP_EXCL_UNENCRYPTED	(0x0002)
#define HERMES_WEP_HOST_ENCRYPT		(0x0010)
#define HERMES_WEP_HOST_DECRYPT		(0x0080)


/* Basic control structure */
typedef struct hermes
{
	u32_t iobase;
	int io_space;		/* 1 if we IO-mapped IO, 0 for memory-mapped
				 * IO */
#define HERMES_IO	1
#define HERMES_MEM	0
	int reg_spacing;
#define HERMES_16BIT_REGSPACING	0
#define HERMES_32BIT_REGSPACING	1
	u16_t inten;		/* Which interrupts should be enabled? */
	char *locmem;
} hermes_t;

typedef struct hermes_response
{
	u16_t status, resp0, resp1, resp2;
} hermes_response_t;

struct hermes_idstring
{
	u16_t len;
	u16_t val[16];
};

#define HERMES_BYTES_TO_RECLEN(n) ( (((n)+1)/2) + 1 )
#define HERMES_RECLEN_TO_BYTES(n) ( ((n)-1) * 2 )

/* Function prototypes */
u16_t hermes_read_reg(const hermes_t * hw, u16_t off);
void hermes_write_reg(const hermes_t * hw, u16_t off, u16_t val);
void hermes_struct_init(hermes_t * hw, u32_t address, int io_space, int
	reg_spacing);
int hermes_init(hermes_t * hw);
int hermes_docmd_wait(hermes_t * hw, u16_t cmd, u16_t parm0,
	hermes_response_t * resp);
int hermes_allocate(hermes_t * hw, u16_t size, u16_t * fid);
int hermes_bap_pread(hermes_t * hw, int bap, void *buf, unsigned len,
	u16_t id, u16_t offset);
int hermes_bap_pwrite(hermes_t * hw, int bap, const void *buf, unsigned
	len, u16_t id, u16_t offset);
void hermes_read_words(hermes_t * hw, int off, void *buf, unsigned
	count);
int hermes_read_ltv(hermes_t * hw, int bap, u16_t rid, unsigned buflen,
	u16_t * length, void *buf);
int hermes_write_ltv(hermes_t * hw, int bap, u16_t rid, u16_t length,
	const void *value);
int hermes_set_irqmask(hermes_t * hw, u16_t events);
u16_t hermes_get_irqmask(hermes_t * hw);
int hermes_read_wordrec(hermes_t * hw, int bap, u16_t rid, u16_t *
	word);
int hermes_write_wordrec(hermes_t * hw, int bap, u16_t rid, u16_t word);
int hermes_cor_reset(hermes_t *hw);
#endif /* _HERMES_H */
