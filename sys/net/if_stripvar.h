/*	$NetBSD: if_stripvar.h,v 1.19 2007/07/14 21:02:42 ad Exp $	*/

#ifndef _NET_IF_STRIPVAR_H_
#define _NET_IF_STRIPVAR_H_

/*
 * Definitions for STRIP interface data structures
 */
struct strip_softc {
	struct	ifnet sc_if;		/* network-visible interface */
	int	sc_unit;		/* XXX unit number */
	struct	ifqueue sc_fastq;	/* interactive output queue */
	struct	ifqueue sc_inq;		/* input queue */
	struct	tty *sc_ttyp;		/* pointer to tty structure */
	struct	callout sc_timo_ch;	/* timeout callout */
	u_char	*sc_mp;			/* pointer to next available buf char */
	u_char	*sc_ep;			/* pointer to last available buf char */
	u_char	*sc_pktstart;		/* pointer to beginning of packet */
	struct mbuf *sc_mbuf;		/* input buffer */
	u_char	*sc_rxbuf;		/* input destuffing buffer */
	u_char	*sc_txbuf;		/* output stuffing buffer */
	u_int	sc_flags;		/* see below */
	long	sc_oqlen;		/* previous output queue size */
	long	sc_otimeout;		/* number of times output's stalled */
	void	*sc_si;			/* softintr handle */
#ifdef __NetBSD__
	int	sc_oldbufsize;		/* previous output buffer size */
	int	sc_oldbufquot;		/* previous output buffer quoting */
#endif
#ifdef INET				/* XXX */
	struct	slcompress sc_comp;	/* tcp compression data */
#endif

	int sc_state;			/* Radio reset state-machine */
#define ST_ALIVE	0x0		/*    answered  probe */
#define ST_PROBE_SENT	0x1		/*    probe sent, answer pending */
#define ST_DEAD		0x2		/*    no answer to probe; do reset */

	long sc_statetimo;		/* When (secs) current state ends */

	struct bintime sc_lastpacket;	/* for watchdog */
	LIST_ENTRY(strip_softc) sc_iflist;
};


/* Internal flags */
#define	SC_ERROR	0x0001		/* Incurred error reading current pkt*/

#define SC_TIMEOUT	0x00000400	/* timeout is currently pending */

/* visible flags */
#define	SC_COMPRESS	IFF_LINK0	/* compress TCP traffic */
#define	SC_NOICMP	IFF_LINK1	/* supress ICMP traffic */
#define	SC_AUTOCOMP	IFF_LINK2	/* auto-enable TCP compression */

#ifdef _KERNEL
void	stripattach(void);
#endif /* _KERNEL */

#endif /* !_NET_IF_STRIPVAR_H_ */
