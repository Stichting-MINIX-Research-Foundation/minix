/*	$NetBSD: dlreg.h,v 1.6 2005/12/11 12:23:29 christos Exp $	*/
/*
 * Ben Harris, 1997
 *
 * This file is in the Public Domain.
 */
/*
 * dlreg.h -- Definitions for the DL11 and DLV11 serial cards.
 *
 * Style in imitation of dzreg.h.
 */

#ifdef notdef
union w_b
{
	u_short word;
	struct {
		u_char byte_lo;
		u_char byte_hi;
	} bytes;
};

struct DLregs
{
	volatile u_short dl_rcsr; /* Receive Control/Status Register (R/W) */
	volatile u_short dl_rbuf; /* Receive Buffer (R) */
	volatile u_short dl_xcsr; /* Transmit Control/Status Register (R/W) */
	volatile union w_b u_xbuf; /* Transmit Buffer (W) */
#define dl_xbuf u_xbuf.bytes.byte_lo
};

typedef struct DLregs dlregs;
#endif

#define	DL_UBA_RCSR	0
#define	DL_UBA_RBUF	2
#define	DL_UBA_XCSR	4
#define	DL_UBA_XBUFL	6

/* RCSR bits */

#define DL_RCSR_RX_DONE		0x0080 /* Receiver Done (R) */
#define DL_RCSR_RXIE		0x0040 /* Receiver Interrupt Enable (R/W) */
#define DL_RCSR_READER_ENABLE	0x0001 /* [paper-tape] Reader Enable (W) */
#define DL_RCSR_BITS		"\20\1READER_ENABLE\7RXIE\10RX_DONE\n"

/* RBUF bits */

#define DL_RBUF_ERR		0x8000 /* Error (R) */
#define DL_RBUF_OVERRUN_ERR	0x4000 /* Overrun Error (R) */
#define DL_RBUF_FRAMING_ERR	0x2000 /* Framing Error (R) */
#define DL_RBUF_PARITY_ERR	0x1000 /* Parity Error (R) */
#define DL_RBUF_DATA_MASK	0x00FF /* Receive Data (R) */
#define DL_RBUF_BITS	"\20\15PARITY_ERR\16FRAMING_ERR\17OVERRUN_ERR\20ERR\n"

/* XCSR bits */

#define DL_XCSR_TX_READY	0x0080 /* Transmitter Ready (R) */
#define DL_XCSR_TXIE		0x0040 /* Transmit Interrupt Enable (R/W) */
#define DL_XCSR_TX_BREAK	0x0001 /* Transmit Break (R/W) */
#define DL_XCSR_BITS		"\20\1TX_BREAK\7TXIE\10TX_READY\n"

/* XBUF is just data byte right justified. */
