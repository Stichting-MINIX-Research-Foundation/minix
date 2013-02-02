#include <assert.h>
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
#include <minix/com.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include "dp.h"

#if (ENABLE_DP8390 == 1)

#define PIO16	0	/* NOTE: pio 16 functions missing */

#include "8390.h"

#if 0
#define	sys_nic2mem(srcOffs,dstProc,dstOffs,length) \
	sys_vircopy(SELF,dep->de_memsegm,(vir_bytes)(srcOffs),\
		    (dstProc),D,(vir_bytes)(dstOffs),length)
#endif
#if 0
#define	sys_user2nic_s(srcProc,grant,dstOffs,length) \
	sys_safecopyfrom((srcProc),(grant),0, \
		    (vir_bytes)(dstOffs),length,dep->de_memsegm)
#endif

static char RdmaErrMsg[] = "remote dma failed to complete";

/*
**  Name:	void ns_rw_setup(dpeth_t *dep, int mode, int size, u16_t offset);
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
  return;
}

/*
**  Name:	void ns_start_xmit(dpeth_t *dep, int size, int pageno);
**  Function:	Sets the board for for transmitting and fires it.
*/
static void ns_start_xmit(const dpeth_t * dep, int size, int pageno)
{

  outb_reg0(dep, DP_TPSR, pageno);
  outb_reg0(dep, DP_TBCR1, size >> 8);
  outb_reg0(dep, DP_TBCR0, size & 0xFF);
  outb_reg0(dep, DP_CR, CR_NO_DMA | CR_STA | CR_TXP);	/* Fires transmission */
  return;
}

/*
**  Name:	void mem_getblock(dpeth_t *dep, u16_t offset,
**  				int size, void *dst)
**  Function:	Reads a block of packet from board (shared memory).
*/
static void mem_getblock(dpeth_t *dep, u16_t offset, int size, void *dst)
{
  panic("mem_getblock: not converted to safecopies");
#if 0
  sys_nic2mem(dep->de_linmem + offset, SELF, dst, size);
  return;
#endif
}

/*
**  Name:	void mem_nic2user(dpeth_t *dep, int pageno, int pktsize);
**  Function:	Copies a packet from board to user area (shared memory).
*/
static void mem_nic2user(dpeth_t * dep, int pageno, int pktsize)
{
  panic("mem_nic2user: not converted to safecopies");
#if 0
  phys_bytes offset;
  iovec_dat_s_t *iovp = &dep->de_read_iovec;
  int bytes, ix = 0;


  /* Computes shared memory address (skipping receive header) */
  offset = pageno * DP_PAGESIZE + sizeof(dp_rcvhdr_t);

  do {				/* Reads chuncks of packet into user area */

	bytes = iovp->iod_iovec[ix].iov_size;	/* Size of a chunck */
	if (bytes > pktsize) bytes = pktsize;

	/* Reads from board to user area */
	if ((offset + bytes) > (dep->de_stoppage * DP_PAGESIZE)) {

		/* Circular buffer wrap-around */
		bytes = dep->de_stoppage * DP_PAGESIZE - offset;
		sys_nic2mem_s(dep->de_linmem + offset, iovp->iod_proc_nr,
			    iovp->iod_iovec[ix].iov_grant, bytes);
		pktsize -= bytes;
		phys_user += bytes;
		bytes = iovp->iod_iovec[ix].iov_size - bytes;
		if (bytes > pktsize) bytes = pktsize;
		offset = dep->de_startpage * DP_PAGESIZE;
	}
	sys_nic2mem_s(dep->de_linmem + offset, iovp->iod_proc_nr,
		    iovp->iod_iovec[ix].iov_grant, bytes);
	offset += bytes;

	if (++ix >= IOVEC_NR) {	/* Next buffer of IO vector */
		dp_next_iovec(iovp);
		ix = 0;
	}
	/* Till packet done */
  } while ((pktsize -= bytes) > 0);
  return;
#endif
}

