/*
ne2000.c

Driver for the ne2000 ethernet cards. This file contains only the ne2000
specific code, the rest is in dp8390.c

Created:	March 15, 1994 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#include <minix/drivers.h>

#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#if __minix_vmd
#include "config.h"
#endif

#include "local.h"
#include "dp8390.h"
#include "ne2000.h"

#if ENABLE_NE2000

#define N 100

extern u32_t system_hz;

#define MILLIS_TO_TICKS(m)  (((m)*system_hz/1000)+1)

typedef int(*testf_t) (dpeth_t *dep, int pos, u8_t *pat);

static u8_t	pat0[]= { 0x00, 0x00, 0x00, 0x00 };
static u8_t	pat1[]= { 0xFF, 0xFF, 0xFF, 0xFF };
static u8_t	pat2[]= { 0xA5, 0x5A, 0x69, 0x96 };
static u8_t	pat3[]= { 0x96, 0x69, 0x5A, 0xA5 };

static int test_8(dpeth_t *dep, int pos, u8_t *pat);
static int test_16(dpeth_t *dep, int pos, u8_t *pat);
static void ne_stop(dpeth_t *dep);
static void milli_delay(unsigned long millis);

/*===========================================================================*
 *				ne_probe				     *
 *===========================================================================*/
int ne_probe(dpeth_t *dep)
{
	int byte;
	int i;
	int loc1, loc2;
	testf_t f;

	dep->de_dp8390_port= dep->de_base_port + NE_DP8390;

	/* We probe for an ne1000 or an ne2000 by testing whether the
	 * on board is reachable through the dp8390. Note that the
	 * ne1000 is an 8bit card and has a memory region distict from
	 * the 16bit ne2000
	 */

	for (dep->de_16bit= 0; dep->de_16bit < 2; dep->de_16bit++)
	{
		/* Reset the ethernet card */
		byte= inb_ne(dep, NE_RESET);
		milli_delay(2);
		outb_ne(dep, NE_RESET, byte);
		milli_delay(2);

		/* Reset the dp8390 */
		outb_reg0(dep, DP_CR, CR_STP | CR_DM_ABORT);
		for (i= 0; i < 0x1000 && ((inb_reg0(dep, DP_ISR) & ISR_RST) == 0); i++)
			; /* Do nothing */

		/* Check if the dp8390 is really there */
		if ((inb_reg0(dep, DP_CR) & (CR_STP|CR_DM_ABORT)) !=
			(CR_STP|CR_DM_ABORT))
		{
			return 0;
		}

		/* Disable the receiver and init TCR and DCR. */
		outb_reg0(dep, DP_RCR, RCR_MON);
		outb_reg0(dep, DP_TCR, TCR_NORMAL);
		if (dep->de_16bit)
		{
			outb_reg0(dep, DP_DCR, DCR_WORDWIDE | DCR_8BYTES |
				DCR_BMS);
		}
		else
		{
			outb_reg0(dep, DP_DCR, DCR_BYTEWIDE | DCR_8BYTES |
				DCR_BMS);
		}

		if (dep->de_16bit)
		{
			loc1= NE2000_START;
			loc2= NE2000_START + NE2000_SIZE - 4;
			f= test_16;
		}
		else
		{
			loc1= NE1000_START;
			loc2= NE1000_START + NE1000_SIZE - 4;
			f= test_8;
		}
		if (f(dep, loc1, pat0) && f(dep, loc1, pat1) &&
			f(dep, loc1, pat2) && f(dep, loc1, pat3) &&
			f(dep, loc2, pat0) && f(dep, loc2, pat1) &&
			f(dep, loc2, pat2) && f(dep, loc2, pat3))
		{
			/* We don't need a memory segment */
			dep->de_linmem= 0;
			if (!dep->de_pci)
				dep->de_initf= ne_init;
			dep->de_stopf= ne_stop;
			dep->de_prog_IO= 1;
			return 1;
		}
	}
	return 0;
}

/*===========================================================================*
 *				ne_init					     *
 *===========================================================================*/
