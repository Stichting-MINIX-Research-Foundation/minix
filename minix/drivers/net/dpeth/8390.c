/*
**  File:	8390.c		May  02, 2000
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  This file contains an ethernet device driver for NICs
**  equipped with the National Semiconductor NS 8390 chip.
**  It has to be associated with the board specific driver.
**  Rewritten from Minix 2.0.0 ethernet driver dp8390.c
**  to extract the NS 8390 common functions.
*/

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <assert.h>
#include "dp.h"

#if (ENABLE_DP8390 == 1)

#include "8390.h"

/*
**  Name:	ns_rw_setup
**  Function:	Sets the board for reading/writing.
*/
static void ns_rw_setup(const dpeth_t *dep, int mode, int size, u16_t offset)
{

  if (mode == CR_DM_RW) outb_reg0(dep, DP_ISR, ISR_RDC);
  outb_reg0(dep, DP_RBCR0, size & 0xFF);
  outb_reg0(dep, DP_RBCR1, (size >> 8) & 0xFF);
  outb_reg0(dep, DP_RSAR0, offset & 0xFF);
  outb_reg0(dep, DP_RSAR1, (offset >> 8) & 0xFF);
  mode |= (CR_PS_P0 | CR_STA);
  outb_reg0(dep, DP_CR, mode);
}

/*
**  Name:	ns_start_xmit
**  Function:	Sets the board for for transmitting and fires it.
*/
static void ns_start_xmit(const dpeth_t * dep, int size, int pageno)
{

  outb_reg0(dep, DP_TPSR, pageno);
  outb_reg0(dep, DP_TBCR1, size >> 8);
  outb_reg0(dep, DP_TBCR0, size & 0xFF);
  outb_reg0(dep, DP_CR, CR_NO_DMA | CR_STA | CR_TXP);	/* Fires transmission */
}

/*
**  Name:	mem_getblock
**  Function:	Reads a block of packet from board (shared memory).
*/
static void mem_getblock(dpeth_t *dep, u16_t offset, int size, void *dst)
{

  assert(size >= 0);
  assert(offset + (unsigned int)size <= dep->de_ramsize);

  memcpy(dst, dep->de_locmem + offset, size);
}

/*
**  Name:	mem_nic2user
**  Function:	Copies a packet from board to user area (shared memory).
*/
static void mem_nic2user(dpeth_t *dep, int pageno, struct netdriver_data *data,
	size_t size)
{
  size_t offset, left;

  /* Computes shared memory address (skipping receive header) */
  offset = pageno * DP_PAGESIZE + sizeof(dp_rcvhdr_t);

  if (offset + size > dep->de_stoppage * DP_PAGESIZE) {
	left = dep->de_stoppage * DP_PAGESIZE - offset;
	netdriver_copyout(data, 0, dep->de_locmem + offset, left);
	offset = dep->de_startpage * DP_PAGESIZE;
	netdriver_copyout(data, left, dep->de_locmem + offset, size - left);
  } else
	netdriver_copyout(data, 0, dep->de_locmem + offset, size);
}

/*
**  Name:	mem_user2nic
**  Function:	Copies a packet from user area to board (shared memory).
*/
static void mem_user2nic(dpeth_t *dep, int pageno, struct netdriver_data *data,
	size_t size)
{
  size_t offset;

  /* Computes shared memory address */
  offset = pageno * DP_PAGESIZE;

  netdriver_copyin(data, 0, dep->de_locmem + offset, size);
}

/*
**  Name:	pio_getblock
**  Function:	Reads a block of packet from board (Prog. I/O).
*/
static void pio_getblock(dpeth_t *dep, u16_t offset, int size, void *dst)
{

  /* Sets up board for reading */
  ns_rw_setup(dep, CR_DM_RR, size, offset);

  if (dep->de_16bit == TRUE)
	insw(dep->de_data_port, dst, size);
  else
	insb(dep->de_data_port, dst, size);
}

