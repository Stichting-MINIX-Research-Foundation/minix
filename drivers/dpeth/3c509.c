/*
**  File:	3c509.c		Jun. 01, 2000
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  This file contains specific implementation of the ethernet
**  device driver for 3Com Etherlink III (3c509) boards.
**  NOTE: The board has to be setup to disable PnP and to assign
**	  I/O base and IRQ.  The driver is for ISA bus only
*/

#include <minix/drivers.h>
#include <minix/com.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

#include "dp.h"

#if (ENABLE_3C509 == 1)

#include "3c509.h"

static const char *const IfNamesMsg[] = {
	"10BaseT", "AUI", "unknown", "BNC",
};

/*
**  Name:	void el3_update_stats(dpeth_t *dep)
**  Function:	Reads statistic counters from board
**  		and updates local counters.
*/
static void el3_update_stats(dpeth_t * dep)
{

  /* Disables statistics while reading and switches to the correct window */
  outw_el3(dep, REG_CmdStatus, CMD_StatsDisable);
  SetWindow(WNO_Statistics);

  /* Reads everything, adding values to the local counters */
  dep->de_stat.ets_sendErr += inb_el3(dep, REG_TxCarrierLost);	/* Reg. 00 */
  dep->de_stat.ets_sendErr += inb_el3(dep, REG_TxNoCD);		/* Reg. 01 */
  dep->de_stat.ets_collision += inb_el3(dep, REG_TxMultColl);	/* Reg. 02 */
  dep->de_stat.ets_collision += inb_el3(dep, REG_TxSingleColl);	/* Reg. 03 */
  dep->de_stat.ets_collision += inb_el3(dep, REG_TxLate);	/* Reg. 04 */
  dep->de_stat.ets_recvErr += inb_el3(dep, REG_RxDiscarded);	/* Reg. 05 */
  dep->de_stat.ets_packetT += inb_el3(dep, REG_TxFrames);	/* Reg. 06 */
  dep->de_stat.ets_packetR += inb_el3(dep, REG_RxFrames);	/* Reg. 07 */
  dep->de_stat.ets_transDef += inb_el3(dep, REG_TxDefer);	/* Reg. 08 */
  dep->bytes_Rx += (unsigned) inw_el3(dep, REG_RxBytes);	/* Reg. 10 */
  dep->bytes_Tx += (unsigned) inw_el3(dep, REG_TxBytes);	/* Reg. 12 */

  /* Goes back to operating window and enables statistics */
  SetWindow(WNO_Operating);
  outw_el3(dep, REG_CmdStatus, CMD_StatsEnable);

  return;
}

/*
**  Name:	void el3_getstats(dpeth_t *dep)
**  Function:	Reads statistics counters from board.
*/
static void el3_getstats(dpeth_t * dep)
{

  lock();
  el3_update_stats(dep);
  unlock();
  return;
}

/*
**  Name:	void el3_dodump(dpeth_t *dep)
**  Function:	Dumps counter on screen (support for console display).
*/
static void el3_dodump(dpeth_t * dep)
{

  el3_getstats(dep);
  return;
}

/*
**  Name:	void el3_rx_mode(dpeth_t *dep)
**  Function:	Initializes receiver mode
*/
static void el3_rx_mode(dpeth_t * dep)
{

  dep->de_recv_mode = FilterIndividual;
  if (dep->de_flags & DEF_BROAD) dep->de_recv_mode |= FilterBroadcast;
  if (dep->de_flags & DEF_MULTI) dep->de_recv_mode |= FilterMulticast;
  if (dep->de_flags & DEF_PROMISC) dep->de_recv_mode |= FilterPromiscuous;

  outw_el3(dep, REG_CmdStatus, CMD_RxReset);
  outw_el3(dep, REG_CmdStatus, CMD_SetRxFilter | dep->de_recv_mode);
  outw_el3(dep, REG_CmdStatus, CMD_RxEnable);

  return;
}

