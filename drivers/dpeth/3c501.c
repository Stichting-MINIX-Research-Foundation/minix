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
#include <minix/com.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include "dp.h"

#if (ENABLE_3C501 == 1)

#include "3c501.h"

static unsigned char StationAddress[SA_ADDR_LEN] = {0, 0, 0, 0, 0, 0,};
static buff_t *TxBuff = NULL;

/*
**  Name:	void el1_getstats(dpeth_t *dep)
**  Function:	Reads statistics counters from board.
**/
static void el1_getstats(dpeth_t * dep)
{

  return;			/* Nothing to do */
}

/*
**  Name:	void el1_reset(dpeth_t *dep)
**  Function:	Reset function specific for Etherlink hardware.
*/
static void el1_reset(dpeth_t * dep)
{
  int ix;

  for (ix = 0; ix < 8; ix += 1)	/* Resets the board */
	outb_el1(dep, EL1_CSR, ECSR_RESET);
  outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);

  /* Set Ethernet Address on controller */
  outb_el1(dep, EL1_CSR, ECSR_LOOP);	/* Loopback mode */
  for (ix = EL1_ADDRESS; ix < SA_ADDR_LEN; ix += 1)
	outb_el1(dep, ix, StationAddress[ix]);

  lock();
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
  unlock();
  return;			/* Done */
}

/*
**  Name:	void el1_dumpstats(dpeth_t *dep, int port, vir_bytes size)
**  Function:	Dumps counter on screen (support for console display).
*/
static void el1_dumpstats(dpeth_t * UNUSED(dep))
{

  return;
}

/*
**  Name:	void el1_mode_init(dpeth_t *dep)
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
  return;
}

/*
**  Name:	void el1_recv(dpeth_t *dep, int from, int size)
**  Function:	Receive function.  Called from interrupt handler to
**  		unload recv. buffer or from main (packet to client)
*/
static void el1_recv(dpeth_t * dep, int from, int size)
{
  buff_t *rxptr;

  while ((dep->de_flags & DEF_READING) && (rxptr = dep->de_recvq_head)) {

	/* Remove buffer from queue and free buffer */
	lock();
	if (dep->de_recvq_tail == dep->de_recvq_head)
		dep->de_recvq_head = dep->de_recvq_tail = NULL;
	else
		dep->de_recvq_head = rxptr->next;
	unlock();

	/* Copy buffer to user area */
	mem2user(dep, rxptr);

	/* Reply information */
	dep->de_read_s = rxptr->size;
	dep->de_flags |= DEF_ACK_RECV;
	dep->de_flags &= NOT(DEF_READING);

	/* Return buffer to the idle pool */
	free_buff(dep, rxptr);
  }
  return;
}

/*
**  Name:	void el1_send(dpeth_t *dep, int from_int, int pktsize)
**  Function:	Send function.  Called from main to transit a packet or
**  		from interrupt handler when a new packet was queued.
*/
static void el1_send(dpeth_t * dep, int from_int, int pktsize)
{
  buff_t *txbuff;
  clock_t now;

  if (from_int == FALSE) {

	if ((txbuff = alloc_buff(dep, pktsize + sizeof(buff_t))) != NULL) {

		/*  Fill transmit buffer from user area */
		txbuff->next = NULL;
		txbuff->size = pktsize;
		txbuff->client = dep->de_client;
		user2mem(dep, txbuff);
	} else
		panic("out of memory for Tx");

  } else if ((txbuff = dep->de_xmitq_head) != NULL) {

	/* Get first packet in queue */
	lock();
	if (dep->de_xmitq_tail == dep->de_xmitq_head)
		dep->de_xmitq_head = dep->de_xmitq_tail = NULL;
	else
		dep->de_xmitq_head = txbuff->next;
	unlock();
	pktsize = txbuff->size;

  } else
	panic("should not be sending ");

  if ((dep->de_flags & DEF_XMIT_BUSY)) {
	if (from_int) panic("should not be sending ");
	getticks(&now);
	if ((now - dep->de_xmit_start) > 4) {
		/* Transmitter timed out */
		DEBUG(printf("3c501: transmitter timed out ... \n"));
		dep->de_stat.ets_sendErr += 1;
		dep->de_flags &= NOT(DEF_XMIT_BUSY);
		el1_reset(dep);
	}

	/* Queue packet */
	lock();			/* Queue packet to receive queue */
	if (dep->de_xmitq_head == NULL)
		dep->de_xmitq_head = txbuff;
	else
		dep->de_xmitq_tail->next = txbuff;
	dep->de_xmitq_tail = txbuff;
	unlock();
  } else {
	/* Save for retransmission */
	TxBuff = txbuff;
	dep->de_flags |= (DEF_XMIT_BUSY | DEF_ACK_SEND);

	/* Setup board for packet loading */
	lock();			/* Buffer to processor */
	outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);
	inb_el1(dep, EL1_RECV);	/* Clears any spurious interrupt */
	inb_el1(dep, EL1_XMIT);
	outw_el1(dep, EL1_RECVPTR, 0);	/* Clears RX packet area */

	/* Loads packet */
	outw_el1(dep, EL1_XMITPTR, (EL1_BFRSIZ - pktsize));
	outsb(dep->de_data_port, SELF, txbuff->buffer, pktsize);
	/* Starts transmitter */
	outw_el1(dep, EL1_XMITPTR, (EL1_BFRSIZ - pktsize));
	outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_XMIT);	/* There it goes... */
	unlock();

	getticks(&dep->de_xmit_start);
	dep->de_flags &= NOT(DEF_SENDING);
  }
  return;
}