/*
**  Name:	pio_nic2user
**  Function:	Copies a packet from board to user area (Prog. I/O).
*/
static void pio_nic2user(dpeth_t *dep, int pageno, struct netdriver_data *data,
	size_t size)
{
  size_t offset, left;

  /* Computes memory address (skipping receive header) */
  offset = pageno * DP_PAGESIZE + sizeof(dp_rcvhdr_t);

  if (offset + size > dep->de_stoppage * DP_PAGESIZE) {
	left = dep->de_stoppage * DP_PAGESIZE - offset;

	ns_rw_setup(dep, CR_DM_RR, left, offset);

	if (dep->de_16bit)
		netdriver_portinw(data, 0, dep->de_data_port, left);
	else
		netdriver_portinb(data, 0, dep->de_data_port, left);

	offset = dep->de_startpage * DP_PAGESIZE;
  } else
	left = 0;

  ns_rw_setup(dep, CR_DM_RR, size - left, offset);

  if (dep->de_16bit)
	netdriver_portinw(data, left, dep->de_data_port, size - left);
  else
	netdriver_portinb(data, left, dep->de_data_port, size - left);
}

/*
**  Name:	pio_user2nic
**  Function:	Copies a packet from user area to board (Prog. I/O).
*/
static void pio_user2nic(dpeth_t *dep, int pageno, struct netdriver_data *data,
	size_t size)
{
  int ix;

  /* Sets up board for writing */
  ns_rw_setup(dep, CR_DM_RW, size, pageno * DP_PAGESIZE);

  if (dep->de_16bit)
	netdriver_portoutw(data, 0, dep->de_data_port, size);
  else
	netdriver_portoutb(data, 0, dep->de_data_port, size);

  for (ix = 0; ix < 100; ix += 1) {
	if (inb_reg0(dep, DP_ISR) & ISR_RDC) break;
  }
  if (ix == 100)
	panic("remote dma failed to complete");
}

/*
**  Name:	ns_stats
**  Function:	Updates counters reading from device
*/
static void ns_stats(dpeth_t * dep)
{

  netdriver_stat_ierror(inb_reg0(dep, DP_CNTR0));
  netdriver_stat_ierror(inb_reg0(dep, DP_CNTR1));
  netdriver_stat_ierror(inb_reg0(dep, DP_CNTR2));
}

/*
**  Name:	ns_dodump
**  Function:	Displays statistics (a request from a function key).
*/
static void ns_dodump(dpeth_t * dep)
{

  ns_stats(dep);		/* Forces reading of counters from board */
}

/*
**  Name:	ns_reinit
**  Function:	Updates receiver configuration.
*/
static void ns_reinit(dpeth_t * dep)
{
  int dp_reg = 0;

  if (dep->de_flags & DEF_PROMISC) dp_reg |= RCR_AB | RCR_PRO | RCR_AM;
  if (dep->de_flags & DEF_BROAD) dp_reg |= RCR_AB;
  if (dep->de_flags & DEF_MULTI) dp_reg |= RCR_AM;
  outb_reg0(dep, DP_CR, CR_PS_P0);
  outb_reg0(dep, DP_RCR, dp_reg);
}

/*
**  Name:	ns_send
**  Function:	Transfers packet to device and starts sending.
*/
static int ns_send(dpeth_t *dep, struct netdriver_data *data, size_t size)
{
  unsigned int queue;

  queue = dep->de_sendq_head;
  if (dep->de_sendq[queue].sq_filled)
	return SUSPEND;

  (dep->de_user2nicf)(dep, dep->de_sendq[queue].sq_sendpage, data, size);
  dep->bytes_Tx += (long) size;
  dep->de_sendq[queue].sq_filled = TRUE;
  dep->de_flags |= DEF_XMIT_BUSY;
  if (dep->de_sendq_tail == queue) {	/* there it goes.. */
	ns_start_xmit(dep, size, dep->de_sendq[queue].sq_sendpage);
  } else
	dep->de_sendq[queue].sq_size = size;

  if (++queue == dep->de_sendq_nr) queue = 0;
  dep->de_sendq_head = queue;

  return OK;
}