/*
**  Name:	void el3_reset(dpeth_t *dep)
**  Function:	Reset function specific for Etherlink hardware.
*/
static void el3_reset(dpeth_t * UNUSED(dep))
{

  return;			/* Done */
}

/*
**  Name:	void el3_write_fifo(dpeth_t * dep, int pktsize);
**  Function:	Writes a packet from user area to board.
**  Remark:	Writing a word/dword at a time may result faster
**  		but is a lot more complicated. Let's go simpler way.
*/
static void el3_write_fifo(dpeth_t * dep, int pktsize)
{
  int bytes, ix = 0;
  iovec_dat_s_t *iovp = &dep->de_write_iovec;
  int r, padding = pktsize;

  do {				/* Writes chuncks of packet from user buffers */

	bytes = iovp->iod_iovec[ix].iov_size;	/* Size of buffer */
	if (bytes > pktsize) bytes = pktsize;
	/* Writes from user buffer to Tx FIFO */
	r= sys_safe_outsb(dep->de_data_port, iovp->iod_proc_nr,
		iovp->iod_iovec[ix].iov_grant, 0, bytes);
	if (r != OK)
		panic("el3_write_fifo: sys_safe_outsb failed: %d", r);
		
	if (++ix >= IOVEC_NR) {	/* Next buffer of IO vector */
		dp_next_iovec(iovp);
		ix = 0;
	}
	/* Till packet done */
  } while ((pktsize -= bytes) > 0);
  while ((padding++ % sizeof(long)) != 0) outb(dep->de_data_port, 0x00);
  return;
}

/*
**  Name:	void el3_recv(dpeth_t *dep, int fromint, int size)
**  Function:	Receive function.  Called from interrupt handler or
**  		from main to unload recv. buffer (packet to client)
*/
static void el3_recv(dpeth_t *dep, int fromint, int size)
{
  buff_t *rxptr;

  while ((dep->de_flags & DEF_READING) && (rxptr = dep->de_recvq_head)) {

	lock();			/* Remove buffer from queue */
	if (dep->de_recvq_tail == dep->de_recvq_head)
		dep->de_recvq_head = dep->de_recvq_tail = NULL;
	else
		dep->de_recvq_head = rxptr->next;
	unlock();

	/* Copy buffer to user area and free it */
	mem2user(dep, rxptr);

	dep->de_read_s = rxptr->size;
	dep->de_flags |= DEF_ACK_RECV;
	dep->de_flags &= NOT(DEF_READING);

	/* Return buffer to the idle pool */
	free_buff(dep, rxptr);
  }
  return;
}

/*
**  Name:	void el3_rx_complete(dpeth_t * dep);
**  Function:	Upon receiving a packet, provides status checks
**  		and if packet is OK copies it to local buffer.
*/
static void el3_rx_complete(dpeth_t * dep)
{
  short int RxStatus;
  int pktsize;
  buff_t *rxptr;

  RxStatus = inw_el3(dep, REG_RxStatus);
  pktsize = RxStatus & RXS_Length;	/* Mask off packet length */

  if (RxStatus & RXS_Error) {

	/* First checks for receiving errors */
	RxStatus &= RXS_ErrType;
	switch (RxStatus) {	/* Bad packet (see error type) */
	    case RXS_Dribble:
	    case RXS_Oversize:
	    case RXS_Runt:	dep->de_stat.ets_recvErr += 1;	break;
	    case RXS_Overrun:	dep->de_stat.ets_OVW += 1;	break;
	    case RXS_Framing:	dep->de_stat.ets_frameAll += 1;	break;
	    case RXS_CRC:	dep->de_stat.ets_CRCerr += 1;	break;
	}

  } else if ((rxptr = alloc_buff(dep, pktsize + sizeof(buff_t))) == NULL) {
	/* Memory not available. Drop packet */
	dep->de_stat.ets_fifoOver += 1;

  } else {
	/* Good packet.  Read it from FIFO */
	insb(dep->de_data_port, SELF, rxptr->buffer, pktsize);
	rxptr->next = NULL;
	rxptr->size = pktsize;

	lock();			/* Queue packet to receive queue */
	if (dep->de_recvq_head == NULL)
		dep->de_recvq_head = rxptr;
	else
		dep->de_recvq_tail->next = rxptr;
	dep->de_recvq_tail = rxptr;
	unlock();

	/* Reply to pending Receive requests, if any */
	el3_recv(dep, TRUE, pktsize);
  }

  /* Discard top packet from queue */
  outw_el3(dep, REG_CmdStatus, CMD_RxDiscard);

  return;
}

