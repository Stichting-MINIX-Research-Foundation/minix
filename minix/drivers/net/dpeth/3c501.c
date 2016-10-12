/*
**  File:	3c501.c		Jan. 14, 1997
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  This file contains specific implementation of the ethernet
**  device driver for 3Com Etherlink (3c501) boards.  This is a
**  very old board and its performances are very poor for today
**  network environments.
*/

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include "dp.h"

#if (ENABLE_3C501 == 1)

#include "3c501.h"

static unsigned char StationAddress[SA_ADDR_LEN] = {0, 0, 0, 0, 0, 0,};
static buff_t *TxBuff = NULL;

/*
**  Name:	el1_getstats
**  Function:	Reads statistics counters from board.
**/
static void el1_getstats(dpeth_t * dep)
{

  /* Nothing to do */
}

/*
**  Name:	el1_reset
**  Function:	Reset function specific for Etherlink hardware.
*/
static void el1_reset(dpeth_t * dep)
{
  unsigned int ix;

  for (ix = 0; ix < 8; ix += 1)	/* Resets the board */
	outb_el1(dep, EL1_CSR, ECSR_RESET);
  outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);

  /* Set Ethernet Address on controller */
  outb_el1(dep, EL1_CSR, ECSR_LOOP);	/* Loopback mode */
  for (ix = EL1_ADDRESS; ix < SA_ADDR_LEN; ix += 1)
	outb_el1(dep, ix, StationAddress[ix]);

  /* Enable DMA/Interrupt, gain control of Buffer */
  outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);
  /* Clear RX packet area */
  outw_el1(dep, EL1_RECVPTR, 0);
  /* Enable transmit/receive configuration and flush pending interrupts */
  outb_el1(dep, EL1_XMIT, EXSR_IDLE | EXSR_16JAM | EXSR_JAM | EXSR_UNDER);
  outb_el1(dep, EL1_RECV, dep->de_recv_mode);
  inb_el1(dep, EL1_RECV);
  inb_el1(dep, EL1_XMIT);
  dep->de_flags &= NOT(DEF_XMIT_BUSY);
}

/*
**  Name:	el1_dumpstats
**  Function:	Dumps counter on screen (support for console display).
*/
static void el1_dumpstats(dpeth_t * UNUSED(dep))
{

}

/*
**  Name:	el1_mode_init
**  Function:	Initializes receicer mode
*/
static void el1_mode_init(dpeth_t * dep)
{

  if (dep->de_flags & DEF_BROAD) {
	dep->de_recv_mode = ERSR_BROAD | ERSR_RMASK;

  } else if (dep->de_flags & DEF_PROMISC) {
	dep->de_recv_mode = ERSR_ALL | ERSR_RMASK;

  } else if (dep->de_flags & DEF_MULTI) {
	dep->de_recv_mode = ERSR_MULTI | ERSR_RMASK;

  } else {
	dep->de_recv_mode = ERSR_NONE | ERSR_RMASK;
  }
  outb_el1(dep, EL1_RECV, dep->de_recv_mode);
  inb_el1(dep, EL1_RECV);
}

/*
**  Name:	el1_recv
**  Function:	Receive function.  Called from interrupt handler to
**  		unload recv. buffer or from main (packet to client)
*/
static ssize_t el1_recv(dpeth_t *dep, struct netdriver_data *data, size_t max)
{
  buff_t *rxptr;
  size_t size;

  if ((rxptr = dep->de_recvq_head) == NULL)
	return SUSPEND;

  /* Remove buffer from queue and free buffer */
  if (dep->de_recvq_tail == dep->de_recvq_head)
	dep->de_recvq_head = dep->de_recvq_tail = NULL;
  else
	dep->de_recvq_head = rxptr->next;

  /* Copy buffer to user area */
  size = MIN((size_t)rxptr->size, max);

  netdriver_copyout(data, 0, rxptr->buffer, size);

  /* Return buffer to the idle pool */
  free_buff(dep, rxptr);

  return size;
}

