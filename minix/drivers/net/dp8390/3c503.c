/*
 *	3c503.c		A shared memory driver for Etherlink II board.
 *
 *	Created:	Dec. 20, 1996 by G. Falzoni <falzoni@marina.scn.de>
 *
 *	Inspired by the TNET package by M. Ostrowski, the driver for Linux
 *	by D. Becker, the Crynwr 3c503 packet driver, and the Amoeba driver.
 *
 *	It works in shared memory mode and should be used with the
 *	device driver for NS 8390 based cards of Minix.  Programmed
 *	I/O could be used as well but would result in poor performance.
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include "local.h"
#include "dp8390.h"
#include "3c503.h"

#if ENABLE_3C503

extern u32_t system_hz;

#define MILLIS_TO_TICKS(m)  (((m)*system_hz/1000)+1)

static void el2_init(dpeth_t *dep);
static void el2_stop(dpeth_t *dep);

/*===========================================================================*
 *				el2_init				     *
 *===========================================================================*/
static void el2_init(dep)
dpeth_t * dep;
{
  /* Initalize hardware and data structures. */
  int ix, irq;
  int sendq_nr;
  int cntr;

  /* Map the address PROM to lower I/O address range */
  cntr = inb_el2(dep, EL2_CNTR);
  outb_el2(dep, EL2_CNTR, cntr | ECNTR_SAPROM);

  /* Read station address from PROM */
  for (ix = EL2_EA0; ix <= EL2_EA5; ix += 1)
	dep->de_address.na_addr[ix] = inb_el2(dep, ix);

  /* Map the 8390 back to lower I/O address range */
  outb_el2(dep, EL2_CNTR, cntr);

  /* Enable memory, but turn off interrupts until we are ready */
  outb_el2(dep, EL2_CFGR, ECFGR_IRQOFF);

  dep->de_data_port = dep->de_dp8390_port = dep->de_base_port;
  dep->de_prog_IO = 0;		/* Programmed I/O not yet available */

  /* Check width of data bus:
   * 1. Write 0 to WTS bit.  The board will drive it to 1 if it is a
   *    16-bit card.
   * 2. Select page 2
   * 3. See if it is a 16-bit card
   * 4. Select page 0
   */
  outb_el2(dep, DP_CR, CR_PS_P0|CR_DM_ABORT|CR_STP);
  outb_el2(dep, DP_DCR, 0);
  outb_el2(dep, DP_CR, CR_PS_P2|CR_DM_ABORT|CR_STP);
  dep->de_16bit = (inb_el2(dep, DP_DCR) & DCR_WTS) != 0;
  outb_el2(dep, DP_CR, CR_PS_P0|CR_DM_ABORT|CR_STP);

  /* Allocate one send buffer (1.5KB) per 8KB of on board memory. */
  sendq_nr = (dep->de_ramsize - dep->de_offset_page) / 0x2000;
  if (sendq_nr < 1)
	sendq_nr = 1;
  else if (sendq_nr > SENDQ_NR)
	sendq_nr = SENDQ_NR;

  dep->de_sendq_nr = sendq_nr;
  for (ix = 0; ix < sendq_nr; ix++)
	dep->de_sendq[ix].sq_sendpage = (ix * SENDQ_PAGES) + EL2_SM_START_PG;

  dep->de_startpage = (ix * SENDQ_PAGES) + EL2_SM_START_PG;
  dep->de_stoppage = EL2_SM_STOP_PG;

  outb_el2(dep, EL2_STARTPG, dep->de_startpage);
  outb_el2(dep, EL2_STOPPG, dep->de_stoppage);

  /* Point the vector pointer registers somewhere ?harmless?. */
  outb_el2(dep, EL2_VP2, 0xFF);	/* Point at the ROM restart location    */
  outb_el2(dep, EL2_VP1, 0xFF);	/* 0xFFFF:0000  (from original sources) */
  outb_el2(dep, EL2_VP0, 0x00);	/*           - What for protected mode? */

  /* Set interrupt level for 3c503 */
  irq = (dep->de_irq &= ~DEI_DEFAULT);	/* Strip the default flag. */
  if (irq == 9) irq = 2;
  if (irq < 2 || irq > 5) panic("bad 3c503 irq configuration: %d", irq);
  outb_el2(dep, EL2_IDCFG, (0x04 << irq));

  outb_el2(dep, EL2_DRQCNT, 0x08);	/* Set burst size to 8 */
  outb_el2(dep, EL2_DMAAH, EL2_SM_START_PG);	/* Put start of TX  */
  outb_el2(dep, EL2_DMAAL, 0x00);	/* buffer in the GA DMA reg */

  outb_el2(dep, EL2_CFGR, ECFGR_NORM);	/* Enable shared memory */

  if (!debug) {
	printf("%s: 3c503 at %X:%d:%lX\n",
		netdriver_name(), dep->de_base_port, dep->de_irq,
		dep->de_linmem + dep->de_offset_page);
  } else {
	printf("%s: 3Com Etherlink II %sat I/O address 0x%X, "
			"memory address 0x%lX, irq %d\n",
		netdriver_name(), dep->de_16bit ? "(16-bit) " : "",
		dep->de_base_port,
		dep->de_linmem + dep->de_offset_page,
		dep->de_irq);
  }
}