void ne_init(dep)
dpeth_t *dep;
{
	int i;
	int word, sendq_nr;

	/* Setup a transfer to get the ethernet address. */
	if (dep->de_16bit)
		outb_reg0(dep, DP_RBCR0, 6*2);
	else
		outb_reg0(dep, DP_RBCR0, 6);
	outb_reg0(dep, DP_RBCR1, 0);
	outb_reg0(dep, DP_RSAR0, 0);
	outb_reg0(dep, DP_RSAR1, 0);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	for (i= 0; i<6; i++)
	{
		if (dep->de_16bit)
		{
			word= inw_ne(dep, NE_DATA);
			dep->de_address.ea_addr[i]= word;
		}
		else
		{
			dep->de_address.ea_addr[i] = inb_ne(dep, NE_DATA);
		}
	}
	dep->de_data_port= dep->de_base_port + NE_DATA;
	if (dep->de_16bit)
	{
		dep->de_ramsize= NE2000_SIZE;
		dep->de_offset_page= NE2000_START / DP_PAGESIZE;
	}
	else
	{
		dep->de_ramsize= NE1000_SIZE;
		dep->de_offset_page= NE1000_START / DP_PAGESIZE;
	}

	/* Allocate one send buffer (1.5KB) per 8KB of on board memory. */
	sendq_nr= dep->de_ramsize / 0x2000;
	if (sendq_nr < 1)
		sendq_nr= 1;
	else if (sendq_nr > SENDQ_NR)
		sendq_nr= SENDQ_NR;
	dep->de_sendq_nr= sendq_nr;
	for (i= 0; i<sendq_nr; i++)
	{
		dep->de_sendq[i].sq_sendpage= dep->de_offset_page +
			i*SENDQ_PAGES;	
	}

	dep->de_startpage= dep->de_offset_page + i*SENDQ_PAGES;
	dep->de_stoppage= dep->de_offset_page + dep->de_ramsize / DP_PAGESIZE;

	/* Can't override the default IRQ. */
	dep->de_irq &= ~DEI_DEFAULT;

	if (!debug)
	{
		printf("%s: NE%d000 at %X:%d\n",
			dep->de_name, dep->de_16bit ? 2 : 1,
			dep->de_base_port, dep->de_irq);
	}
	else
	{
		printf("%s: Novell NE%d000 ethernet card at I/O address "
			"0x%X, memory size 0x%X, irq %d\n",
			dep->de_name, dep->de_16bit ? 2 : 1,
			dep->de_base_port, dep->de_ramsize, dep->de_irq);
	}
}

/*===========================================================================*
 *				test_8					     *
 *===========================================================================*/
static int test_8(dep, pos, pat)
dpeth_t *dep;
int pos;
u8_t *pat;
{
	u8_t buf[4];
	int i;
	int r;

	outb_reg0(dep, DP_ISR, 0xFF);

	/* Setup a transfer to put the pattern. */
	outb_reg0(dep, DP_RBCR0, 4);
	outb_reg0(dep, DP_RBCR1, 0);
	outb_reg0(dep, DP_RSAR0, pos & 0xFF);
	outb_reg0(dep, DP_RSAR1, pos >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RW | CR_PS_P0 | CR_STA);

	for (i= 0; i<4; i++)
		outb_ne(dep, NE_DATA, pat[i]);

	for (i= 0; i<N; i++)
	{
		if (inb_reg0(dep, DP_ISR) & ISR_RDC)
			break;
	}
	if (i == N)
	{
		if (debug)
		{
			printf("%s: NE1000 remote DMA test failed\n",
				dep->de_name);
		}
		return 0;
	}

	outb_reg0(dep, DP_RBCR0, 4);
	outb_reg0(dep, DP_RBCR1, 0);
	outb_reg0(dep, DP_RSAR0, pos & 0xFF);
	outb_reg0(dep, DP_RSAR1, pos >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	for (i= 0; i<4; i++)
		buf[i]= inb_ne(dep, NE_DATA);

	r= (memcmp(buf, pat, 4) == 0);
	return r;
}

/*===========================================================================*
 *				test_16					     *
 *===========================================================================*/
static int test_16(dep, pos, pat)
dpeth_t *dep;
int pos;
u8_t *pat;
{
	u8_t buf[4];
	int i;
	int r;

	outb_reg0(dep, DP_ISR, 0xFF);

	/* Setup a transfer to put the pattern. */
	outb_reg0(dep, DP_RBCR0, 4);
	outb_reg0(dep, DP_RBCR1, 0);
	outb_reg0(dep, DP_RSAR0, pos & 0xFF);
	outb_reg0(dep, DP_RSAR1, pos >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RW | CR_PS_P0 | CR_STA);

	for (i= 0; i<4; i += 2)
	{
		outw_ne(dep, NE_DATA, *(u16_t *)(pat+i));
	}

	for (i= 0; i<N; i++)
	{
		if (inb_reg0(dep, DP_ISR) & ISR_RDC)
			break;
	}
	if (i == N)
	{
		if (debug)
		{
			printf("%s: NE2000 remote DMA test failed\n",
				dep->de_name);
		}
		return 0;
	}

	outb_reg0(dep, DP_RBCR0, 4);
	outb_reg0(dep, DP_RBCR1, 0);
	outb_reg0(dep, DP_RSAR0, pos & 0xFF);
	outb_reg0(dep, DP_RSAR1, pos >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	for (i= 0; i<4; i += 2)
	{
		*(u16_t *)(buf+i)= inw_ne(dep, NE_DATA);
	}

	r= (memcmp(buf, pat, 4) == 0);
	return r;
}

/*===========================================================================*
 *				ne_stop					     *
 *===========================================================================*/
static void ne_stop(dep)
dpeth_t *dep;
{
	int byte;

	/* Reset the ethernet card */
	byte= inb_ne(dep, NE_RESET);
	milli_delay(2);
	outb_ne(dep, NE_RESET, byte);
}

static void milli_delay(unsigned long millis)
{
	tickdelay(MILLIS_TO_TICKS(millis));
}

#endif /* ENABLE_NE2000 */

/*
 * $PchId: ne2000.c,v 1.10 2004/08/03 12:03:00 philip Exp $
 */
