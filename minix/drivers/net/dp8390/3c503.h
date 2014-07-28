/*
 *	3c503.h		A shared memory driver for Etherlink II board.
 *
 *	Created:	Dec. 20, 1996 by G. Falzoni <falzoni@marina.scn.de>
 */

#define EL2_MEMTEST	0	/* Set to 1 for on board memory test */

#define EL2_GA		0x0400	/* Offset of registers in Gate Array */

/* EtherLink II card */

#define EL2_STARTPG	(EL2_GA+0x00)	/* Start page matching DP_PSTARTPG */
#define EL2_STOPPG	(EL2_GA+0x01)	/* Stop page matching DP_PSTOPPG   */
#define EL2_DRQCNT	(EL2_GA+0x02)	/* DMA burst count                 */
#define EL2_IOBASE	(EL2_GA+0x03)	/* I/O base jumpers (bit coded)    */
#define EL2_MEMBASE	(EL2_GA+0x04)	/* Memory base jumpers (bit coded) */
#define EL2_CFGR	(EL2_GA+0x05)	/* Configuration Register  for GA  */
#define EL2_CNTR	(EL2_GA+0x06)	/* Control(write) and status(read) */
#define EL2_STATUS	(EL2_GA+0x07)
#define EL2_IDCFG	(EL2_GA+0x08)	/* Interrupt/DMA configuration reg */
#define EL2_DMAAH	(EL2_GA+0x09)	/* DMA address register (High byte) */
#define EL2_DMAAL	(EL2_GA+0x0A)	/* DMA address register (Low byte) */
#define EL2_VP2		(EL2_GA+0x0B)	/* Vector pointer - set to         */
#define EL2_VP1		(EL2_GA+0x0C)	/* reset address (0xFFFF:0)  */
#define EL2_VP0		(EL2_GA+0x0D)	/* */
#define EL2_FIFOH	(EL2_GA+0x0E)	/* FIFO for progr. I/O (High byte) */
#define EL2_FIFOL	(EL2_GA+0x0F)	/* FIFO for progr. I/O (Low byte)  */

#define EL2_EA0		0x00	/* Most significant byte of ethernet address */
#define EL2_EA1		0x01
#define EL2_EA2		0x02
#define EL2_EA3		0x03
#define EL2_EA4		0x04
#define EL2_EA5		0x05	/* Least significant byte of ethernet address */

/* Bits in EL2_CNTR register */
#define ECNTR_RESET	0x01	/* Software Reset */
#define ECNTR_THIN	0x02	/* Onboard transceiver enable */
#define ECNTR_AUI	0x00	/* Onboard transceiver disable */
#define ECNTR_SAPROM	0x04	/* Map the station address prom */

/* Bits in EL2_CFGR register */
#define ECFGR_NORM	0x49	/* Enable 8k shared memory, no DMA, TC int */
#define ECFGR_IRQOFF	0xC9	/* As above, disable 8390 IRQ */

/* Shared memory management parameters */
#define EL2_SM_START_PG	0x20	/* First page of TX buffer */
#define EL2_SM_STOP_PG	0x40	/* Last page +1 of RX ring */

/* Physical addresses where an Etherlink board can be configured */
#define EL2_BASE_0C8000	0x0C8000
#define EL2_BASE_0CC000	0x0CC000
#define EL2_BASE_0D8000	0x0D8000
#define EL2_BASE_0DC000	0x0DC000

#define inb_el2(dep,reg)	(inb((dep)->de_base_port+(reg)))
#define outb_el2(dep,reg,data)	(outb((dep)->de_base_port+(reg),(data)))

/** 3c503.h **/

/*
 * $PchId: 3c503.h,v 1.3 2003/09/10 15:34:29 philip Exp $
 */