/*
**  Name:	void mem_user2nic(dpeth_t *dep, int pageno, int pktsize)
**  Function:	Copies a packet from user area to board (shared memory).
*/
static void mem_user2nic(dpeth_t *dep, int pageno, int pktsize)
{
#if 1
  panic("mem_user2nic: not converted to safecopies");
#else
  phys_bytes offset, phys_user;
  iovec_dat_s_t *iovp = &dep->de_write_iovec;
  int bytes, ix = 0;

  /* Computes shared memory address */
  offset = pageno * DP_PAGESIZE;

  do {				/* Reads chuncks of packet from user area */

	bytes = iovp->iod_iovec[ix].iov_size;	/* Size of chunck */
	if (bytes > pktsize) bytes = pktsize;

	/* Reads from user area to board (shared memory) */
	sys_user2nic_s(iovp->iod_proc_nr, iovp->iod_iovec[ix].iov_grant, 
		     dep->de_linmem + offset, bytes);
	offset += bytes;

	if (++ix >= IOVEC_NR) {	/* Next buffer of IO vector */
		dp_next_iovec(iovp);
		ix = 0;
	}
	/* Till packet done */
  } while ((pktsize -= bytes) > 0);
  return;
#endif
}

/*
**  Name:	void pio_getblock(dpeth_t *dep, u16_t offset,
**  				int size, void *dst)
**  Function:	Reads a block of packet from board (Prog. I/O).
*/
static void pio_getblock(dpeth_t *dep, u16_t offset, int size, void *dst)
{

  /* Sets up board for reading */
  ns_rw_setup(dep, CR_DM_RR, size, offset);

#if PIO16 == 0
  insb(dep->de_data_port, SELF, dst, size);
#else
  if (dep->de_16bit == TRUE) {
	insw(dep->de_data_port, dst, size);
  } else {
	insb(dep->de_data_port, dst, size);
  }
#endif
  return;
}

/*
**  Name:	void pio_nic2user(dpeth_t *dep, int pageno, int pktsize)
**  Function:	Copies a packet from board to user area (Prog. I/O).
*/
static void pio_nic2user(dpeth_t *dep, int pageno, int pktsize)
{
  iovec_dat_s_t *iovp = &dep->de_read_iovec;
  unsigned offset, iov_offset; int r, bytes, ix = 0;

  /* Computes memory address (skipping receive header) */
  offset = pageno * DP_PAGESIZE + sizeof(dp_rcvhdr_t);
  /* Sets up board for reading */
  ns_rw_setup(dep, CR_DM_RR, ((offset + pktsize) > (dep->de_stoppage * DP_PAGESIZE)) ?
	(dep->de_stoppage * DP_PAGESIZE) - offset : pktsize, offset);

  iov_offset= 0;
  do {				/* Reads chuncks of packet into user area */

	bytes = iovp->iod_iovec[ix].iov_size;	/* Size of a chunck */
	if (bytes > pktsize) bytes = pktsize;

	if ((offset + bytes) > (dep->de_stoppage * DP_PAGESIZE)) {

		/* Circular buffer wrap-around */
		bytes = dep->de_stoppage * DP_PAGESIZE - offset;
		r= sys_safe_insb(dep->de_data_port, iovp->iod_proc_nr, 
			iovp->iod_iovec[ix].iov_grant, iov_offset, bytes);
		if (r != OK) {
			panic("pio_nic2user: sys_safe_insb failed: %d", 				r);
		}
		pktsize -= bytes;
		iov_offset += bytes;
		bytes = iovp->iod_iovec[ix].iov_size - bytes;
		if (bytes > pktsize) bytes = pktsize;
		offset = dep->de_startpage * DP_PAGESIZE;
  		ns_rw_setup(dep, CR_DM_RR, pktsize, offset);
	}
	r= sys_safe_insb(dep->de_data_port, iovp->iod_proc_nr,
		iovp->iod_iovec[ix].iov_grant, iov_offset, bytes);
	if (r != OK)
		panic("pio_nic2user: sys_safe_insb failed: %d", r);
	offset += bytes;

	if (++ix >= IOVEC_NR) {	/* Next buffer of IO vector */
		dp_next_iovec(iovp);
		ix = 0;
	}
	iov_offset= 0;
	/* Till packet done */
  } while ((pktsize -= bytes) > 0);
  return;
}

