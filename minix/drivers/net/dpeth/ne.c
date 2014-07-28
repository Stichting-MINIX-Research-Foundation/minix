/*
**  File:	ne.c	Jun. 08, 2000
**
**  Driver for the NE*000 ethernet cards and derivates.
**  This file contains only the ne specific code,
**  the rest is in 8390.c   Code specific for ISA bus only
**
**  Created:	March 15, 1994 by Philip Homburg <philip@cs.vu.nl>
**  PchId: 	ne2000.c,v 1.4 1996/01/19 23:30:34 philip Exp
**
**  Modified:	Jun. 08, 2000 by Giovanni Falzoni <gfalzoni@inwind.it>
**  		Adapted to interface new main network task.
*/

#include <minix/drivers.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include "dp.h"

#if (ENABLE_NE2000 == 1)

#include "8390.h"
#include "ne.h"

/*
**  Name:	void ne_reset(dpeth_t * dep);
**  Function:	Resets the board and checks if reset cycle completes
*/
static int ne_reset(dpeth_t * dep)
{
  int count = 0;

  /* Reset the ethernet card */
  outb_ne(dep, NE_RESET, inb_ne(dep, NE_RESET));
  do {
	if (++count > 10) return FALSE;	/* 20 mSecs. timeout */
	milli_delay(2);
  } while ((inb_ne(dep, DP_ISR) & ISR_RST) == 0);
  return TRUE;
}

/*
**  Name:	void ne_close(dpeth_t * dep);
**  Function:	Stops the board by resetting it and masking interrupts.
*/
static void ne_close(dpeth_t * dep)
{

  (void)ne_reset(dep);
  outb_ne(dep, DP_ISR, 0xFF);
  sys_irqdisable(&dep->de_hook);
  return;
}

/* 
**  Name:	void ne_init(dpeth_t * dep);
**  Function:	Initialize the board making it ready to work.
*/
static void ne_init(dpeth_t * dep)
{
  int ix;

  dep->de_data_port = dep->de_base_port + NE_DATA;
  if (dep->de_16bit) {
	dep->de_ramsize = NE2000_SIZE;
	dep->de_offset_page = NE2000_START / DP_PAGESIZE;
  } else {
	dep->de_ramsize = NE1000_SIZE;
	dep->de_offset_page = NE1000_START / DP_PAGESIZE;
  }

  /* Allocates two send buffers from onboard RAM */
  dep->de_sendq_nr = SENDQ_NR;
  for (ix = 0; ix < SENDQ_NR; ix += 1) {
	dep->de_sendq[ix].sq_sendpage = dep->de_offset_page + ix * SENDQ_PAGES;
  }

  /* Remaining onboard RAM allocated for receiving */
  dep->de_startpage = dep->de_offset_page + ix * SENDQ_PAGES;
  dep->de_stoppage = dep->de_offset_page + dep->de_ramsize / DP_PAGESIZE;

  /* Can't override the default IRQ. */
  dep->de_irq &= NOT(DEI_DEFAULT);

  ns_init(dep);			/* Initialize DP controller */

  printf("%s: NE%d000 (%dkB RAM) at %X:%d - ",
         dep->de_name,
         dep->de_16bit ? 2 : 1,
         dep->de_ramsize / 1024,
         dep->de_base_port, dep->de_irq);
  for (ix = 0; ix < SA_ADDR_LEN; ix += 1)
	printf("%02X%c", dep->de_address.ea_addr[ix], ix < SA_ADDR_LEN - 1 ? ':' : '\n');
  return;
}

