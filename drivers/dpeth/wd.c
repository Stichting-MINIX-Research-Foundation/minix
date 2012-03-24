/*
**  File:	wd.c
**
**  Driver for the Ethercard (WD80x3) and derivates
**  This file contains only the wd80x3 specific code,
**  the rest is in 8390.c
**
**  Created:	March 14, 1994 by Philip Homburg
**  PchId:
**
**  Modified: Jun. 08, 2000 by Giovanni Falzoni <gfalzoni@inwind.it>
**  	Adaptation to interfacing new main network task.
*/

#include <minix/drivers.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include "dp.h"

#if (ENABLE_WDETH == 1)

#include "8390.h"
#include "wd.h"

#define WET_ETHERNET	0x01	/* Ethernet transceiver */
#define WET_STARLAN	0x02	/* Starlan transceiver */
#define WET_INTERF_CHIP	0x04	/* has a WD83C583 interface chip */
#define WET_BRD_16BIT	0x08	/* 16 bit board */
#define WET_SLT_16BIT	0x10	/* 16 bit slot */
#define WET_790		0x20	/* '790 chip */

static const int we_int_table[8] = {9, 3, 5, 7, 10, 11, 15, 4};
static const int we_790int_table[8] = {0, 9, 3, 5, 7, 10, 11, 15};

static void we_init(dpeth_t * dep);
static void we_stop(dpeth_t * dep);
static int we_aliasing(dpeth_t * dep);
static int we_interface_chip(dpeth_t * dep);
static int we_16bitboard(dpeth_t * dep);
static int we_16bitslot(dpeth_t * dep);
static int we_ultra(dpeth_t * dep);

/*===========================================================================*
 *				wdeth_probe				     *
 *===========================================================================*/
int wdeth_probe(dep)
dpeth_t *dep;
{
  int sum;

  if (dep->de_linmem == 0)
	return FALSE;		/* No shared memory, so no WD board */

  sum = inb_we(dep, EPL_EA0) + inb_we(dep, EPL_EA1) +
	inb_we(dep, EPL_EA2) + inb_we(dep, EPL_EA3) +
	inb_we(dep, EPL_EA4) + inb_we(dep, EPL_EA5) +
	inb_we(dep, EPL_TLB) + inb_we(dep, EPL_CHKSUM);
  if ((sum & 0xFF) != 0xFF)
	return FALSE;		/* No ethernet board at this address */

  dep->de_initf = we_init;
  dep->de_stopf = we_stop;
  dep->de_prog_IO = 0;
  return TRUE;
}

/*===========================================================================*
 *				we_init					     *
 *===========================================================================*/