/*
**  Name:	void pio_user2nic(dpeth_t *dep, int pageno, int pktsize)
**  Function:	Copies a packet from user area to board (Prog. I/O).
*/
static void pio_user2nic(dpeth_t *dep, int pageno, int pktsize)
{
  iovec_dat_s_t *iovp = &dep->de_write_iovec;
  int r, bytes, ix = 0;

  /* Sets up board for writing */
  ns_rw_setup(dep, CR_DM_RW, pktsize, pageno * DP_PAGESIZE);
  
  do {				/* Reads chuncks of packet from user area */

	bytes = iovp->iod_iovec[ix].iov_size;	/* Size of chunck */
	if (bytes > pktsize) bytes = pktsize;
	r= sys_safe_outsb(dep->de_data_port, iovp->iod_proc_nr,
	      iovp->iod_iovec[ix].iov_grant, 0, bytes);
	if (r != OK)
		panic("pio_user2nic: sys_safe_outsb failed: %d", r);

	if (++ix >= IOVEC_NR) {	/* Next buffer of I/O vector */
		dp_next_iovec(iovp);
		ix = 0;
	}
	/* Till packet done */
  } while ((pktsize -= bytes) > 0);

  for (ix = 0; ix < 100; ix += 1) {
	if (inb_reg0(dep, DP_ISR) & ISR_RDC) break;
  }
  if (ix == 100) {
	panic("%s", RdmaErrMsg);
  }
  return;
}

/*
**  Name:	void ns_stats(dpeth_t * dep)
**  Function:	Updates counters reading from device
*/
static void ns_stats(dpeth_t * dep)
{

  dep->de_stat.ets_CRCerr += inb_reg0(dep, DP_CNTR0);
  dep->de_stat.ets_recvErr += inb_reg0(dep, DP_CNTR1);
  dep->de_stat.ets_fifoOver += inb_reg0(dep, DP_CNTR2);
  return;
}

/*
**  Name:	void ns_dodump(dpeth_t * dep)
**  Function:	Displays statistics (a request from F5 key).
*/
static void ns_dodump(dpeth_t * dep)
{

  ns_stats(dep);		/* Forces reading fo counters from board */
  return;
}

/*
**  Name:	void ns_reinit(dpeth_t *dep)
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
  return;
}

/*
**  Name:	void ns_send(dpeth_t * dep, int from_int, int size)
**  Function:	Transfers packet to device and starts sending.
*/
static void ns_send(dpeth_t * dep, int from_int, int size)
{
  int queue;

  if (queue = dep->de_sendq_head, dep->de_sendq[queue].sq_filled) {
	if (from_int) panic("should not be sending ");
	dep->de_send_s = size;
	return;
  }
  (dep->de_user2nicf) (dep, dep->de_sendq[queue].sq_sendpage, size);
  dep->bytes_Tx += (long) size;
  dep->de_sendq[queue].sq_filled = TRUE;
  dep->de_flags |= (DEF_XMIT_BUSY | DEF_ACK_SEND);
  if (dep->de_sendq_tail == queue) {	/* there it goes.. */
	ns_start_xmit(dep, size, dep->de_sendq[queue].sq_sendpage);
  } else
	dep->de_sendq[queue].sq_size = size;

  if (++queue == dep->de_sendq_nr) queue = 0;
  dep->de_sendq_head = queue;
  dep->de_flags &= NOT(DEF_SENDING);

  return;
}

/*
**  Name:	void ns_reset(dpeth_t *dep)
**  Function:	Resets device.
*/
static void ns_reset(dpeth_t * dep)
{
  int ix;

  /* Stop chip */
  outb_reg0(dep, DP_CR, CR_STP | CR_NO_DMA);
  outb_reg0(dep, DP_RBCR0, 0);
  outb_reg0(dep, DP_RBCR1, 0);
  for (ix = 0; ix < 0x1000 && ((inb_reg0(dep, DP_ISR) & ISR_RST) == 0); ix += 1)
	 /* Do nothing */ ;
  outb_reg0(dep, DP_TCR, TCR_1EXTERNAL | TCR_OFST);
  outb_reg0(dep, DP_CR, CR_STA | CR_NO_DMA);
  outb_reg0(dep, DP_TCR, TCR_NORMAL | TCR_OFST);

  /* Acknowledge the ISR_RDC (remote dma) interrupt. */
  for (ix = 0; ix < 0x1000 && ((inb_reg0(dep, DP_ISR) & ISR_RDC) == 0); ix += 1)
	 /* Do nothing */ ;
  outb_reg0(dep, DP_ISR, inb_reg0(dep, DP_ISR) & NOT(ISR_RDC));

  /* Reset the transmit ring. If we were transmitting a packet, we
   * pretend that the packet is processed. Higher layers will
   * retransmit if the packet wasn't actually sent. */
  dep->de_sendq_head = dep->de_sendq_tail = 0;
  for (ix = 0; ix < dep->de_sendq_nr; ix++)
	dep->de_sendq[ix].sq_filled = FALSE;
  ns_send(dep, TRUE, dep->de_send_s);
  return;
}