/*===========================================================================*
 *				el2_stop				     *
 *===========================================================================*/
static void el2_stop(dep)
dpeth_t * dep;
{
  /* Stops board by disabling interrupts. */

#if DEBUG
  printf("%s: stopping Etherlink\n", netdriver_name());
#endif
  outb_el2(dep, EL2_CFGR, ECFGR_IRQOFF);
  return;
}

/*===========================================================================*
 *				el2_probe				     *
 *===========================================================================*/
int el2_probe(dep)
dpeth_t * dep;
{
  /* Probe for the presence of an EtherLink II card.  Initialize memory
   * addressing if card detected.
   */
  int iobase, membase;
  int thin;

  /* Thin ethernet or AUI? */
  thin = (dep->de_linmem & 1) ? ECNTR_AUI : ECNTR_THIN;

  /* Location registers should have 1 bit set */
  if (!(iobase = inb_el2(dep, EL2_IOBASE))) return 0;
  if (!((membase = inb_el2(dep, EL2_MEMBASE)) & 0xF0)) return 0;
  if ((iobase & (iobase - 1)) || (membase & (membase - 1))) return 0;

  /* Resets board */
  outb_el2(dep, EL2_CNTR, ECNTR_RESET | thin);
  micro_delay(1000);
  outb_el2(dep, EL2_CNTR, thin);
  micro_delay(5000);

  /* Map the address PROM to lower I/O address range */
  outb_el2(dep, EL2_CNTR, ECNTR_SAPROM | thin);
  if (inb_el2(dep, EL2_EA0) != 0x02 ||	/* Etherlink II Station address */
      inb_el2(dep, EL2_EA1) != 0x60 ||	/* MUST be 02:60:8c:xx:xx:xx */
      inb_el2(dep, EL2_EA2) != 0x8C)
	return 0;		/* No Etherlink board at this address */

  /* Map the 8390 back to lower I/O address range */
  outb_el2(dep, EL2_CNTR, thin);

  /* Setup shared memory addressing for 3c503 */
  dep->de_linmem = ((membase & 0xC0) ? EL2_BASE_0D8000 : EL2_BASE_0C8000) +
	((membase & 0xA0) ? (EL2_BASE_0CC000 - EL2_BASE_0C8000) : 0x0000);
  dep->de_offset_page = (EL2_SM_START_PG * DP_PAGESIZE);
  dep->de_ramsize = (EL2_SM_STOP_PG - EL2_SM_START_PG) * DP_PAGESIZE;

  /* (Bad kludge, something Philip needs to look into. -- kjb) */
  dep->de_linmem -= dep->de_offset_page;
  dep->de_ramsize += dep->de_offset_page;

  /* Board initialization and stop functions */
  dep->de_initf = el2_init;
  dep->de_stopf = el2_stop;
  return 1;
}

#endif /* ENABLE_3C503 */

/** 3c503.c **/

/*
 * $PchId: 3c503.c,v 1.3 2003/09/10 15:33:04 philip Exp $
 */