/*
**  Name:	int ne_probe(dpeth_t * dep);
**  Function:	Probe for the presence of a NE*000 card by testing
**  		whether the board is reachable through the dp8390.
**  		Note that the NE1000 is an 8bit card and has a memory
**  		region distict from the 16bit NE2000.
*/
int ne_probe(dpeth_t * dep)
{
  int ix, wd, loc1, loc2;
  char EPROM[32];
  static const struct {
	unsigned char offset;
	unsigned char value;
  } InitSeq[] =
  {
	{	/* Selects page 0. */
		DP_CR, (CR_NO_DMA | CR_PS_P0 | CR_STP),
	},
	{	/* Set byte-wide access and 8 bytes burst mode. */
		DP_DCR, (DCR_8BYTES | DCR_BMS), 
	},
	{	/* Clears the count registers. */
		DP_RBCR0, 0x00, }, {	DP_RBCR1, 0x00,
	},
	{	/* Mask completion irq. */
		DP_IMR, 0x00, }, {	DP_ISR, 0xFF,
	},
	{	/* Set receiver to monitor */
		DP_RCR, RCR_MON, 
	},
	{	/* and transmitter to loopback mode. */
		DP_TCR, TCR_INTERNAL, 
	},
	{	/* Transmit 32 bytes */
		DP_RBCR0, 32, }, { DP_RBCR1, 0,
	},
	{	/* DMA starting at 0x0000. */
		DP_RSAR0, 0x00, }, { DP_RSAR1, 0x00,
	},
	{			/* Start board (reads) */
		DP_CR, (CR_PS_P0 | CR_DM_RR | CR_STA),
	},
  };

  dep->de_dp8390_port = dep->de_base_port + NE_DP8390;

  if ((loc1 = inb_ne(dep, NE_DP8390)) == 0xFF) return FALSE;

  /* Check if the dp8390 is really there */
  outb_ne(dep, DP_CR, CR_STP | CR_NO_DMA | CR_PS_P1);
  loc2 = inb_ne(dep, DP_MAR5);	/* Saves one byte of the address */
  outb_ne(dep, DP_MAR5, 0xFF);	/* Write 0xFF to it (same offset as DP_CNTR0) */
  outb_ne(dep, DP_CR, CR_NO_DMA | CR_PS_P0);	/* Back to page 0 */
  inb_ne(dep, DP_CNTR0);	/* Reading counter resets it */
  if (inb_ne(dep, DP_CNTR0) != 0) {
	outb_ne(dep, NE_DP8390, loc1);	/* Try to restore modified bytes */
	outb_ne(dep, DP_TCR, loc2);
	return FALSE;
  }

  /* Try to reset the board */
  if (ne_reset(dep) == FALSE) return FALSE;

  /* Checks whether the board is 8/16bits and a real NE*000 or clone */
  for (ix = 0; ix < sizeof(InitSeq)/sizeof(InitSeq[0]); ix += 1) {
	outb_ne(dep, InitSeq[ix].offset, InitSeq[ix].value);
  }
  for (ix = 0, wd = 1; ix < 32; ix += 2) {
	EPROM[ix + 0] = inb_ne(dep, NE_DATA);
	EPROM[ix + 1] = inb_ne(dep, NE_DATA);
	/* NE2000s and clones read same value for even and odd addresses */
	if (EPROM[ix + 0] != EPROM[ix + 1]) wd = 0;
  }
  if (wd == 1) {	/* Normalize EPROM contents for NE2000 */
	for (ix = 0; ix < 16; ix += 1) EPROM[ix] = EPROM[ix * 2];
  }
  /* Real NE*000 and good clones have '0x57' at locations 14 and 15 */
  if (EPROM[14] != 0x57 || EPROM[15] != 0x57) return FALSE;

  /* Setup the ethernet address. */
  for (ix = 0; ix < SA_ADDR_LEN; ix += 1) {
	dep->de_address.ea_addr[ix] = EPROM[ix];
  }
  dep->de_16bit = wd;
  dep->de_linmem = 0;		/* Uses Programmed I/O only */
  dep->de_prog_IO = 1;
  dep->de_initf = ne_init;
  dep->de_stopf = ne_close;
  return TRUE;
}

#endif				/* ENABLE_NE2000 */

/** ne.c **/