/*
**  Name:	ns_reset
**  Function:	Resets device.
*/
static void ns_reset(dpeth_t * dep)
{
  unsigned int ix;

  /* Stop chip */
  outb_reg0(dep, DP_CR, CR_STP | CR_NO_DMA);
  outb_reg0(dep, DP_RBCR0, 0);
  outb_reg0(dep, DP_RBCR1, 0);
  for (ix = 0; ix < 0x1000 && (inb_reg0(dep, DP_ISR) & ISR_RST) == 0; ix += 1)
	 /* Do nothing */ ;
  outb_reg0(dep, DP_TCR, TCR_1EXTERNAL | TCR_OFST);
  outb_reg0(dep, DP_CR, CR_STA | CR_NO_DMA);
  outb_reg0(dep, DP_TCR, TCR_NORMAL | TCR_OFST);

  /* Acknowledge the ISR_RDC (remote dma) interrupt. */
  for (ix = 0; ix < 0x1000 && (inb_reg0(dep, DP_ISR) & ISR_RDC) == 0; ix += 1)
	 /* Do nothing */ ;
  outb_reg0(dep, DP_ISR, inb_reg0(dep, DP_ISR) & NOT(ISR_RDC));

  /* Reset the transmit ring. If we were transmitting a packet, we
   * pretend that the packet is processed. Higher layers will
   * retransmit if the packet wasn't actually sent. */
  dep->de_sendq_head = dep->de_sendq_tail = 0;
  for (ix = 0; ix < dep->de_sendq_nr; ix++)
	dep->de_sendq[ix].sq_filled = FALSE;
  netdriver_send();
}

/*
**  Name:	ns_recv
**  Function:	Gets a packet from device
*/
static ssize_t ns_recv(dpeth_t *dep, struct netdriver_data *data, size_t max)
{
  dp_rcvhdr_t header;
  unsigned pageno, curr, next;
  size_t length;
  int packet_processed = FALSE;
#ifdef ETH_IGN_PROTO
  u16_t eth_type;
#endif

  pageno = inb_reg0(dep, DP_BNRY) + 1;
  if (pageno == dep->de_stoppage) pageno = dep->de_startpage;

  do {
	/* */
	outb_reg0(dep, DP_CR, CR_PS_P1);
	curr = inb_reg1(dep, DP_CURR);
	outb_reg0(dep, DP_CR, CR_PS_P0 | CR_NO_DMA | CR_STA);

	if (curr == pageno)
		return SUSPEND;

	(dep->de_getblockf)(dep, pageno * DP_PAGESIZE, sizeof(header),
	    &header);
#ifdef ETH_IGN_PROTO
	(dep->de_getblockf)(dep, pageno * DP_PAGESIZE + sizeof(header) +
	    2 * sizeof(netdriver_addr_t), sizeof(eth_type), &eth_type);
#endif
	length = (header.dr_rbcl | (header.dr_rbch << 8)) -
	    sizeof(dp_rcvhdr_t);
	next = header.dr_next;

	if (length < NDEV_ETH_PACKET_MIN || length > max) {
		printf("%s: packet with strange length arrived: %zu\n",
			netdriver_name(), length);
		netdriver_stat_ierror(1);
		next = curr;

	} else if (next < dep->de_startpage || next >= dep->de_stoppage) {
		printf("%s: strange next page\n", netdriver_name());
		netdriver_stat_ierror(1);
		next = curr;
#ifdef ETH_IGN_PROTO
	} else if (eth_type == eth_ign_proto) {
		/* Hack: ignore packets of a given protocol */
		static int first = TRUE;
		if (first) {
			first = FALSE;
			printf("%s: dropping proto %04x packet\n",
			    netdriver_name(), ntohs(eth_ign_proto));
		}
		next = curr;
#endif
	} else if (header.dr_status & RSR_FO) {
		/* This is very serious, issue a warning and reset buffers */
		printf("%s: fifo overrun, resetting receive buffer\n",
		    netdriver_name());
		netdriver_stat_ierror(1);
		next = curr;

	} else if (header.dr_status & RSR_PRX) {
		(dep->de_nic2userf)(dep, pageno, data, length);
		packet_processed = TRUE;
	}
	dep->bytes_Rx += (long) length;
	outb_reg0(dep, DP_BNRY,
	    (next == dep->de_startpage ? dep->de_stoppage : next) - 1);
	pageno = next;
  } while (!packet_processed);

  return length;
}