/*
**  Name:	el1_send
**  Function:	Send function.
*/
static int el1_send(dpeth_t *dep, struct netdriver_data *data, size_t size)
{
  buff_t *txbuff;
  clock_t now;

  if (dep->de_flags & DEF_XMIT_BUSY) {
	now = getticks();
	if ((now - dep->de_xmit_start) > 4) {
		/* Transmitter timed out */
		DEBUG(printf("3c501: transmitter timed out ... \n"));
		netdriver_stat_oerror(1);
		dep->de_flags &= NOT(DEF_XMIT_BUSY);
		/* Try sending anyway. */
	} else
		return SUSPEND;
  }

  /* Since we may have to retransmit, we need a local copy. */
  if ((txbuff = alloc_buff(dep, size + sizeof(buff_t))) == NULL)
	panic("out of memory");

  /* Fill transmit buffer from user area */
  txbuff->next = NULL;
  txbuff->size = size;

  netdriver_copyin(data, 0, txbuff->buffer, size);

  /* Save for retransmission */
  TxBuff = txbuff;
  dep->de_flags |= DEF_XMIT_BUSY;

  /* Setup board for packet loading */
  outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);
  inb_el1(dep, EL1_RECV);	/* Clears any spurious interrupt */
  inb_el1(dep, EL1_XMIT);
  outw_el1(dep, EL1_RECVPTR, 0);	/* Clears RX packet area */

  /* Loads packet */
  outw_el1(dep, EL1_XMITPTR, (EL1_BFRSIZ - size));
  outsb(dep->de_data_port, txbuff->buffer, size);
  /* Starts transmitter */
  outw_el1(dep, EL1_XMITPTR, (EL1_BFRSIZ - size));
  outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_XMIT);	/* There it goes... */

  dep->de_xmit_start = getticks();

  return OK;
}

/*
**  Name:	el1_stop
**  Function:	Stops board and disable interrupts.
*/
static void el1_stop(dpeth_t * dep)
{
  int ix;

  DEBUG(printf("%s: stopping Etherlink ....\n", netdriver_name()));
  for (ix = 0; ix < 8; ix += 1)	/* Reset board */
	outb_el1(dep, EL1_CSR, ECSR_RESET);
  outb_el1(dep, EL1_CSR, ECSR_SYS);
  sys_irqdisable(&dep->de_hook);	/* Disable interrupt */
}

/*
**  Name:	el1_interrupt
**  Function:	Interrupt handler.  Acknwledges transmit interrupts
**  		or unloads receive buffer to memory queue.
*/
static void el1_interrupt(dpeth_t * dep)
{
  u16_t csr, isr;
  int pktsize;
  buff_t *rxptr;

  csr = inb_el1(dep, EL1_CSR);
  if ((csr & ECSR_XMIT) && (dep->de_flags & DEF_XMIT_BUSY)) {

	/* Got a transmit interrupt */
	isr = inb_el1(dep, EL1_XMIT);
	if ((isr & (EXSR_16JAM | EXSR_UNDER | EXSR_JAM)) || !(isr & EXSR_IDLE)) {
	DEBUG(printf("3c501: got xmit interrupt (ASR=0x%02X XSR=0x%02X)\n", csr, isr));
		if (isr & EXSR_JAM) {
			/* Sending, packet got a collision */
			netdriver_stat_coll(1);
			/* Put pointer back to beginning of packet */
			outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);
			outw_el1(dep, EL1_XMITPTR, (EL1_BFRSIZ - TxBuff->size));
			/* And retrigger transmission */
			outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_XMIT);
			return;

		} else if ((isr & EXSR_16JAM) || !(isr & EXSR_IDLE)) {
			netdriver_stat_oerror(1);
		} else if (isr & EXSR_UNDER) {
			netdriver_stat_oerror(1);
		}
		DEBUG(printf("3c501: got xmit interrupt (0x%02X)\n", isr));
		el1_reset(dep);
	} else {
		/** if (inw_el1(dep, EL1_XMITPTR) == EL1_BFRSIZ) **/
		/* Packet transmitted successfully */
		dep->bytes_Tx += (long) (TxBuff->size);
		free_buff(dep, TxBuff);
		dep->de_flags &= NOT(DEF_XMIT_BUSY);
		netdriver_send();
		if (dep->de_flags & DEF_XMIT_BUSY)
			return;
	}

  } else if ((csr & (ECSR_RECV | ECSR_XMTBSY)) == (ECSR_RECV | ECSR_XMTBSY)) {

	/* Got a receive interrupt */
	isr = inb_el1(dep, EL1_RECV);
	pktsize = inw_el1(dep, EL1_RECVPTR);
	if ((isr & ERSR_RERROR) || (isr & ERSR_STALE)) {
		DEBUG(printf("Rx0 (ASR=0x%02X RSR=0x%02X size=%d)\n",
		    csr, isr, pktsize));
		netdriver_stat_ierror(1);

	} else if (pktsize < NDEV_ETH_PACKET_MIN ||
	    pktsize > NDEV_ETH_PACKET_MAX) {
		DEBUG(printf("Rx1 (ASR=0x%02X RSR=0x%02X size=%d)\n",
		    csr, isr, pktsize));
		netdriver_stat_ierror(1);

	} else if ((rxptr = alloc_buff(dep, pktsize + sizeof(buff_t))) == NULL) {
		/* Memory not available. Drop packet */
		netdriver_stat_ierror(1);

	} else if (isr & (ERSR_GOOD | ERSR_ANY)) {
		/* Got a good packet. Read it from buffer */
		outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);
		outw_el1(dep, EL1_XMITPTR, 0);
		insb(dep->de_data_port, rxptr->buffer, pktsize);
		rxptr->next = NULL;
		rxptr->size = pktsize;
		dep->bytes_Rx += (long) pktsize;
		/* Queue packet to receive queue */
		if (dep->de_recvq_head == NULL)
			dep->de_recvq_head = rxptr;
		else
			dep->de_recvq_tail->next = rxptr;
		dep->de_recvq_tail = rxptr;

		/* Reply to pending Receive requests, if any */
		netdriver_recv();
	}
  } else {			/* Nasty condition, should never happen */
	DEBUG(
	      printf("3c501: got interrupt with status 0x%02X\n"
		     "       de_flags=0x%04X  XSR=0x%02X RSR=0x%02X \n"
		     "       xmit buffer = 0x%4X recv buffer = 0x%4X\n",
			csr, dep->de_flags,
			inb_el1(dep, EL1_RECV),
			inb_el1(dep, EL1_XMIT),
			inw_el1(dep, EL1_XMITPTR),
			inw_el1(dep, EL1_RECVPTR))
		);
	el1_reset(dep);
  }

  /* Move into receive mode */
  outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_RECV);
  outw_el1(dep, EL1_RECVPTR, 0);
  /* Be sure that interrupts are cleared */
  inb_el1(dep, EL1_RECV);
  inb_el1(dep, EL1_XMIT);
}