static void we_init(dep)
dpeth_t *dep;
{
  int i, int_indx, int_nr;
  int tlb, rambit, revision;
  int icr, irr, hwr, b, gcr;
  int we_type;
  int sendq_nr;

  for (i = 0; i < 6; i += 1) {
	dep->de_address.ea_addr[i] = inb_we(dep, EPL_EA0 + i);
  }

  dep->de_dp8390_port = dep->de_base_port + EPL_DP8390;

  dep->de_16bit = 0;

  we_type = 0;
  we_type |= WET_ETHERNET;	/* assume ethernet */
  if (we_ultra(dep)) we_type |= WET_790;
  if (!we_aliasing(dep)) {
	if (we_interface_chip(dep)) we_type |= WET_INTERF_CHIP;
	if (we_16bitboard(dep)) {
		we_type |= WET_BRD_16BIT;
		if (we_16bitslot(dep)) we_type |= WET_SLT_16BIT;
	}
  }
  if (we_type & WET_SLT_16BIT) dep->de_16bit = 1;

  /* Look at the on board ram size. */
  tlb = inb_we(dep, EPL_TLB);
  revision = tlb & E_TLB_REV;
  rambit = tlb & E_TLB_RAM;

  if (dep->de_ramsize != 0) {
	/* Size set from boot environment. */
  } else if (revision < 2) {
	dep->de_ramsize = 0x2000;	/* 8K */
	if (we_type & WET_BRD_16BIT)
		dep->de_ramsize = 0x4000;	/* 16K */
	else if ((we_type & WET_INTERF_CHIP) &&
		 inb_we(dep, EPL_ICR) & E_ICR_MEMBIT) {
		dep->de_ramsize = 0x8000;	/* 32K */
	}
  } else {
	if (we_type & WET_BRD_16BIT) {
		/* 32K or 16K */
		dep->de_ramsize = rambit ? 0x8000 : 0x4000;
	} else {
		/* 32K or 8K */
		dep->de_ramsize = rambit ? 0x8000 : 0x2000;
	}
  }

  if (we_type & WET_790) {
	outb_we(dep, EPL_MSR, E_MSR_RESET);
	if ((we_type & (WET_BRD_16BIT | WET_SLT_16BIT)) ==
	    (WET_BRD_16BIT | WET_SLT_16BIT)) {
		outb_we(dep, EPL_LAAR, E_LAAR_LAN16E | E_LAAR_MEM16E);
	}
  } else if (we_type & WET_BRD_16BIT) {
	if (we_type & WET_SLT_16BIT) {
		outb_we(dep, EPL_LAAR, E_LAAR_A19 | E_LAAR_SOFTINT |
			E_LAAR_LAN16E | E_LAAR_MEM16E);
	} else {
		outb_we(dep, EPL_LAAR, E_LAAR_A19 | E_LAAR_SOFTINT |
			E_LAAR_LAN16E);
	}
  }
  if (we_type & WET_790) {
	outb_we(dep, EPL_MSR, E_MSR_MENABLE);
	hwr = inb_we(dep, EPL_790_HWR);
	outb_we(dep, EPL_790_HWR, hwr | E_790_HWR_SWH);
	b = inb_we(dep, EPL_790_B);
	outb_we(dep, EPL_790_B, ((dep->de_linmem >> 13) & 0x0f) |
		((dep->de_linmem >> 11) & 0x40) | (b & 0xb0));
	outb_we(dep, EPL_790_HWR, hwr & ~E_790_HWR_SWH);
  } else {
	outb_we(dep, EPL_MSR, E_MSR_RESET);
	outb_we(dep, EPL_MSR, E_MSR_MENABLE |
		((dep->de_linmem >> 13) & E_MSR_MEMADDR));
  }

  if ((we_type & WET_INTERF_CHIP) && !(we_type & WET_790)) {
	icr = inb_we(dep, EPL_ICR);
	irr = inb_we(dep, EPL_IRR);
	int_indx = (icr & E_ICR_IR2) | ((irr & (E_IRR_IR0 | E_IRR_IR1)) >> 5);
	int_nr = we_int_table[int_indx];
	DEBUG(printf("%s: encoded irq= %d\n", dep->de_name, int_nr));
	if (dep->de_irq & DEI_DEFAULT) dep->de_irq = int_nr;
	outb_we(dep, EPL_IRR, irr | E_IRR_IEN);
  }
  if (we_type & WET_790) {
	hwr = inb_we(dep, EPL_790_HWR);
	outb_we(dep, EPL_790_HWR, hwr | E_790_HWR_SWH);

	gcr = inb_we(dep, EPL_790_GCR);

	outb_we(dep, EPL_790_HWR, hwr & ~E_790_HWR_SWH);

	int_indx = ((gcr & E_790_GCR_IR2) >> 4) |
		((gcr & (E_790_GCR_IR1 | E_790_GCR_IR0)) >> 2);
	int_nr = we_790int_table[int_indx];
	DEBUG(printf("%s: encoded irq= %d\n", dep->de_name, int_nr));
	if (dep->de_irq & DEI_DEFAULT) dep->de_irq = int_nr;
	icr = inb_we(dep, EPL_790_ICR);
	outb_we(dep, EPL_790_ICR, icr | E_790_ICR_EIL);
  }

  /* Strip the "default flag." */
  dep->de_irq &= ~DEI_DEFAULT;

  dep->de_offset_page = 0;	/* Shared memory starts at 0 */
  /* Allocate one send buffer (1.5KB) per 8KB of on board memory.
  sendq_nr = dep->de_ramsize / 0x2000;
  if (sendq_nr < 1)
	sendq_nr = 1;
  else if (sendq_nr > SENDQ_NR) */
	sendq_nr = SENDQ_NR;
  dep->de_sendq_nr = sendq_nr;
  for (i = 0; i < sendq_nr; i++) {
	dep->de_sendq[i].sq_sendpage = i * SENDQ_PAGES;
  }
  dep->de_startpage = i * SENDQ_PAGES;
  dep->de_stoppage = dep->de_ramsize / DP_PAGESIZE;

  ns_init(dep);			/* Initialize DP controller */

  printf("%s: WD80%d3 (%dkB RAM) at %X:%d:%lX - ",
         dep->de_name,
         we_type & WET_BRD_16BIT ? 1 : 0,
         dep->de_ramsize / 1024,
         dep->de_base_port,
         dep->de_irq,
         dep->de_linmem);
  for (i = 0; i < SA_ADDR_LEN; i += 1)
	printf("%02X%c", dep->de_address.ea_addr[i],
	       i < SA_ADDR_LEN - 1 ? ':' : '\n');

  return;
}

