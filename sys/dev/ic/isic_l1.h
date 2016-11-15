/* $NetBSD: isic_l1.h,v 1.21 2012/10/27 17:18:21 chs Exp $ */

/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _ISIC_L1_H
#define _ISIC_L1_H

#include <netisdn/i4b_l3l4.h>

/*---------------------------------------------------------------------------
 *	kernel config file flags definition
 *---------------------------------------------------------------------------*/
#define FLAG_TELES_S0_8		1
#define FLAG_TELES_S0_16	2
#define FLAG_TELES_S0_163	3
#define FLAG_AVM_A1		4
#define FLAG_TELES_S0_163_PnP	5
#define FLAG_CREATIX_S0_PnP	6
#define FLAG_USR_ISDN_TA_INT	7
#define FLAG_DRN_NGO		8
#define FLAG_SWS		9
#define FLAG_AVM_A1_PCMCIA	10
#define FLAG_DYNALINK		11
#define FLAG_BLMASTER		12
#define FLAG_ELSA_QS1P_ISA	13
#define FLAG_ELSA_QS1P_PCI	14
#define FLAG_SIEMENS_ITALK	15
#define	FLAG_ELSA_MLIMC		16
#define	FLAG_ELSA_MLMCALL	17
#define FLAG_ITK_IX1		18
#define FLAG_AVMA1PCI     	19
#define FLAG_ELSA_PCC16		20
#define FLAG_AVM_PNP		21
#define FLAG_SIEMENS_ISURF2	22
#define FLAG_ASUSCOM_IPAC	23

#define SEC_DELAY		1000000	/* one second DELAY for DELAY*/

#define MAX_DFRAME_LEN		264	/* max length of a D frame */

#ifndef __bsdi__
#define min(a,b)		((a)<(b)?(a):(b))
#endif

/* We try to map as few as possible as small as possible io and/or
   memory regions. Each card defines its own interpretation of this
   mapping array. At probe time we have a fixed size array, later
   (when the card type is known) we allocate a minimal array
   dynamically. */

#define	ISIC_MAX_IO_MAPS	49	/* no cardtype needs more yet */

/* one entry in mapping array */
struct isic_io_map {
	bus_space_tag_t t;	/* which bus-space is this? */
	bus_space_handle_t h;	/* handle of mapped bus space region */
	bus_size_t offset;	/* offset into region */
	bus_size_t size;	/* size of region, zero if not ours
				   (i.e.: don't ever unmap it!) */
};

/* this is passed around at probe time (no struct isic_softc yet) */
struct isic_attach_args {
	int ia_flags;			/* flags from config file */
	int ia_num_mappings;		/* number of io mappings provided */
	struct isic_io_map ia_maps[ISIC_MAX_IO_MAPS];
};

/*---------------------------------------------------------------------------*
 *	isic_Bchan: the state of one B channel
 *---------------------------------------------------------------------------*/
typedef struct
{
	int		channel;	/* which channel is this*/

	u_char		hscx_mask;	/* HSCX interrupt mask	*/

	int		bprot;		/* B channel protocol	*/

	int		state;		/* this channels state	*/
#define HSCX_IDLE	0x00		/* channel idle 	*/
#define HSCX_TX_ACTIVE	0x01		/* tx running		*/

	/* receive data from ISDN */

	struct ifqueue	rx_queue;	/* receiver queue	*/

	int		rxcount;	/* rx statistics counter*/

	struct	mbuf	*in_mbuf;	/* rx input buffer	*/
	u_char 		*in_cbptr;	/* curr buffer pointer	*/
	int		in_len;		/* rx input buffer len	*/

	/* transmit data to ISDN */

	struct ifqueue	tx_queue;	/* transmitter queue	*/

	int		txcount;	/* tx statistics counter*/

	struct mbuf	*out_mbuf_head;	/* first mbuf in possible chain	*/
	struct mbuf	*out_mbuf_cur;	/* current mbuf in possbl chain */
	unsigned char	*out_mbuf_cur_ptr; /* data pointer into mbuf	*/
	int		out_mbuf_cur_len; /* remaining bytes in mbuf	*/

	/* link between b channel and driver */

	isdn_link_t	isdn_linktab;		/* b channel driver data	*/
	const struct isdn_l4_driver_functions
			*l4_driver;		/* layer 4 driver		*/
	void		*l4_driver_softc;	/* layer 4 driver instance	*/

	/* statistics */

	/* RSTA */

	int		stat_VFR;	/* HSCX RSTA Valid FRame */
	int		stat_RDO;	/* HSCX RSTA Rx Data Overflow */
	int		stat_CRC;	/* HSCX RSTA CRC */
	int		stat_RAB;	/* HSCX RSTA Rx message ABorted */

	/* EXIR */

	int		stat_XDU;	/* HSCX EXIR tx data underrun */
	int		stat_RFO;	/* HSCX EXIR rx frame overflow */

} l1_bchan_state_t;