/*
**  Name:	void el3_send(dpeth_t *dep, int count)
**  Function:	Send function.  Called from main to transit a packet or
**  		from interrupt handler when Tx FIFO gets available.
*/
static void el3_send(dpeth_t * dep, int from_int, int count)
{
  clock_t now;
  int ix;
  short int TxStatus;

  getticks(&now);
  if ((dep->de_flags & DEF_XMIT_BUSY) &&
      (now - dep->de_xmit_start) > 4) {

	DEBUG(printf("3c509:  Transmitter timed out. Resetting ....\n");)
	dep->de_stat.ets_sendErr += 1;
	/* Resets and restars the transmitter */
	outw_el3(dep, REG_CmdStatus, CMD_TxReset);
	outw_el3(dep, REG_CmdStatus, CMD_TxEnable);
	dep->de_flags &= NOT(DEF_XMIT_BUSY);
  }
  if (!(dep->de_flags & DEF_XMIT_BUSY)) {

	/* Writes Transmitter preamble 1st Word (packet len, no ints) */
	outw_el3(dep, REG_TxFIFO, count);
	/* Writes Transmitter preamble 2nd Word (all zero) */
	outw_el3(dep, REG_TxFIFO, 0);
	/* Writes packet */
	el3_write_fifo(dep, count);

	getticks(&dep->de_xmit_start);
	dep->de_flags |= (DEF_XMIT_BUSY | DEF_ACK_SEND);
	if (inw_el3(dep, REG_TxFree) > ETH_MAX_PACK_SIZE) {
		/* Tx has enough room for a packet of maximum size */
		dep->de_flags &= NOT(DEF_XMIT_BUSY | DEF_SENDING);
	} else {
		/* Interrupt driver when enough room is available */
		outw_el3(dep, REG_CmdStatus, CMD_SetTxAvailable | ETH_MAX_PACK_SIZE);
		dep->de_flags &= NOT(DEF_SENDING);
	}

	/* Pops Tx status stack */
	for (ix = 4; --ix && (TxStatus = inb_el3(dep, REG_TxStatus)) > 0;) {
		if (TxStatus & 0x38) dep->de_stat.ets_sendErr += 1;
		if (TxStatus & 0x30)
			outw_el3(dep, REG_CmdStatus, CMD_TxReset);
		if (TxStatus & 0x3C)
			outw_el3(dep, REG_CmdStatus, CMD_TxEnable);
		outb_el3(dep, REG_TxStatus, 0);
	}
  }
  return;
}

/*
**  Name:	void el3_close(dpeth_t *dep)
**  Function:	Stops board and makes it ready to shut down.
*/
static void el3_close(dpeth_t * dep)
{

  /* Disables statistics, Receiver and Transmitter */
  outw_el3(dep, REG_CmdStatus, CMD_StatsDisable);
  outw_el3(dep, REG_CmdStatus, CMD_RxDisable);
  outw_el3(dep, REG_CmdStatus, CMD_TxDisable);

  if (dep->de_if_port == BNC_XCVR) {
	outw_el3(dep, REG_CmdStatus, CMD_StopIntXcvr);
	/* milli_delay(5); */

  } else if (dep->de_if_port == TP_XCVR) {
	SetWindow(WNO_Diagnostics);
	outw_el3(dep, REG_MediaStatus, inw_el3(dep, REG_MediaStatus) &
		 NOT((MediaLBeatEnable | MediaJabberEnable)));
	/* milli_delay(5); */
  }
  DEBUG(printf("%s: stopping Etherlink ... \n", dep->de_name));
  /* Issues a global reset
  outw_el3(dep, REG_CmdStatus, CMD_GlobalReset); */
  sys_irqdisable(&dep->de_hook);	/* Disable interrupt */
 
  return;
}