/*===========================================================================*
 *				we_stop					     *
 *===========================================================================*/
static void we_stop(dep)
dpeth_t *dep;
{

  if (dep->de_16bit) outb_we(dep, EPL_LAAR, E_LAAR_A19 | E_LAAR_LAN16E);
  outb_we(dep, EPL_MSR, E_MSR_RESET);
  outb_we(dep, EPL_MSR, 0);
  sys_irqdisable(&dep->de_hook);
  return;
}

/*===========================================================================*
 *				we_aliasing				     *
 *===========================================================================*/
static int we_aliasing(dep)
dpeth_t *dep;
{
/* Determine whether wd8003 hardware performs register aliasing. This implies
 * an old WD8003E board. */

  if (inb_we(dep, EPL_REG1) != inb_we(dep, EPL_EA1)) return 0;
  if (inb_we(dep, EPL_REG2) != inb_we(dep, EPL_EA2)) return 0;
  if (inb_we(dep, EPL_REG3) != inb_we(dep, EPL_EA3)) return 0;
  if (inb_we(dep, EPL_REG4) != inb_we(dep, EPL_EA4)) return 0;
  if (inb_we(dep, EPL_REG7) != inb_we(dep, EPL_CHKSUM)) return 0;
  return 1;
}

/*===========================================================================*
 *				we_interface_chip			     *
 *===========================================================================*/
static int we_interface_chip(dep)
dpeth_t *dep;
{
/* Determine if the board has an interface chip. */

  outb_we(dep, EPL_GP2, 0x35);
  if (inb_we(dep, EPL_GP2) != 0x35) return 0;
  outb_we(dep, EPL_GP2, 0x3A);
  if (inb_we(dep, EPL_GP2) != 0x3A) return 0;
  return 1;
}

/*===========================================================================*
 *				we_16bitboard				     *
 *===========================================================================*/
static int we_16bitboard(dep)
dpeth_t *dep;
{
/* Determine whether the board is capable of doing 16 bit memory moves.
 * If the 16 bit enable bit is unchangable by software we'll assume an
 * 8 bit board.
 */
  int icr;
  u8_t tlb;

  icr = inb_we(dep, EPL_ICR);

  outb_we(dep, EPL_ICR, icr ^ E_ICR_16BIT);
  if (inb_we(dep, EPL_ICR) == icr) {
	tlb = inb_we(dep, EPL_TLB);

	DEBUG(printf("%s: tlb= 0x%x\n", dep->de_name, tlb));

	return tlb == E_TLB_EB || tlb == E_TLB_E ||
		tlb == E_TLB_SMCE || tlb == E_TLB_SMC8216C;
  }
  outb_we(dep, EPL_ICR, icr);
  return 1;
}

/*===========================================================================*
 *				we_16bitslot				     *
 *===========================================================================*/
static int we_16bitslot(dep)
dpeth_t *dep;
{
/* Determine if the 16 bit board in plugged into a 16 bit slot.  */

  return !!(inb_we(dep, EPL_ICR) & E_ICR_16BIT);
}

/*===========================================================================*
 *				we_ultra				     *
 *===========================================================================*/
static int we_ultra(dep)
dpeth_t *dep;
{
/* Determine if we has an '790 chip.  */
  u8_t tlb;

  tlb = inb_we(dep, EPL_TLB);
  return tlb == E_TLB_SMC8216C;
}

#endif				/* ENABLE_WDETH */

/** wd.c **/