/*
**  Name:	ns_interrupt
**  Function:	Handles interrupt.
*/
static void ns_interrupt(dpeth_t * dep)
{
  int isr, tsr;
  unsigned int queue;

  while ((isr = inb_reg0(dep, DP_ISR)) != 0) {

	outb_reg0(dep, DP_ISR, isr);
	if (isr & (ISR_PTX | ISR_TXE)) {

		tsr = inb_reg0(dep, DP_TSR);
		if (tsr & TSR_PTX) {
			/* Packet transmission was successful. */
		}
		if (tsr & TSR_COL)
			netdriver_stat_coll(1);
		if (tsr & (TSR_ABT | TSR_FU)) {
			netdriver_stat_oerror(1);
		}
		if ((isr & ISR_TXE) || (tsr & (TSR_CRS | TSR_CDH | TSR_OWC))) {
			printf("%s: got send Error (0x%02X)\n",
			    netdriver_name(), tsr);
			netdriver_stat_oerror(1);
		}
		queue = dep->de_sendq_tail;

		if (!(dep->de_sendq[queue].sq_filled)) { /* Hardware bug? */
			printf("%s: transmit interrupt, but not sending\n",
			    netdriver_name());
			continue;
		}
		dep->de_sendq[queue].sq_filled = FALSE;
		if (++queue == dep->de_sendq_nr) queue = 0;
		dep->de_sendq_tail = queue;
		if (dep->de_sendq[queue].sq_filled) {
			ns_start_xmit(dep, dep->de_sendq[queue].sq_size,
				dep->de_sendq[queue].sq_sendpage);
		}
		netdriver_send();
	}
	if (isr & ISR_PRX) {
		netdriver_recv();
	}
	if (isr & ISR_RXE) {
		printf("%s: got recv Error (0x%04X)\n",
		    netdriver_name(), inb_reg0(dep, DP_RSR));
		netdriver_stat_ierror(1);
	}
	if (isr & ISR_CNT) {
		ns_stats(dep);
	}
	if (isr & ISR_OVW) {
		printf("%s: got overwrite warning\n", netdriver_name());
	}
	if (isr & ISR_RDC) {
		/* Nothing to do */
	}
	if (isr & ISR_RST) {
		/* This means we got an interrupt but the ethernet chip is shut
		 * down. We reset the chip right away, possibly losing received
		 * packets in the process. There used to be a more elaborate
		 * approach of resetting only after all pending packets had
		 * been accepted, but it was broken and this is simpler anyway.
		 */
		printf("%s: network interface stopped\n",
		    netdriver_name());
		ns_reset(dep);
		break;
	}
  }
}