/*---------------------------------------------------------------------------*
 *	isic_softc: the state of the layer 1 of the D channel
 *---------------------------------------------------------------------------*/
struct isic_softc
{
	device_t	sc_dev;
	void 		*sc_l3token;	/* pointer to registered L3 instance */
	struct l2_softc	sc_l2;		/* D-channel variables */

	int		sc_irq;		/* interrupt vector	*/
	int		sc_intr_valid;	/* set when card is detached or disable */
#define	ISIC_INTR_VALID		0	/* normal operation */
#define	ISIC_INTR_DISABLED	1	/* ISDN subsystem not opened */
#define	ISIC_INTR_DYING		2	/* card is detaching */

	int sc_num_mappings;		/* number of io mappings provided */
	struct isic_io_map *sc_maps;

#define	MALLOC_MAPS(sc)	\
	(sc)->sc_maps = (struct isic_io_map*)malloc(sizeof((sc)->sc_maps[0])*(sc)->sc_num_mappings, M_DEVBUF, 0)

	int		sc_cardtyp;	/* only needed for some cards	*/
#define CARD_TYPEP_UNK		0	/* unknown			*/
#define CARD_TYPEP_8		1	/* Teles, S0/8 			*/
#define CARD_TYPEP_16		2	/* Teles, S0/16			*/
#define CARD_TYPEP_16_3		3	/* Teles, S0/16.3		*/
#define CARD_TYPEP_AVMA1	4	/* AVM A1 or AVM Fritz!Card	*/
#define CARD_TYPEP_163P		5	/* Teles, S0/16.3 PnP		*/
#define CARD_TYPEP_CS0P		6	/* Creatix, S0 PnP		*/
#define CARD_TYPEP_USRTA	7	/* US Robotics ISDN TA internal	*/
#define CARD_TYPEP_DRNNGO	8	/* Dr. Neuhaus Niccy GO@	*/
#define CARD_TYPEP_SWS		9	/* Sedlbauer Win Speed		*/
#define CARD_TYPEP_DYNALINK	10	/* Dynalink IS64PH		*/
#define CARD_TYPEP_BLMASTER	11	/* ISDN Blaster / ISDN Master	*/
#define	CARD_TYPEP_PCFRITZ	12	/* AVM PCMCIA Fritz!Card	*/
#define CARD_TYPEP_ELSAQS1ISA	13	/* ELSA QuickStep 1000pro ISA	*/
#define CARD_TYPEP_ELSAQS1PCI	14	/* ELSA QuickStep 1000pro PCI	*/
#define CARD_TYPEP_SIEMENSITALK	15	/* Siemens I-Talk		*/
#define	CARD_TYPEP_ELSAMLIMC	16	/* ELSA MicroLink ISDN/MC	*/
#define	CARD_TYPEP_ELSAMLMCALL	17	/* ELSA MicroLink MCall		*/
#define	CARD_TYPEP_ITKIX1	18	/* ITK ix1 micro 		*/
#define CARD_TYPEP_AVMA1PCI	19	/* AVM FRITZ!CARD PCI		*/
#define CARD_TYPEP_PCC16	20	/* ELSA PCC-16			*/
#define CARD_TYPEP_AVM_PNP	21	/* AVM FRITZ!CARD PnP		*/
#define CARD_TYPEP_SIE_ISURF2 	22	/* Siemens I-Surf 2 PnP		*/
#define CARD_TYPEP_ASUSCOMIPAC	23	/* Asuscom ISDNlink 128 K PnP	*/
#define CARD_TYPEP_AVMA1PCIV2	24	/* AVM FRITZ!CARD V2 PCI	*/

	int		sc_bustyp;	/* IOM1 or IOM2		*/
#define BUS_TYPE_IOM1  0x01
#define BUS_TYPE_IOM2  0x02