/*
**  Name:	void el3_interrupt(dpeth_t *dep)
**  Function:	Interrupt handler.  Acknwledges transmit interrupts
**  		or unloads receive buffer to memory queue.
*/
static void el3_interrupt(dpeth_t * dep)
{
  int loop;
  unsigned short isr;

  for (loop = 5; loop > 0 && ((isr = inw_el3(dep, REG_CmdStatus)) &
         (INT_Latch | INT_RxComplete | INT_UpdateStats)); loop -= 1) {

	if (isr & INT_RxComplete)	/* Got a new packet */
		el3_rx_complete(dep);

	if (isr & INT_TxAvailable) {	/* Tx has room for big packets */
		DEBUG(printf("3c509: got Tx interrupt, Status=0x%04x\n", isr);)
		dep->de_flags &= NOT(DEF_XMIT_BUSY);
		outw_el3(dep, REG_CmdStatus, CMD_Acknowledge | INT_TxAvailable);
		if (dep->de_flags & DEF_SENDING)	/* Send pending */
			el3_send(dep, TRUE, dep->de_send_s);
	}
	if (isr & (INT_AdapterFail | INT_RxEarly | INT_UpdateStats)) {

		if (isr & INT_UpdateStats)	/* Empties statistics */
			el3_getstats(dep);

		if (isr & INT_RxEarly)	/* Not really used. Do nothing */
			outw_el3(dep, REG_CmdStatus, CMD_Acknowledge | (INT_RxEarly));

		if (isr & INT_AdapterFail) {
			/* Adapter error. Reset and re-enable receiver */
			DEBUG(printf("3c509: got Rx fail interrupt, Status=0x%04x\n", isr);)
			el3_rx_mode(dep);
			outw_el3(dep, REG_CmdStatus, CMD_Acknowledge | INT_AdapterFail);
		}
	}

	/* Acknowledge interrupt */
	outw_el3(dep, REG_CmdStatus, CMD_Acknowledge | (INT_Latch | INT_Requested));
  }
  return;
}

/*
**  Name:	unsigned el3_read_eeprom(port_t port, unsigned address);
**  Function:	Reads the EEPROM at specified address
*/
static unsigned el3_read_eeprom(port_t port, unsigned address)
{
  unsigned int result;
  int bit;

  address |= EL3_READ_EEPROM;
  outb(port, address);
  milli_delay(5);		/* Allows EEPROM reads */
  for (result = 0, bit = 16; bit > 0; bit -= 1) {
	result = (result << 1) | (inb(port) & 0x0001);
  }
  return result;
}

/*
**  Name:	void el3_read_StationAddress(dpeth_t *dep)
**  Function:	Reads station address from board
*/
static void el3_read_StationAddress(dpeth_t * dep)
{
  unsigned int ix, rc;

  for (ix = EE_3COM_NODE_ADDR; ix < SA_ADDR_LEN+EE_3COM_NODE_ADDR;) {
	/* Accesses with word No. */
	rc = el3_read_eeprom(dep->de_id_port, ix / 2);
	/* Swaps bytes of word */
	dep->de_address.ea_addr[ix++] = (rc >> 8) & 0xFF;
	dep->de_address.ea_addr[ix++] = rc & 0xFF;
  }
  return;
}