/*
**  Name:	ns_init
**  Function:	Initializes the NS 8390
*/
void ns_init(dpeth_t * dep)
{
  unsigned int dp_reg;
  unsigned int ix;

  /* NS8390 initialization (as recommended in National Semiconductor specs) */
  outb_reg0(dep, DP_CR, CR_PS_P0 | CR_STP | CR_NO_DMA);	/* 0x21 */
  outb_reg0(dep, DP_DCR, (((dep->de_16bit) ? DCR_WORDWIDE : DCR_BYTEWIDE) |
			DCR_LTLENDIAN | DCR_8BYTES | DCR_BMS));
  outb_reg0(dep, DP_RBCR0, 0);
  outb_reg0(dep, DP_RBCR1, 0);
  outb_reg0(dep, DP_RCR, RCR_MON);	/* Sets Monitor mode */
  outb_reg0(dep, DP_TCR, TCR_INTERNAL);	/* Sets Loopback mode 1 */
  outb_reg0(dep, DP_PSTART, dep->de_startpage);
  outb_reg0(dep, DP_PSTOP, dep->de_stoppage);
  outb_reg0(dep, DP_BNRY, dep->de_stoppage - 1);
  outb_reg0(dep, DP_ISR, 0xFF);	/* Clears Interrupt Status Register */
  outb_reg0(dep, DP_IMR, 0);	/* Clears Interrupt Mask Register */

  /* Copies station address in page 1 registers */
  outb_reg0(dep, DP_CR, CR_PS_P1 | CR_NO_DMA);	/* Selects Page 1 */
  for (ix = 0; ix < SA_ADDR_LEN; ix += 1)	/* Initializes address */
	outb_reg1(dep, DP_PAR0 + ix, dep->de_address.na_addr[ix]);
  for (ix = DP_MAR0; ix <= DP_MAR7; ix += 1)	/* Initializes address */
	outb_reg1(dep, ix, 0xFF);

  outb_reg1(dep, DP_CURR, dep->de_startpage);
  outb_reg1(dep, DP_CR, CR_PS_P0 | CR_NO_DMA);	/* Selects Page 0 */

  inb_reg0(dep, DP_CNTR0);	/* Resets counters by reading them */
  inb_reg0(dep, DP_CNTR1);
  inb_reg0(dep, DP_CNTR2);

  dp_reg = IMR_PRXE | IMR_PTXE | IMR_RXEE | IMR_TXEE | IMR_OVWE | IMR_CNTE;
  outb_reg0(dep, DP_ISR, 0xFF);	/* Clears Interrupt Status Register */
  outb_reg0(dep, DP_IMR, dp_reg);	/* Sets Interrupt Mask register */

  dp_reg = 0;
  if (dep->de_flags & DEF_PROMISC) dp_reg |= RCR_AB | RCR_PRO | RCR_AM;
  if (dep->de_flags & DEF_BROAD) dp_reg |= RCR_AB;
  if (dep->de_flags & DEF_MULTI) dp_reg |= RCR_AM;
  outb_reg0(dep, DP_RCR, dp_reg);	/* Sets receive as requested */
  outb_reg0(dep, DP_TCR, TCR_NORMAL);	/* Sets transmitter */

  outb_reg0(dep, DP_CR, CR_STA | CR_NO_DMA);	/* Starts board */

  /* Initializes the send queue. */
  for (ix = 0; ix < dep->de_sendq_nr; ix += 1)
	dep->de_sendq[ix].sq_filled = 0;
  dep->de_sendq_head = dep->de_sendq_tail = 0;

  /* Device specific functions */
  if (!dep->de_prog_IO) {
	dep->de_user2nicf = mem_user2nic;
	dep->de_nic2userf = mem_nic2user;
	dep->de_getblockf = mem_getblock;
  } else {
	dep->de_user2nicf = pio_user2nic;
	dep->de_nic2userf = pio_nic2user;
	dep->de_getblockf = pio_getblock;
  }
  dep->de_recvf = ns_recv;
  dep->de_sendf = ns_send;
  dep->de_flagsf = ns_reinit;
  dep->de_resetf = ns_reset;
  dep->de_getstatsf = ns_stats;
  dep->de_dumpstatsf = ns_dodump;
  dep->de_interruptf = ns_interrupt;
}

#endif				/* ENABLE_DP8390 */

/** end 8390.c **/
