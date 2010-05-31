/*
**  File:	3c501.h		Jan. 14, 1997
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  Interface description for 3Com Etherlink boards
**
**  $Log$
**  Revision 1.1  2005/06/29 10:16:46  beng
**  Import of dpeth 3c501/3c509b/.. ethernet driver by
**  Giovanni Falzoni <fgalzoni@inwind.it>.
**
**  Revision 2.0  2005/06/26 16:16:46  lsodgf0
**  Initial revision for Minix 3.0.6
*/

/* The various board command registers					 */
#define EL1_ADDRESS	0x00	/* Board station address, 6 bytes	 */
#define EL1_RECV	0x06	/* Board Receive Config/Status Reg.	 */
#define EL1_XMIT	0x07	/* Board Transmit Config/Status Reg.	 */
#define EL1_XMITPTR	0x08	/* Transmit buffer pointer (word access) */
#define EL1_RECVPTR	0x0A	/* Receive buffer pointer (word access)	 */
#define EL1_SAPROM	0x0C	/* Window on Station Addr prom		 */
#define EL1_CSR		0x0E	/* Board Command/Status Register	 */
#define EL1_DATAPORT	0x0F	/* Window on packet buffer (Data Port)	 */

/* Bits in EL1_RECV, interrupt enable on write, status when read	 */
#define ERSR_NONE	0x00	/* Match mode in bits 5-6 (wo)		 */
#define ERSR_ALL	0x40	/* Promiscuous receive (wo)		 */
#define ERSR_BROAD	0x80	/* Station address plus broadcast (wo)	 */
#define ERSR_MULTI	0x80	/* Station address plus multicast 0xC0	 */
#define ERSR_STALE	0x80	/* Receive status previously read (ro)	 */
#define ERSR_GOOD	0x20	/* Well formed packets only (rw)	 */
#define ERSR_ANY	0x10	/* Any packet, even with errors (rw)	 */
#define ERSR_SHORT	0x08	/* Short frame (rw)			 */
#define ERSR_DRIBBLE	0x04	/* Dribble error (rw)			 */
#define ERSR_FCS	0x02	/* CRC error (rw)			 */
#define ERSR_OVER	0x01	/* Data overflow (rw)			 */

#define ERSR_RERROR	(ERSR_SHORT|ERSR_DRIBBLE|ERSR_FCS|ERSR_OVER)
#define ERSR_RMASK	(ERSR_GOOD|ERSR_RERROR)/*(ERSR_GOOD|ERSR_ANY|ERSR_RERROR)*/

/* Bits in EL1_XMIT, interrupt enable on write, status when read	 */
#define EXSR_IDLE	0x08	/* Transmit idle (send completed)	 */
#define EXSR_16JAM	0x04	/* Packet sending got 16 collisions	 */
#define EXSR_JAM	0x02	/* Packet sending got a collision	 */
#define EXSR_UNDER	0x01	/* Data underflow in sending		 */

/* Bits in EL1_CSR (Configuration Status Register)			 */
#define ECSR_RESET	0x80	/* Reset the controller (wo)		 */
#define ECSR_XMTBSY	0x80	/* Transmitter busy (ro)		 */
#define ECSR_RIDE	0x01	/* Request interrupt/DMA enable (rw)	 */
#define ECSR_DMA	0x20	/* DMA request (rw)			 */
#define ECSR_EDMA	0x10	/* DMA done (ro)			 */
#define ECSR_CRC	0x02	/* Causes CRC error on transmit (wo)	 */
#define ECSR_RCVBSY	0x01	/* Receive in progress (ro)		 */
#define ECSR_LOOP	(3<<2)	/* 2 bit field in bits 2,3, loopback	 */
#define ECSR_RECV	(2<<2)	/* Gives buffer to receiver (rw)	 */
#define ECSR_XMIT	(1<<2)	/* Gives buffer to transmit (rw)	 */
#define ECSR_SYS	(0<<2)	/* Gives buffer to processor (wo)	 */

#define EL1_BFRSIZ	2048	/* Number of bytes in board buffer	 */

#define inb_el1(dep,reg) (inb(dep->de_base_port+(reg)))
#define inw_el1(dep,reg) (inw(dep->de_base_port+(reg)))
#define outb_el1(dep,reg,data) (outb(dep->de_base_port+(reg),data))
#define outw_el1(dep,reg,data) (outw(dep->de_base_port+(reg),data))

/** 3c501.h **/