/*
**  Name:	void el3_open(dpeth_t *dep)
**  Function:	Initalizes board hardware and driver data structures.
*/
static void el3_open(dpeth_t * dep)
{
  unsigned int AddrCfgReg, ResCfgReg;
  unsigned int ix;

  el3_read_StationAddress(dep);	/* Get ethernet address */

  /* Get address and resource configurations */
  AddrCfgReg = el3_read_eeprom(dep->de_id_port, EE_ADDR_CFG);
  ResCfgReg = el3_read_eeprom(dep->de_id_port, EE_RESOURCE_CFG);
  outb(dep->de_id_port, EL3_ACTIVATE);	/* Activate the board */

  /* Gets xcvr configuration */
  dep->de_if_port = AddrCfgReg & EL3_CONFIG_XCVR_MASK;

  AddrCfgReg = ((AddrCfgReg & EL3_CONFIG_IOBASE_MASK) << 4) + EL3_IO_BASE_ADDR;
  if (AddrCfgReg != dep->de_base_port)
	panic("Bad I/O port for Etherlink board");

  ResCfgReg >>= 12;
  dep->de_irq &= NOT(DEI_DEFAULT);	/* Strips the default flag */
  if (ResCfgReg != dep->de_irq) panic("Bad IRQ for Etherlink board");

  SetWindow(WNO_Setup);

  /* Reset transmitter and receiver */
  outw_el3(dep, REG_CmdStatus, CMD_TxReset);
  outw_el3(dep, REG_CmdStatus, CMD_RxReset);

  /* Enable the adapter */
  outb_el3(dep, REG_CfgControl, EL3_EnableAdapter);
  /* Disable Status bits */
  outw_el3(dep, REG_CmdStatus, CMD_SetStatusEnab + 0x00);

  /* Set "my own" address */
  SetWindow(WNO_StationAddress);
  for (ix = 0; ix < 6; ix += 1)
	outb_el3(dep, REG_SA0_1 + ix, dep->de_address.ea_addr[ix]);

  /* Start Transceivers as required */
  if (dep->de_if_port == BNC_XCVR) {
	/* Start internal transceiver for Coaxial cable */
	outw_el3(dep, REG_CmdStatus, CMD_StartIntXcvr);
	milli_delay(5);

  } else if (dep->de_if_port == TP_XCVR) {
	/* Start internal transceiver for Twisted pair cable */
	SetWindow(WNO_Diagnostics);
	outw_el3(dep, REG_MediaStatus,
		 inw_el3(dep, REG_MediaStatus) | (MediaLBeatEnable | MediaJabberEnable));
  }

  /* Switch to the statistic window, and clear counts (by reading) */
  SetWindow(WNO_Statistics);
  for (ix = REG_TxCarrierLost; ix <= REG_TxDefer; ix += 1) inb_el3(dep, ix);
  inw_el3(dep, REG_RxBytes);
  inw_el3(dep, REG_TxBytes);

  /* Switch to operating window for normal use */
  SetWindow(WNO_Operating);

  /* Receive individual address & broadcast. (Mofified later by rx_mode) */
  outw_el3(dep, REG_CmdStatus, CMD_SetRxFilter |
	 (FilterIndividual | FilterBroadcast));

  /* Turn on statistics */
  outw_el3(dep, REG_CmdStatus, CMD_StatsEnable);

  /* Enable transmitter and receiver */
  outw_el3(dep, REG_CmdStatus, CMD_TxEnable);
  outw_el3(dep, REG_CmdStatus, CMD_RxEnable);

  /* Enable all the status bits */
  outw_el3(dep, REG_CmdStatus, CMD_SetStatusEnab | 0xFF);

  /* Acknowledge all interrupts to clear adapter. Enable interrupts */
  outw_el3(dep, REG_CmdStatus, CMD_Acknowledge | 0xFF);
  outw_el3(dep, REG_CmdStatus, CMD_SetIntMask |
    (INT_Latch | INT_TxAvailable | INT_RxComplete | INT_UpdateStats));

  /* Ready to operate, sets the environment for eth_task */
  dep->de_data_port = dep->de_base_port;
  /* Allocates Rx/Tx buffers */
  init_buff(dep, NULL);

  /* Device specific functions */
  dep->de_recvf = el3_recv;
  dep->de_sendf = el3_send;
  dep->de_flagsf = el3_rx_mode;
  dep->de_resetf = el3_reset;
  dep->de_getstatsf = el3_getstats;
  dep->de_dumpstatsf = el3_dodump;
  dep->de_interruptf = el3_interrupt;

  printf("%s: Etherlink III (%s) at %X:%d, %s port - ",
         dep->de_name, "3c509", dep->de_base_port, dep->de_irq,
         IfNamesMsg[dep->de_if_port >> 14]);
  for (ix = 0; ix < SA_ADDR_LEN; ix += 1)
	printf("%02X%c", dep->de_address.ea_addr[ix],
	       ix < SA_ADDR_LEN - 1 ? ':' : '\n');

  return;			/* Done */
}