/*
**  Name:	void ns_recv(dpeth_t *dep, int fromint, int size)
**  Function:	Gets a packet from device
*/
static void ns_recv(dpeth_t *dep, int fromint, int size)
{
  dp_rcvhdr_t header;
  unsigned pageno, curr, next;
  vir_bytes length;
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

	if (curr == pageno) break;

	(dep->de_getblockf) (dep, pageno * DP_PAGESIZE, sizeof(header), &header);
#ifdef ETH_IGN_PROTO
	(dep->de_getblockf) (dep, pageno * DP_PAGESIZE + sizeof(header) + 2 * sizeof(ether_addr_t), sizeof(eth_type), &eth_type);
#endif
	length = (header.dr_rbcl | (header.dr_rbch << 8)) - sizeof(dp_rcvhdr_t);
	next = header.dr_next;

	if (length < ETH_MIN_PACK_SIZE || length > ETH_MAX_PACK_SIZE) {
		printf("%s: packet with strange length arrived: %ld\n",
			dep->de_name, length);
		dep->de_stat.ets_recvErr += 1;
		next = curr;

	} else if (next < dep->de_startpage || next >= dep->de_stoppage) {
		printf("%s: strange next page\n", dep->de_name);
		dep->de_stat.ets_recvErr += 1;
		next = curr;

#ifdef ETH_IGN_PROTO
	} else if (eth_type == eth_ign_proto) {
		/* Hack: ignore packets of a given protocol */
		static int first = TRUE;
		if (first) {
			first = FALSE;
			printf("%s: dropping proto %04x packet\n", dep->de_name, ntohs(eth_ign_proto));
		}
		next = curr;
#endif
	} else if (header.dr_status & RSR_FO) {
		/* This is very serious, issue a warning and reset buffers */
		printf("%s: fifo overrun, resetting receive buffer\n", dep->de_name);
		dep->de_stat.ets_fifoOver += 1;
		next = curr;

	} else if ((header.dr_status & RSR_PRX) && (dep->de_flags & DEF_ENABLED)) {

		if (!(dep->de_flags & DEF_READING)) break;

		(dep->de_nic2userf) (dep, pageno, length);
		dep->de_read_s = length;
		dep->de_flags |= DEF_ACK_RECV;
		dep->de_flags &= NOT(DEF_READING);
		packet_processed = TRUE;
	}
	dep->bytes_Rx += (long) length;
	dep->de_stat.ets_packetR += 1;
	outb_reg0(dep, DP_BNRY, (next == dep->de_startpage ? dep->de_stoppage : next) - 1);
	pageno = next;

  } while (!packet_processed);
#if 0
  if ((dep->de_flags & (DEF_READING | DEF_STOPPED)) == (DEF_READING | DEF_STOPPED))
	/* The chip is stopped, and all arrived packets delivered */
	(*dep->de_resetf) (dep);
  dep->de_flags &= NOT(DEF_STOPPED);
#endif
  return;
}