	int		sc_trace;	/* output protocol data for tracing */
	unsigned int	sc_trace_dcount;/* d channel trace frame counter */
	unsigned int	sc_trace_bcount;/* b channel trace frame counter */

	int		sc_state;	/* ISAC state flag	*/
#define ISAC_IDLE	0x00		/* state = idle */
#define ISAC_TX_ACTIVE	0x01		/* state = transmitter active */

	int		sc_init_tries;	/* no of out tries to access S0 */
	int		sc_maddr;	/* some stupid ISA cards need this */

	u_char		sc_isac_mask;	/* ISAC IRQ mask	*/
#define ISAC_IMASK	(sc->sc_isac_mask)

	l1_bchan_state_t	sc_chan[2];	/* B-channel state	*/
#define HSCX_A_BASE	(sc->sc_chan[0].hscx)
#define HSCX_A_IMASK	(sc->sc_chan[0].hscx_mask)
#define HSCX_B_BASE	(sc->sc_chan[1].hscx)
#define HSCX_B_IMASK	(sc->sc_chan[1].hscx_mask)

	struct mbuf	*sc_ibuf;	/* input buffer mgmt	*/
	u_short		sc_ilen;
	u_char		*sc_ib;
					/* this is for the irq TX routine */
	struct mbuf	*sc_obuf;	/* pointer to an mbuf with TX frame */
	u_char		*sc_op;		/* ptr to next chunk of frame to tx */
	int		sc_ol;		/* length of remaining frame to tx */
	int		sc_freeflag;	/* m_freem mbuf if set */

	struct mbuf	*sc_obuf2;	/* pointer to an mbuf with TX frame */
	int		sc_freeflag2;	/* m_freem mbuf if set */

	int		sc_isac_version;	/* version number of ISAC */
	int		sc_hscx_version;	/* version number of HSCX */
	int		sc_ipac_version;	/* version number of IPAC */

	int		sc_I430state;	/* I.430 state F3 .... F8 */

	int		sc_I430T3;	/* I.430 Timer T3 running */
	struct callout sc_T3_callout;

	int		sc_I430T4;	/* Timer T4 running */
	struct callout sc_T4_callout;

	int		sc_driver_specific;	/* used for LED values */
	struct callout	sc_driver_callout;	/* used for LED timer */

	/*
	 * byte fields for the AVM Fritz!Card PCI. These are packed into
	 * a u_int in the driver.
	 */
	u_char		avma1pp_cmd;
	u_char		avma1pp_txl;
	u_char		avma1pp_prot;

	int		sc_ipac;	/* flag, running on ipac */
	int		sc_bfifolen;	/* length of b channel fifos */

#define	ISIC_WHAT_ISAC	0
#define	ISIC_WHAT_HSCXA	1
#define	ISIC_WHAT_HSCXB	2
#define	ISIC_WHAT_IPAC	3

	u_int8_t	(*readreg)(struct isic_softc *, int, bus_size_t);
	void		(*writereg)(struct isic_softc *, int, bus_size_t, u_int8_t);
	void		(*readfifo)(struct isic_softc *, int, void *, size_t);
	void		(*writefifo)(struct isic_softc *, int what,
			    const void *, size_t);
	void		(*clearirq)(struct isic_softc *);

	void		(*drv_command)(struct isic_softc *, int, void *);

#define	ISAC_READ(r)		(*sc->readreg)(sc, ISIC_WHAT_ISAC, (r))
#define	ISAC_WRITE(r,v)		(*sc->writereg)(sc, ISIC_WHAT_ISAC, (r), (v))
#define	ISAC_RDFIFO(b,s)	(*sc->readfifo)(sc, ISIC_WHAT_ISAC, (b), (s))
#define	ISAC_WRFIFO(b,s)	(*sc->writefifo)(sc, ISIC_WHAT_ISAC, (b), (s))

#define	HSCX_READ(n,r)		(*sc->readreg)(sc, ISIC_WHAT_HSCXA+(n), (r))
#define	HSCX_WRITE(n,r,v)	(*sc->writereg)(sc, ISIC_WHAT_HSCXA+(n), (r), (v))
#define	HSCX_RDFIFO(n,b,s)	(*sc->readfifo)(sc, ISIC_WHAT_HSCXA+(n), (b), (s))
#define	HSCX_WRFIFO(n,b,s)	(*sc->writefifo)(sc, ISIC_WHAT_HSCXA+(n), (b), (s))

#define IPAC_READ(r)		(*sc->readreg)(sc, ISIC_WHAT_IPAC, (r))
#define IPAC_WRITE(r, v)	(*sc->writereg)(sc, ISIC_WHAT_IPAC, (r), (v))

};