/*
**  Name:	el1_init
**  Function:	Initalizes board hardware and driver data structures.
*/
static void el1_init(dpeth_t * dep)
{
  unsigned int ix;

  dep->de_irq &= NOT(DEI_DEFAULT);	/* Strip the default flag. */
  dep->de_offset_page = 0;
  dep->de_data_port = dep->de_base_port + EL1_DATAPORT;

  el1_reset(dep);		/* Reset and initialize board */

  /* Start receiver (default mode) */
  outw_el1(dep, EL1_RECVPTR, 0);
  outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_RECV);

  /* Initializes buffer pool */
  init_buff(dep, NULL);
  el1_mode_init(dep);

  printf("%s: Etherlink (%s) at %X:%d - ",
         netdriver_name(), "3c501", dep->de_base_port, dep->de_irq);
  for (ix = 0; ix < SA_ADDR_LEN; ix += 1)
	printf("%02X%c", (dep->de_address.na_addr[ix] = StationAddress[ix]),
	       ix < SA_ADDR_LEN - 1 ? ':' : '\n');

  /* Device specific functions */
  dep->de_recvf = el1_recv;
  dep->de_sendf = el1_send;
  dep->de_flagsf = el1_mode_init;
  dep->de_resetf = el1_reset;
  dep->de_getstatsf = el1_getstats;
  dep->de_dumpstatsf = el1_dumpstats;
  dep->de_interruptf = el1_interrupt;
}

/*
**  Name:	el1_probe
**  Function:	Checks for presence of the board.
*/
int el1_probe(dpeth_t * dep)
{
  unsigned int ix;

  for (ix = 0; ix < 8; ix += 1)	/* Reset the board */
	outb_el1(dep, EL1_CSR, ECSR_RESET);
  outb_el1(dep, EL1_CSR, ECSR_SYS);	/* Leaves buffer to system */

  /* Check station address */
  for (ix = 0; ix < SA_ADDR_LEN; ix += 1) {
	outw_el1(dep, EL1_XMITPTR, ix);
	StationAddress[ix] = inb_el1(dep, EL1_SAPROM);
  }
  if (StationAddress[0] != 0x02 ||	/* Etherlink Station address  */
      StationAddress[1] != 0x60 ||	/* MUST be 02:60:8c:xx:xx:xx  */
      StationAddress[2] != 0x8C)
	return FALSE;		/* No Etherlink board at this address */

  dep->de_ramsize = 0;		/* RAM size is meaningless */
  dep->de_linmem = 0L;		/* Access is via I/O port  */

  /* Device specific functions */
  dep->de_initf = el1_init;
  dep->de_stopf = el1_stop;

  return TRUE;			/* Etherlink board found */
}

#endif				/* ENABLE_3C501 */

/** 3c501.c **/