/*
**  Name:	void ns_interrupt(dpeth_t * dep)
**  Function:	Handles interrupt.
*/
static void ns_interrupt(dpeth_t * dep)
{
  int isr, tsr;
  int queue;

  while ((isr = inb_reg0(dep, DP_ISR)) != 0) {

	outb_reg0(dep, DP_ISR, isr);
	if (isr & (ISR_PTX | ISR_TXE)) {

		tsr = inb_reg0(dep, DP_TSR);
		if (tsr & TSR_PTX) {
			dep->de_stat.ets_packetT++;
		}
		if (tsr & TSR_COL) dep->de_stat.ets_collision++;
		if (tsr & (TSR_ABT | TSR_FU)) {
			dep->de_stat.ets_fifoUnder++;
		}
		if ((isr & ISR_TXE) || (tsr & (TSR_CRS | TSR_CDH | TSR_OWC))) {
			printf("%s: got send Error (0x%02X)\n", dep->de_name, tsr);
			dep->de_stat.ets_sendErr++;
		}
		queue = dep->de_sendq_tail;

		if (!(dep->de_sendq[queue].sq_filled)) {	/* Hardware bug? */
			printf("%s: transmit interrupt, but not sending\n", dep->de_name);
			continue;
		}
		dep->de_sendq[queue].sq_filled = FALSE;
		if (++queue == dep->de_sendq_nr) queue = 0;
		dep->de_sendq_tail = queue;
		if (dep->de_sendq[queue].sq_filled) {
			ns_start_xmit(dep, dep->de_sendq[queue].sq_size,
				dep->de_sendq[queue].sq_sendpage);
		}
		if (dep->de_flags & DEF_SENDING) {
			ns_send(dep, TRUE, dep->de_send_s);
		}
	}
	if (isr & ISR_PRX) {
		ns_recv(dep, TRUE, 0);
	}
	if (isr & ISR_RXE) {
		printf("%s: got recv Error (0x%04X)\n", dep->de_name, inb_reg0(dep, DP_RSR));
		dep->de_stat.ets_recvErr++;
	}
	if (isr & ISR_CNT) {
		dep->de_stat.ets_CRCerr += inb_reg0(dep, DP_CNTR0);
		dep->de_stat.ets_recvErr += inb_reg0(dep, DP_CNTR1);
		dep->de_stat.ets_fifoOver += inb_reg0(dep, DP_CNTR2);
	}
	if (isr & ISR_OVW) {
		printf("%s: got overwrite warning\n", dep->de_name);
	}
	if (isr & ISR_RDC) {
		/* Nothing to do */
	}
	if (isr & ISR_RST) {
		/* This means we got an interrupt but the ethernet
		 * chip is shutdown. We set the flag DEF_STOPPED, and
		 * continue processing arrived packets. When the
		 * receive buffer is empty, we reset the dp8390. */
		printf("%s: network interface stopped\n", dep->de_name);
		dep->de_flags |= DEF_STOPPED;
		break;
	}
  }
  if ((dep->de_flags & (DEF_READING | DEF_STOPPED)) == (DEF_READING | DEF_STOPPED)) {

	/* The chip is stopped, and all arrived packets delivered */
	ns_reset(dep);
	dep->de_flags &= NOT(DEF_STOPPED);
  }
  return;
}

/*
**  Name:	void ns_init(dpeth_t *dep)
**  Function:	Initializes the NS 8390
*/
void ns_init(dpeth_t * dep)
{
  int dp_reg;
  int ix;

  /* NS8390 initialization (as recommended in National Semiconductor specs) */
  outb_reg0(dep, DP_CR, CR_PS_P0 | CR_STP | CR_NO_DMA);	/* 0x21 */
#if PIO16 == 0
  outb_reg0(dep, DP_DCR, (DCR_BYTEWIDE | DCR_LTLENDIAN | DCR_8BYTES | DCR_BMS));
#else
  outb_reg0(dep, DP_DCR, (((dep->de_16bit) ? DCR_WORDWIDE : DCR_BYTEWIDE) |
			DCR_LTLENDIAN | DCR_8BYTES | DCR_BMS));
#endif
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
	outb_reg1(dep, DP_PAR0 + ix, dep->de_address.ea_addr[ix]);
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
#if PIO16 == 0
	dep->de_user2nicf = pio_user2nic;
	dep->de_nic2userf = pio_nic2user;
	dep->de_getblockf = pio_getblock;
#else
#error	Missing I/O functions for pio 16 bits
#endif
  }
  dep->de_recvf = ns_recv;
  dep->de_sendf = ns_send;
  dep->de_flagsf = ns_reinit;
  dep->de_resetf = ns_reset;
  dep->de_getstatsf = ns_stats;
  dep->de_dumpstatsf = ns_dodump;
  dep->de_interruptf = ns_interrupt;

  return;			/* Done */
}

#if PIO16 == 1