/*---------------------------------------------------------------------------*
 *	possible I.430/ISAC states
 *---------------------------------------------------------------------------*/
enum I430states {
	ST_F3,		/* F3 Deactivated	*/
	ST_F4,		/* F4 Awaiting Signal	*/
	ST_F5,		/* F5 Identifying Input */
	ST_F6,		/* F6 Synchronized	*/
	ST_F7,		/* F7 Activated		*/
	ST_F8,		/* F8 Lost Framing	*/
	ST_ILL,		/* Illegal State	*/
	N_STATES
};

/*---------------------------------------------------------------------------*
 *	possible I.430/ISAC events
 *---------------------------------------------------------------------------*/
enum I430events {
	EV_PHAR,	/* PH ACTIVATE REQUEST 		*/
	EV_T3,		/* Timer 3 expired 		*/
	EV_INFO0,	/* receiving INFO0 		*/
	EV_RSY,		/* receiving any signal		*/
	EV_INFO2,	/* receiving INFO2		*/
	EV_INFO48,	/* receiving INFO4 pri 8/9 	*/
	EV_INFO410,	/* receiving INFO4 pri 10/11	*/
	EV_DR,		/* Deactivate Request 		*/
	EV_PU,		/* Power UP			*/
	EV_DIS,		/* Disconnected (only 2085) 	*/
	EV_EI,		/* Error Indication 		*/
	EV_ILL,		/* Illegal Event 		*/
	N_EVENTS
};

enum I430commands {
	CMD_TIM,	/*	Timing				*/
	CMD_RS,		/*	Reset				*/
	CMD_AR8,	/*	Activation request pri 8	*/
	CMD_AR10,	/*	Activation request pri 10	*/
	CMD_DIU,	/*	Deactivate Indication Upstream	*/
	CMD_ILL		/*	Illegal command			*/
};

#define N_COMMANDS CMD_ILL

extern void isic_recover(struct isic_softc *);
extern int isicintr(void *);
extern int isicprobe(struct isic_attach_args *);
extern int isic_attach_avma1(struct isic_softc *);
extern int isic_attach_s016(struct isic_softc *);
extern int isic_attach_s0163(struct isic_softc *);
extern int isic_attach_s08(struct isic_softc *);
extern int isic_attach_usrtai(struct isic_softc *);
extern int isic_attach_itkix1(struct isic_softc *);
extern void isic_bchannel_setup(void *, int, int, int);
extern void isic_hscx_init(struct isic_softc *, int, int);
extern void isic_hscx_irq(struct isic_softc *, u_char ista, int, u_char);
extern void isic_hscx_cmd( struct isic_softc *, int, unsigned char);
extern void isic_hscx_waitxfw( struct isic_softc *, int);
extern void isic_init_linktab(struct isic_softc *);
extern int isic_isac_init(struct isic_softc *);
extern void isic_isac_irq(struct isic_softc *, int);
extern void isic_isac_l1_cmd(struct isic_softc *, int);
extern void isic_next_state(struct isic_softc *, int);
extern const char * isic_printstate(struct isic_softc *);
extern int isic_probe_avma1(struct isic_attach_args *);
extern int isic_probe_s016(struct isic_attach_args *);
extern int isic_probe_s0163(struct isic_attach_args *);
extern int isic_probe_s08(struct isic_attach_args *ia);
extern int isic_probe_usrtai(struct isic_attach_args *ia);
extern int isic_probe_itkix1(struct isic_attach_args *ia);
extern int isic_attach_bri(struct isic_softc *sc, const char *cardname, const struct isdn_layer1_isdnif_driver *dchan_driver);
extern int isic_detach_bri(struct isic_softc *sc);

extern void isic_isacsx_disable_intr(struct isic_softc *sc);
extern void isic_isacsx_recover(struct isic_softc *sc);
extern void isic_isacsx_irq(struct isic_softc *sc, int r);
extern void isic_isacsx_l1_cmd(struct isic_softc *sc, int command);
extern int isic_isacsx_init(struct isic_softc *sc);

#endif /* !_ISIC_L1_H */