/*
**  Name:	void el1_stop(dpeth_t *dep)
**  Function:	Stops board and disable interrupts.
*/
static void el1_stop(dpeth_t * dep)
{
  int ix;

  DEBUG(printf("%s: stopping Etherlink ....\n", dep->de_name));
  for (ix = 0; ix < 8; ix += 1)	/* Reset board */
	outb_el1(dep, EL1_CSR, ECSR_RESET);
  outb_el1(dep, EL1_CSR, ECSR_SYS);
  sys_irqdisable(&dep->de_hook);	/* Disable interrupt */
  return;
}

/*
**  Name:	void el1_interrupt(dpeth_t *dep)
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
			dep->de_stat.ets_collision += 1;
			/* Put pointer back to beginning of packet */
			outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);
			outw_el1(dep, EL1_XMITPTR, (EL1_BFRSIZ - TxBuff->size));
			/* And retrigger transmission */
			outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_XMIT);
			return;

		} else if ((isr & EXSR_16JAM) || !(isr & EXSR_IDLE)) {
			dep->de_stat.ets_sendErr += 1;

		} else if (isr & EXSR_UNDER) {
			dep->de_stat.ets_fifoUnder += 1;
		}
		DEBUG(printf("3c501: got xmit interrupt (0x%02X)\n", isr));
		el1_reset(dep);

	} else {
		/** if (inw_el1(dep, EL1_XMITPTR) == EL1_BFRSIZ) **/
		/* Packet transmitted successfully */
		dep->de_stat.ets_packetT += 1;
		dep->bytes_Tx += (long) (TxBuff->size);
		free_buff(dep, TxBuff);
		dep->de_flags &= NOT(DEF_XMIT_BUSY);
		if ((dep->de_flags & DEF_SENDING) && dep->de_xmitq_head) {
			/* Pending transmit request available in queue */
			el1_send(dep, TRUE, 0);
			if (dep->de_flags & (DEF_XMIT_BUSY | DEF_ACK_SEND))
				return;
		}
	}

  } else if ((csr & (ECSR_RECV | ECSR_XMTBSY)) == (ECSR_RECV | ECSR_XMTBSY)) {

	/* Got a receive interrupt */
	isr = inb_el1(dep, EL1_RECV);
	pktsize = inw_el1(dep, EL1_RECVPTR);
	if ((isr & ERSR_RERROR) || (isr & ERSR_STALE)) {
	DEBUG(printf("Rx0 (ASR=0x%02X RSR=0x%02X size=%d)\n", csr, isr, pktsize));
		dep->de_stat.ets_recvErr += 1;

	} else if (pktsize < ETH_MIN_PACK_SIZE || pktsize > ETH_MAX_PACK_SIZE) {
	DEBUG(printf("Rx1 (ASR=0x%02X RSR=0x%02X size=%d)\n", csr, isr, pktsize));
		dep->de_stat.ets_recvErr += 1;

	} else if ((rxptr = alloc_buff(dep, pktsize + sizeof(buff_t))) == NULL) {
		/* Memory not available. Drop packet */
		dep->de_stat.ets_fifoOver += 1;

	} else if (isr & (ERSR_GOOD | ERSR_ANY)) {
		/* Got a good packet. Read it from buffer */
		outb_el1(dep, EL1_CSR, ECSR_RIDE | ECSR_SYS);
		outw_el1(dep, EL1_XMITPTR, 0);
		insb(dep->de_data_port, SELF, rxptr->buffer, pktsize);
		rxptr->next = NULL;
		rxptr->size = pktsize;
		dep->de_stat.ets_packetR += 1;
		dep->bytes_Rx += (long) pktsize;
		lock();		/* Queue packet to receive queue */
		if (dep->de_recvq_head == NULL)
			dep->de_recvq_head = rxptr;
		else
			dep->de_recvq_tail->next = rxptr;
		dep->de_recvq_tail = rxptr;
		unlock();

		/* Reply to pending Receive requests, if any */
		el1_recv(dep, TRUE, 0);
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
  return;
}

/*
**  Name:	void el1_init(dpeth_t *dep)
**  Function:	Initalizes board hardware and driver data structures.
*/
static void el1_init(dpeth_t * dep)
{
  int ix;

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
         dep->de_name, "3c501", dep->de_base_port, dep->de_irq);
  for (ix = 0; ix < SA_ADDR_LEN; ix += 1)
	printf("%02X%c", (dep->de_address.ea_addr[ix] = StationAddress[ix]),
	       ix < SA_ADDR_LEN - 1 ? ':' : '\n');

  /* Device specific functions */
  dep->de_recvf = el1_recv;
  dep->de_sendf = el1_send;
  dep->de_flagsf = el1_mode_init;
  dep->de_resetf = el1_reset;
  dep->de_getstatsf = el1_getstats;
  dep->de_dumpstatsf = el1_dumpstats;
  dep->de_interruptf = el1_interrupt;

  return;			/* Done */
}

/*
**  Name:	int el1_probe(dpeth_t *dep)
**  Function:	Checks for presence of the board.
*/
int el1_probe(dpeth_t * dep)
{
  int ix;

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