/*
**  Name:	void dp_pio16_user2nic(dpeth_t *dep, int pageno, int pktsize)
**  Function:	Copies a packet from user area to board (Prog. I/O, 16bits).
*/
static void dp_pio16_user2nic(dpeth_t *dep, int pageno, int pktsize)
{
  u8_t two_bytes[2];
  phys_bytes phys_user, phys_2bytes = vir2phys(two_bytes);
  vir_bytes ecount = (pktsize + 1) & NOT(0x0001);
  int bytes, ix = 0, odd_byte = 0;
  iovec_dat_t *iovp = &dep->de_write_iovec;

  outb_reg0(dep, DP_ISR, ISR_RDC);
  dp_read_setup(dep, ecount, pageno * DP_PAGESIZE);

  do {
	bytes = iovp->iod_iovec[ix].iov_size;
	if (bytes > pktsize) bytes = pktsize;

	phys_user = numap(iovp->iod_proc_nr, iovp->iod_iovec[ix].iov_addr, bytes);
	if (!phys_user) panic(UmapErrMsg);

	if (odd_byte) {
		phys_copy(phys_user, phys_2bytes + 1, (phys_bytes) 1);
		out_word(dep->de_data_port, *(u16_t *)two_bytes);
		pktsize--;
		bytes--;
		phys_user++;
		odd_byte = 0;
		if (!bytes) continue;
	}
	ecount = bytes & NOT(0x0001);
	if (ecount != 0) {
		phys_outsw(dep->de_data_port, phys_user, ecount);
		pktsize -= ecount;
		bytes -= ecount;
		phys_user += ecount;
	}
	if (bytes) {
		phys_copy(phys_user, phys_2bytes, (phys_bytes) 1);
		pktsize--;
		bytes--;
		phys_user++;
		odd_byte = 1;
	}
	if (++ix >= IOVEC_NR) {	/* Next buffer of I/O vector */
		dp_next_iovec(iovp);
		ix = 0;
	}

  }  while (bytes > 0);

  if (odd_byte) out_word(dep->de_data_port, *(u16_t *) two_bytes);
  for (ix = 0; ix < 100; ix++) {
	if (inb_reg0(dep, DP_ISR) & ISR_RDC) break;
  }
  if (ix == 100) {
	panic(RdmaErrMsg);
  }
  return;
}

/*
**  Name:	void dp_pio16_nic2user(dpeth_t *dep, int pageno, int pktsize)
**  Function:	Copies a packet from board to user area (Prog. I/O, 16bits).
*/
static void dp_pio16_nic2user(dpeth_t * dep, int nic_addr, int count)
{
  phys_bytes phys_user;
  vir_bytes ecount;
  int bytes, i;
  u8_t two_bytes[2];
  phys_bytes phys_2bytes;
  int odd_byte;

  ecount = (count + 1) & ~1;
  phys_2bytes = vir2phys(two_bytes);
  odd_byte = 0;

  dp_read_setup(dep, ecount, nic_addr);

  i = 0;
  while (count > 0) {
	if (i >= IOVEC_NR) {
		dp_next_iovec(iovp);
		i = 0;
		continue;
	}
	bytes = iovp->iod_iovec[i].iov_size;
	if (bytes > count) bytes = count;

	phys_user = numap(iovp->iod_proc_nr,
			  iovp->iod_iovec[i].iov_addr, bytes);
	if (!phys_user) panic(UmapErrMsg);
	if (odd_byte) {
		phys_copy(phys_2bytes + 1, phys_user, (phys_bytes) 1);
		count--;
		bytes--;
		phys_user++;
		odd_byte = 0;
		if (!bytes) continue;
	}
	ecount = bytes & ~1;
	if (ecount != 0) {
		phys_insw(dep->de_data_port, phys_user, ecount);
		count -= ecount;
		bytes -= ecount;
		phys_user += ecount;
	}
	if (bytes) {
		*(u16_t *) two_bytes = in_word(dep->de_data_port);
		phys_copy(phys_2bytes, phys_user, (phys_bytes) 1);
		count--;
		bytes--;
		phys_user++;
		odd_byte = 1;
	}
  }
  return;
}

#endif				/* PIO16 == 1 */

#endif				/* ENABLE_DP8390 */

/** end 8390.c **/