/*
**  Name:	unsigned int el3_checksum(port_t port);
**  Function:	Reads EEPROM and computes checksum.
*/
static unsigned short el3_checksum(port_t port)
{
  unsigned short rc, checksum, address;
  unsigned char lo, hi;

  for (checksum = address = 0; address < 15; address += 1) {
	rc = el3_read_eeprom(port, address);
	lo = rc & 0xFF;
	hi = (rc >> 8) & 0xFF;
	if ((address == EE_PROD_ID && (rc & EE_PROD_ID_MASK) != EL3_PRODUCT_ID) ||
	    (address == EE_3COM_CODE && rc != EL3_3COM_CODE))
		return address;
	if (address == EE_ADDR_CFG ||
	    address == EE_RESOURCE_CFG ||
	    address == EE_SW_CONFIG_INFO) {
		lo ^= hi;
		hi = 0;
	} else {
		hi ^= lo;
		lo = 0;
	}
	rc = ((unsigned) hi << 8) + lo;
	checksum ^= rc;
  }
  rc = el3_read_eeprom(port, address);
  return(checksum ^= rc);	/* If OK checksum is 0 */
}

/*
**  Name:	void el3_write_id(port_t port);
**  Function:	Writes the ID sequence to the board.
*/
static void el3_write_id(port_t port)
{
  int ix, pattern;

  outb(port, 0);		/* Selects the ID port */
  outb(port, 0);		/* Resets hardware pattern generator */
  for (pattern = ix = 0x00FF; ix > 0; ix -= 1) {
	outb(port, pattern);
	pattern <<= 1;
	pattern = (pattern & 0x0100) ? pattern ^ 0xCF : pattern;
  }
  return;
}

/*
**  Name:	int el3_probe(dpeth_t *dep)
**  Function:	Checks for presence of the board.
*/
int el3_probe(dpeth_t * dep)
{
  port_t id_port;

  /* Don't ask me what is this for !! */
  outb(0x0279, 0x02);	/* Select PnP config control register. */
  outb(0x0A79, 0x02);	/* Return to WaitForKey state. */
  /* Tests I/O ports in the 0x1xF range for a valid ID port */
  for (id_port = 0x110; id_port < 0x200; id_port += 0x10) {
	outb(id_port, 0x00);
	outb(id_port, 0xFF);
	if (inb(id_port) & 0x01) break;
  }
  if (id_port == 0x200) return 0;	/* No board responding */

  el3_write_id(id_port);
  outb(id_port, EL3_ID_GLOBAL_RESET);	/* Reset the board */
  milli_delay(5);		/* Technical reference says 162 micro sec. */
  el3_write_id(id_port);
  outb(id_port, EL3_SET_TAG_REGISTER);
  milli_delay(5);

  dep->de_id_port = id_port;	/* Stores ID port No. */
  dep->de_ramsize =		/* RAM size is meaningless */
	dep->de_offset_page = 0;
  dep->de_linmem = 0L;		/* Access is via I/O port  */

  /* Device specific functions */
  dep->de_initf = el3_open;
  dep->de_stopf = el3_close;

  return(el3_checksum(id_port) == 0);	/* Etherlink board found/not found */
}

#endif				/* ENABLE_3C509 */

/** 3c509.c **/
