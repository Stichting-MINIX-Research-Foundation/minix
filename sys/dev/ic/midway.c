/*	$NetBSD: midway.c,v 1.94 2012/03/13 18:40:31 elad Exp $	*/
/*	(sync'd to midway.c 1.68)	*/

/*
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 * m i d w a y . c   e n i 1 5 5   d r i v e r
 *
 * author: Chuck Cranor <chuck@netbsd>
 * started: spring, 1996 (written from scratch).
 *
 * notes from the author:
 *   Extra special thanks go to Werner Almesberger, EPFL LRC.   Werner's
 *   ENI driver was especially useful in figuring out how this card works.
 *   I would also like to thank Werner for promptly answering email and being
 *   generally helpful.
 */
/*
 *  1997/12/02, major update on 1999/04/06 kjc
 *    new features added:
 *	- BPF support (link type is DLT_ATM_RFC1483)
 *	  BPF understands only LLC/SNAP!! (because bpf can't
 *	  handle variable link header length.)
 *	  (bpfwrite should work if atm_pseudohdr and LLC/SNAP are prepended.)
 *	- support vc shaping
 *	- integrate IPv6 support.
 *	- support pvc sub interface
 *
 *	  initial work on per-pvc-interface for ipv6 was done
 *	  by Katsushi Kobayashi <ikob@cc.uec.ac.jp> of the WIDE Project.
 * 	  some of the extensions for pvc subinterfaces are merged from
 *	  the CAIRN project written by Suresh Bhogavilli (suresh@isi.edu).
 *
 *    code cleanup:
 *	- remove WMAYBE related code.  ENI WMAYBE DMA doesn't work.
 *	- remove updating if_lastchange for every packet.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: midway.c,v 1.94 2012/03/13 18:40:31 elad Exp $");

#include "opt_natm.h"

#undef	EN_DEBUG
#undef	EN_DEBUG_RANGE		/* check ranges on en_read/en_write's? */
#define	EN_MBUF_OPT		/* try and put more stuff in mbuf? */
#define	EN_DIAG
#define	EN_STAT
#ifndef EN_DMA
#define EN_DMA		1	/* use DMA? */
#endif
#define EN_NOTXDMA	0	/* hook to disable tx DMA only */
#define EN_NORXDMA	0	/* hook to disable rx DMA only */
#define EN_NOWMAYBE	1	/* hook to disable word maybe DMA */
				/* XXX: WMAYBE doesn't work, needs debugging */
#define EN_DDBHOOK	1	/* compile in ddb functions */
#if defined(MIDWAY_ADPONLY)
#define EN_ENIDMAFIX	0	/* no ENI cards to worry about */
#else
#define EN_ENIDMAFIX	1	/* avoid byte DMA on the ENI card (see below) */
#endif

/*
 * note on EN_ENIDMAFIX: the byte aligner on the ENI version of the card
 * appears to be broken.   it works just fine if there is no load... however
 * when the card is loaded the data get corrupted.   to see this, one only
 * has to use "telnet" over ATM.   do the following command in "telnet":
 * 	cat /usr/share/misc/termcap
 * "telnet" seems to generate lots of 1023 byte mbufs (which make great
 * use of the byte aligner).   watch "netstat -s" for checksum errors.
 *
 * I further tested this by adding a function that compared the transmit
 * data on the card's SRAM with the data in the mbuf chain _after_ the
 * "transmit DMA complete" interrupt.   using the "telnet" test I got data
 * mismatches where the byte-aligned data should have been.   using ddb
 * and en_dumpmem() I verified that the DTQs fed into the card were
 * absolutely correct.   thus, we are forced to concluded that the ENI
 * hardware is buggy.   note that the Adaptec version of the card works
 * just fine with byte DMA.
 *
 * bottom line: we set EN_ENIDMAFIX to 1 to avoid byte DMAs on the ENI
 * card.
 */

#if defined(DIAGNOSTIC) && !defined(EN_DIAG)
#define EN_DIAG			/* link in with master DIAG option */
#endif
#ifdef EN_STAT
#define EN_COUNT(X) (X)++
#else
#define EN_COUNT(X) /* nothing */
#endif

#ifdef EN_DEBUG
#undef	EN_DDBHOOK
#define	EN_DDBHOOK	1
#define STATIC /* nothing */
#define INLINE /* nothing */
#else /* EN_DEBUG */
#define STATIC static
#define INLINE inline
#endif /* EN_DEBUG */

#ifdef __FreeBSD__
#include "en.h"
#endif

#ifdef __NetBSD__
#include "opt_ddb.h"
#include "opt_inet.h"
#endif

#if NEN > 0 || !defined(__FreeBSD__)

#include <sys/param.h>
#include <sys/systm.h>
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
#include <sys/device.h>
#endif
#if defined(__FreeBSD__)
#include <sys/sockio.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/kauth.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_atm.h>

#ifdef __NetBSD__
#include <uvm/uvm_extern.h>
#else
#include <vm/vm.h>
#endif

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/if_atm.h>
#ifdef INET6
#include <netinet6/in6_var.h>
#endif
#endif

#ifdef NATM
#if !(defined(INET) || defined(INET6))
#include <netinet/in.h>
#endif
#include <netnatm/natm.h>
#endif


#if !defined(__FreeBSD__)
#include <sys/bus.h>

#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/midwayreg.h>
#include <dev/ic/midwayvar.h>
#if defined(__alpha__)
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vaddr_t)(va))
#endif
#elif defined(__FreeBSD__)
#include <machine/cpufunc.h>            /* for rdtsc proto for clock.h below */
#include <machine/clock.h>              /* for DELAY */
#include <dev/en/midwayreg.h>
#include <dev/en/midwayvar.h>
#include <vm/pmap.h>			/* for vtophys proto */

/*
 * 2.1.x does not have if_softc.   detect this by seeing if IFF_NOTRAILERS
 * is defined, as per kjc.
 */
#ifdef IFF_NOTRAILERS
#define MISSING_IF_SOFTC
#else
#define IFF_NOTRAILERS 0
#endif

#endif	/* __FreeBSD__ */

#ifdef ATM_PVCEXT
# ifndef NATM
   /* this is for for __KAME__ */
#  include <netinet/in.h>
# endif
# if defined (__KAME__) && defined(INET6)
#  include <netinet6/in6_ifattach.h>
# endif
#endif /*ATM_PVCEXT*/

#include <net/bpf.h>

/*
 * params
 */

#ifndef EN_TXHIWAT
#define EN_TXHIWAT	(64*1024)	/* max 64 KB waiting to be DMAd out */
#endif

#ifndef EN_MINDMA
#define EN_MINDMA	32	/* don't DMA anything less than this (bytes) */
#endif

#define RX_NONE		0xffff	/* recv VC not in use */

#define EN_OBHDR	ATM_PH_DRIVER7  /* TBD in first mbuf ! */
#define EN_OBTRL	ATM_PH_DRIVER8  /* PDU trailer in last mbuf ! */

#define ENOTHER_FREE	0x01		/* free rxslot */
#define ENOTHER_DRAIN	0x02		/* almost free (drain DRQ DMA) */
#define ENOTHER_RAW	0x04		/* 'raw' access  (aka boodi mode) */
#define ENOTHER_SWSL	0x08		/* in software service list */

int en_dma = EN_DMA;			/* use DMA (switch off for dbg) */

/*
 * autoconfig attachments
 */

extern struct cfdriver en_cd;

/*
 * local structures
 */

/*
 * params to en_txlaunch() function
 */

struct en_launch {
  u_int32_t tbd1;		/* TBD 1 */
  u_int32_t tbd2;		/* TBD 2 */
  u_int32_t pdu1;		/* PDU 1 (aal5) */
  int nodma;			/* don't use DMA */
  int need;			/* total space we need (pad out if less data) */
  int mlen;			/* length of mbuf (for dtq) */
  struct mbuf *t;		/* data */
  u_int32_t aal;		/* aal code */
  u_int32_t atm_vci;		/* vci */
  u_int8_t atm_flags;		/* flags */
};


/*
 * DMA table (index by # of words)
 *
 * plan A: use WMAYBE
 * plan B: avoid WMAYBE
 */

struct en_dmatab {
  u_int8_t bcode;		/* code */
  u_int8_t divshift;		/* byte divisor */
};

static struct en_dmatab en_dma_planA[] = {
  { 0, 0 },		/* 0 */		{ MIDDMA_WORD, 2 },	/* 1 */
  { MIDDMA_2WORD, 3},	/* 2 */		{ MIDDMA_4WMAYBE, 2},	/* 3 */
  { MIDDMA_4WORD, 4},	/* 4 */		{ MIDDMA_8WMAYBE, 2},	/* 5 */
  { MIDDMA_8WMAYBE, 2},	/* 6 */		{ MIDDMA_8WMAYBE, 2},	/* 7 */
  { MIDDMA_8WORD, 5},   /* 8 */		{ MIDDMA_16WMAYBE, 2},	/* 9 */
  { MIDDMA_16WMAYBE,2},	/* 10 */	{ MIDDMA_16WMAYBE, 2},	/* 11 */
  { MIDDMA_16WMAYBE,2},	/* 12 */	{ MIDDMA_16WMAYBE, 2},	/* 13 */
  { MIDDMA_16WMAYBE,2},	/* 14 */	{ MIDDMA_16WMAYBE, 2},	/* 15 */
  { MIDDMA_16WORD, 6},  /* 16 */
};

static struct en_dmatab en_dma_planB[] = {
  { 0, 0 },		/* 0 */		{ MIDDMA_WORD, 2},	/* 1 */
  { MIDDMA_2WORD, 3},	/* 2 */		{ MIDDMA_WORD, 2},	/* 3 */
  { MIDDMA_4WORD, 4},	/* 4 */		{ MIDDMA_WORD, 2},	/* 5 */
  { MIDDMA_2WORD, 3},	/* 6 */		{ MIDDMA_WORD, 2},	/* 7 */
  { MIDDMA_8WORD, 5},   /* 8 */		{ MIDDMA_WORD, 2},	/* 9 */
  { MIDDMA_2WORD, 3},	/* 10 */	{ MIDDMA_WORD, 2},	/* 11 */
  { MIDDMA_4WORD, 4},	/* 12 */	{ MIDDMA_WORD, 2},	/* 13 */
  { MIDDMA_2WORD, 3},	/* 14 */	{ MIDDMA_WORD, 2},	/* 15 */
  { MIDDMA_16WORD, 6},  /* 16 */
};

static struct en_dmatab *en_dmaplan = en_dma_planA;

/*
 * prototypes
 */

STATIC INLINE	int en_b2sz(int) __unused;
#ifdef EN_DDBHOOK
		int en_dump(int,int);
		int en_dumpmem(int,int,int);
#endif
STATIC		void en_dmaprobe(struct en_softc *);
STATIC		int en_dmaprobe_doit(struct en_softc *, u_int8_t *,
		    u_int8_t *, int);
STATIC INLINE	int en_dqneed(struct en_softc *, void *, u_int,
		    u_int) __unused;
STATIC		void en_init(struct en_softc *);
STATIC		int en_ioctl(struct ifnet *, EN_IOCTL_CMDT, void *);
STATIC INLINE	int en_k2sz(int) __unused;
STATIC		void en_loadvc(struct en_softc *, int);
STATIC		int en_mfix(struct en_softc *, struct mbuf **,
		    struct mbuf *);
STATIC INLINE	struct mbuf *en_mget(struct en_softc *, u_int,
		    u_int *) __unused;
STATIC INLINE	u_int32_t en_read(struct en_softc *,
		    u_int32_t) __unused;
STATIC		int en_rxctl(struct en_softc *, struct atm_pseudoioctl *, int);
STATIC		void en_txdma(struct en_softc *, int);
STATIC		void en_txlaunch(struct en_softc *, int, struct en_launch *);
STATIC		void en_service(struct en_softc *);
STATIC		void en_start(struct ifnet *);
STATIC INLINE	int en_sz2b(int) __unused;
STATIC INLINE	void en_write(struct en_softc *, u_int32_t,
		    u_int32_t) __unused;

#ifdef ATM_PVCEXT
static void rrp_add(struct en_softc *, struct ifnet *);
static struct ifnet *en_pvcattach(struct ifnet *);
static int en_txctl(struct en_softc *, int, int, int);
static int en_pvctx(struct en_softc *, struct pvctxreq *);
static int en_pvctxget(struct en_softc *, struct pvctxreq *);
static int en_pcr2txspeed(int);
static int en_txspeed2pcr(int);
static struct ifnet *en_vci2ifp(struct en_softc *, int);
#endif

/*
 * macros/inline
 */

/*
 * raw read/write macros
 */

#define EN_READDAT(SC,R) en_read(SC,R)
#define EN_WRITEDAT(SC,R,V) en_write(SC,R,V)

/*
 * cooked read/write macros
 */

#define EN_READ(SC,R) ntohl(en_read(SC,R))
#define EN_WRITE(SC,R,V) en_write(SC,R, htonl(V))

#define EN_WRAPADD(START,STOP,CUR,VAL) { \
	(CUR) = (CUR) + (VAL); \
	if ((CUR) >= (STOP)) \
		(CUR) = (START) + ((CUR) - (STOP)); \
	}

#define WORD_IDX(START, X) (((X) - (START)) / sizeof(u_int32_t))

/* we store sc->dtq and sc->drq data in the following format... */
#define EN_DQ_MK(SLOT,LEN) (((SLOT) << 20)|(LEN)|(0x80000))
					/* the 0x80000 ensures we != 0 */
#define EN_DQ_SLOT(X) ((X) >> 20)
#define EN_DQ_LEN(X) ((X) & 0x3ffff)

/* format of DTQ/DRQ word 1 differs between ENI and ADP */
#if defined(MIDWAY_ENIONLY)

#define MID_MK_TXQ(SC,CNT,CHAN,END,BCODE) \
	EN_WRITE((SC), (SC)->dtq_us, \
		MID_MK_TXQ_ENI((CNT), (CHAN), (END), (BCODE)));

#define MID_MK_RXQ(SC,CNT,VCI,END,BCODE) \
	EN_WRITE((SC), (SC)->drq_us, \
		MID_MK_RXQ_ENI((CNT), (VCI), (END), (BCODE)));

#elif defined(MIDWAY_ADPONLY)

#define MID_MK_TXQ(SC,CNT,CHAN,END,JK) \
	EN_WRITE((SC), (SC)->dtq_us, \
		MID_MK_TXQ_ADP((CNT), (CHAN), (END), (JK)));

#define MID_MK_RXQ(SC,CNT,VCI,END,JK) \
	EN_WRITE((SC), (SC)->drq_us, \
		MID_MK_RXQ_ADP((CNT), (VCI), (END), (JK)));

#else

#define MID_MK_TXQ(SC,CNT,CHAN,END,JK_OR_BCODE) { \
	if ((SC)->is_adaptec) \
	  EN_WRITE((SC), (SC)->dtq_us, \
		  MID_MK_TXQ_ADP((CNT), (CHAN), (END), (JK_OR_BCODE))); \
	else \
	  EN_WRITE((SC), (SC)->dtq_us, \
		  MID_MK_TXQ_ENI((CNT), (CHAN), (END), (JK_OR_BCODE))); \
	}

#define MID_MK_RXQ(SC,CNT,VCI,END,JK_OR_BCODE) { \
	if ((SC)->is_adaptec) \
	  EN_WRITE((SC), (SC)->drq_us, \
		  MID_MK_RXQ_ADP((CNT), (VCI), (END), (JK_OR_BCODE))); \
	else \
	  EN_WRITE((SC), (SC)->drq_us, \
		   MID_MK_RXQ_ENI((CNT), (VCI), (END), (JK_OR_BCODE))); \
	}

#endif

/* add an item to the DTQ */
#define EN_DTQADD(SC,CNT,CHAN,JK_OR_BCODE,ADDR,LEN,END) { \
	if (END) \
	  (SC)->dtq[MID_DTQ_A2REG((SC)->dtq_us)] = EN_DQ_MK(CHAN,LEN); \
	MID_MK_TXQ(SC,CNT,CHAN,END,JK_OR_BCODE); \
	(SC)->dtq_us += 4; \
	EN_WRITE((SC), (SC)->dtq_us, (ADDR)); \
	EN_WRAPADD(MID_DTQOFF, MID_DTQEND, (SC)->dtq_us, 4); \
	(SC)->dtq_free--; \
	if (END) \
	  EN_WRITE((SC), MID_DMA_WRTX, MID_DTQ_A2REG((SC)->dtq_us)); \
}

/* DRQ add macro */
#define EN_DRQADD(SC,CNT,VCI,JK_OR_BCODE,ADDR,LEN,SLOT,END) { \
	if (END) \
	  (SC)->drq[MID_DRQ_A2REG((SC)->drq_us)] = EN_DQ_MK(SLOT,LEN); \
	MID_MK_RXQ(SC,CNT,VCI,END,JK_OR_BCODE); \
	(SC)->drq_us += 4; \
	EN_WRITE((SC), (SC)->drq_us, (ADDR)); \
	EN_WRAPADD(MID_DRQOFF, MID_DRQEND, (SC)->drq_us, 4); \
	(SC)->drq_free--; \
	if (END) \
	  EN_WRITE((SC), MID_DMA_WRRX, MID_DRQ_A2REG((SC)->drq_us)); \
}

/*
 * the driver code
 *
 * the code is arranged in a specific way:
 * [1] short/inline functions
 * [2] autoconfig stuff
 * [3] ioctl stuff
 * [4] reset -> init -> transmit -> intr -> receive functions
 *
 */

/***********************************************************************/

/*
 * en_read: read a word from the card.   this is the only function
 * that reads from the card.
 */

STATIC INLINE u_int32_t en_read(struct en_softc *sc, uint32_t r)
{

#ifdef EN_DEBUG_RANGE
  if (r > MID_MAXOFF || (r % 4))
    panic("en_read out of range, r=0x%x", r);
#endif

  return(bus_space_read_4(sc->en_memt, sc->en_base, r));
}

/*
 * en_write: write a word to the card.   this is the only function that
 * writes to the card.
 */

STATIC INLINE void en_write(struct en_softc *sc, uint32_t r, uint32_t v)
{
#ifdef EN_DEBUG_RANGE
  if (r > MID_MAXOFF || (r % 4))
    panic("en_write out of range, r=0x%x", r);
#endif

  bus_space_write_4(sc->en_memt, sc->en_base, r, v);
}

/*
 * en_k2sz: convert KBytes to a size parameter (a log2)
 */

STATIC INLINE int en_k2sz(int k)
{
  switch(k) {
    case 1:   return(0);
    case 2:   return(1);
    case 4:   return(2);
    case 8:   return(3);
    case 16:  return(4);
    case 32:  return(5);
    case 64:  return(6);
    case 128: return(7);
    default: panic("en_k2sz");
  }
  return(0);
}
#define en_log2(X) en_k2sz(X)


/*
 * en_b2sz: convert a DMA burst code to its byte size
 */

STATIC INLINE int en_b2sz(int b)
{
  switch (b) {
    case MIDDMA_WORD:   return(1*4);
    case MIDDMA_2WMAYBE:
    case MIDDMA_2WORD:  return(2*4);
    case MIDDMA_4WMAYBE:
    case MIDDMA_4WORD:  return(4*4);
    case MIDDMA_8WMAYBE:
    case MIDDMA_8WORD:  return(8*4);
    case MIDDMA_16WMAYBE:
    case MIDDMA_16WORD: return(16*4);
    default: panic("en_b2sz");
  }
  return(0);
}


/*
 * en_sz2b: convert a burst size (bytes) to DMA burst code
 */

STATIC INLINE int en_sz2b(int sz)
{
  switch (sz) {
    case 1*4:  return(MIDDMA_WORD);
    case 2*4:  return(MIDDMA_2WORD);
    case 4*4:  return(MIDDMA_4WORD);
    case 8*4:  return(MIDDMA_8WORD);
    case 16*4: return(MIDDMA_16WORD);
    default: panic("en_sz2b");
  }
  return(0);
}


/*
 * en_dqneed: calculate number of DTQ/DRQ's needed for a buffer
 */

STATIC INLINE int en_dqneed(struct en_softc *sc, void *data, u_int len, u_int tx)
{
  int result, needalign, sz;

#if !defined(MIDWAY_ENIONLY)
#if !defined(MIDWAY_ADPONLY)
    if (sc->is_adaptec)
#endif /* !MIDWAY_ADPONLY */
      return(1);	/* adaptec can DMA anything in one go */
#endif

#if !defined(MIDWAY_ADPONLY)
    result = 0;
    if (len < EN_MINDMA) {
      if (!tx)			/* XXX: conservative */
        return(1);		/* will copy/DMA_JK */
    }

    if (tx) {			/* byte burst? */
      needalign = (((unsigned long) data) % sizeof(u_int32_t));
      if (needalign) {
        result++;
        sz = min(len, sizeof(u_int32_t) - needalign);
        len -= sz;
        data = (char *)data + sz;
      }
    }

    if (sc->alburst && len) {
      needalign = (((unsigned long) data) & sc->bestburstmask);
      if (needalign) {
	result++;		/* alburst */
        sz = min(len, sc->bestburstlen - needalign);
        len -= sz;
      }
    }

    if (len >= sc->bestburstlen) {
      sz = len / sc->bestburstlen;
      sz = sz * sc->bestburstlen;
      len -= sz;
      result++;			/* best shot */
    }

    if (len) {
      result++;			/* clean up */
      if (tx && (len % sizeof(u_int32_t)) != 0)
        result++;		/* byte cleanup */
    }

    return(result);
#endif	/* !MIDWAY_ADPONLY */
}


/*
 * en_mget: get an mbuf chain that can hold totlen bytes and return it
 * (for recv)   [based on am7990_get from if_le and ieget from if_ie]
 * after this call the sum of all the m_len's in the chain will be totlen.
 */

STATIC INLINE struct mbuf *en_mget(struct en_softc *sc, u_int totlen, u_int *drqneed)
{
  struct mbuf *m;
  struct mbuf *top, **mp;
  *drqneed = 0;

  MGETHDR(m, M_DONTWAIT, MT_DATA);
  if (m == NULL)
    return(NULL);
  m->m_pkthdr.rcvif = &sc->enif;
  m->m_pkthdr.len = totlen;
  m->m_len = MHLEN;
  top = NULL;
  mp = &top;

  /* if (top != NULL) then we've already got 1 mbuf on the chain */
  while (totlen > 0) {
    if (top) {
      MGET(m, M_DONTWAIT, MT_DATA);
      if (!m) {
	m_freem(top);
	return(NULL);	/* out of mbufs */
      }
      m->m_len = MLEN;
    }
    if (totlen >= MINCLSIZE) {
      MCLGET(m, M_DONTWAIT);
      if ((m->m_flags & M_EXT) == 0) {
	m_free(m);
	m_freem(top);
	return(NULL);	/* out of mbuf clusters */
      }
      m->m_len = MCLBYTES;
    }
    m->m_len = min(totlen, m->m_len);
    totlen -= m->m_len;
    *mp = m;
    mp = &m->m_next;

    *drqneed += en_dqneed(sc, m->m_data, m->m_len, 0);

  }
  return(top);
}

/***********************************************************************/

/*
 * autoconfig stuff
 */

void en_attach(struct en_softc *sc)
{
  struct ifnet *ifp = &sc->enif;
  int sz;
  u_int32_t reg, lcv, check, ptr, sav, midvloc;

  /*
   * probe card to determine memory size.   the stupid ENI card always
   * reports to PCI that it needs 4MB of space (2MB regs and 2MB RAM).
   * if it has less than 2MB RAM the addresses wrap in the RAM address space.
   * (i.e. on a 512KB card addresses 0x3ffffc, 0x37fffc, and 0x2ffffc
   * are aliases for 0x27fffc  [note that RAM starts at offset 0x200000]).
   */

  if (sc->en_busreset)
    sc->en_busreset(sc);
  EN_WRITE(sc, MID_RESID, 0x0);	/* reset card before touching RAM */
  for (lcv = MID_PROBEOFF; lcv <= MID_MAXOFF ; lcv += MID_PROBSIZE) {
    EN_WRITE(sc, lcv, lcv);	/* data[address] = address */
    for (check = MID_PROBEOFF ; check < lcv ; check += MID_PROBSIZE) {
      reg = EN_READ(sc, check);
      if (reg != check) {		/* found an alias! */
	goto done_probe;		/* and quit */
      }
    }
  }
done_probe:
  lcv -= MID_PROBSIZE;			/* take one step back */
  sc->en_obmemsz = (lcv + 4) - MID_RAMOFF;

  /*
   * determine the largest DMA burst supported
   */

  en_dmaprobe(sc);

  /*
   * "hello world"
   */

  if (sc->en_busreset)
    sc->en_busreset(sc);
  EN_WRITE(sc, MID_RESID, 0x0);		/* reset */
  for (lcv = MID_RAMOFF ; lcv < MID_RAMOFF + sc->en_obmemsz ; lcv += 4)
    EN_WRITE(sc, lcv, 0);	/* zero memory */

  reg = EN_READ(sc, MID_RESID);

  aprint_normal_dev(sc->sc_dev, 
      "ATM midway v%d, board IDs %d.%d, %s%s%s, %ldKB on-board RAM\n",
	MID_VER(reg), MID_MID(reg), MID_DID(reg),
	(MID_IS_SABRE(reg)) ? "sabre controller, " : "",
	(MID_IS_SUNI(reg)) ? "SUNI" : "Utopia",
	(!MID_IS_SUNI(reg) && MID_IS_UPIPE(reg)) ? " (pipelined)" : "",
	(u_long)sc->en_obmemsz / 1024);

  if (sc->is_adaptec) {
    if (sc->bestburstlen == 64 && sc->alburst == 0)
      aprint_normal_dev(sc->sc_dev, "passed 64 byte DMA test\n");
    else
      aprint_error_dev(sc->sc_dev, "FAILED DMA TEST: burst=%d, alburst=%d\n",
	    sc->bestburstlen, sc->alburst);
  } else {
    aprint_normal_dev(sc->sc_dev, "maximum DMA burst length = %d bytes%s\n",
	  sc->bestburstlen, (sc->alburst) ? " (must align)" : "");
  }

#if 0		/* WMAYBE doesn't work, don't complain about it */
  /* check if en_dmaprobe disabled wmaybe */
  if (en_dmaplan == en_dma_planB)
    aprint_normal_dev(sc->sc_dev, "note: WMAYBE DMA has been disabled\n");
#endif

  /*
   * link into network subsystem and prepare card
   */

#if defined(__NetBSD__) || defined(__OpenBSD__)
  strlcpy(sc->enif.if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
#endif
#if !defined(MISSING_IF_SOFTC)
  sc->enif.if_softc = sc;
#endif
  ifp->if_flags = IFF_SIMPLEX|IFF_NOTRAILERS;
  ifp->if_ioctl = en_ioctl;
  ifp->if_output = atm_output;
  ifp->if_start = en_start;
  IFQ_SET_READY(&ifp->if_snd);

  /*
   * init softc
   */

  for (lcv = 0 ; lcv < MID_N_VC ; lcv++) {
    sc->rxvc2slot[lcv] = RX_NONE;
    sc->txspeed[lcv] = 0;	/* full */
    sc->txvc2slot[lcv] = 0;	/* full speed == slot 0 */
  }

  sz = sc->en_obmemsz - (MID_BUFOFF - MID_RAMOFF);
  ptr = sav = MID_BUFOFF;
  ptr = roundup(ptr, EN_TXSZ * 1024);	/* align */
  sz = sz - (ptr - sav);
  if (EN_TXSZ*1024 * EN_NTX > sz) {
    aprint_error_dev(sc->sc_dev, "EN_NTX/EN_TXSZ too big\n");
    return;
  }
  for (lcv = 0 ; lcv < EN_NTX ; lcv++) {
    sc->txslot[lcv].mbsize = 0;
    sc->txslot[lcv].start = ptr;
    ptr += (EN_TXSZ * 1024);
    sz -= (EN_TXSZ * 1024);
    sc->txslot[lcv].stop = ptr;
    sc->txslot[lcv].nref = 0;
#ifdef ATM_PVCEXT
    sc->txrrp = NULL;
#endif
    memset(&sc->txslot[lcv].indma, 0, sizeof(sc->txslot[lcv].indma));
    memset(&sc->txslot[lcv].q, 0, sizeof(sc->txslot[lcv].q));
#ifdef EN_DEBUG
    aprint_debug_dev(sc->sc_dev, "tx%d: start 0x%x, stop 0x%x\n", lcv,
		sc->txslot[lcv].start, sc->txslot[lcv].stop);
#endif
  }

  sav = ptr;
  ptr = roundup(ptr, EN_RXSZ * 1024);	/* align */
  sz = sz - (ptr - sav);
  sc->en_nrx = sz / (EN_RXSZ * 1024);
  if (sc->en_nrx <= 0) {
    aprint_error_dev(sc->sc_dev, "EN_NTX/EN_TXSZ/EN_RXSZ too big\n");
    return;
  }

  /*
   * ensure that there is always one VC slot on the service list free
   * so that we can tell the difference between a full and empty list.
   */
  if (sc->en_nrx >= MID_N_VC)
    sc->en_nrx = MID_N_VC - 1;

  for (lcv = 0 ; lcv < sc->en_nrx ; lcv++) {
    sc->rxslot[lcv].rxhand = NULL;
    sc->rxslot[lcv].oth_flags = ENOTHER_FREE;
    memset(&sc->rxslot[lcv].indma, 0, sizeof(sc->rxslot[lcv].indma));
    memset(&sc->rxslot[lcv].q, 0, sizeof(sc->rxslot[lcv].q));
    midvloc = sc->rxslot[lcv].start = ptr;
    ptr += (EN_RXSZ * 1024);
    sz -= (EN_RXSZ * 1024);
    sc->rxslot[lcv].stop = ptr;
    midvloc = midvloc - MID_RAMOFF;
    midvloc = (midvloc & ~((EN_RXSZ*1024) - 1)) >> 2; /* mask, cvt to words */
    midvloc = midvloc >> MIDV_LOCTOPSHFT;  /* we only want the top 11 bits */
    midvloc = (midvloc & MIDV_LOCMASK) << MIDV_LOCSHIFT;
    sc->rxslot[lcv].mode = midvloc |
	(en_k2sz(EN_RXSZ) << MIDV_SZSHIFT) | MIDV_TRASH;

#ifdef EN_DEBUG
    aprint_debug_dev(sc->sc_dev, "rx%d: start 0x%x, stop 0x%x, mode 0x%x\n",
	lcv, sc->rxslot[lcv].start, sc->rxslot[lcv].stop, sc->rxslot[lcv].mode);
#endif
  }

#ifdef EN_STAT
  sc->vtrash = sc->otrash = sc->mfix = sc->txmbovr = sc->dmaovr = 0;
  sc->txoutspace = sc->txdtqout = sc->launch = sc->lheader = sc->ltail = 0;
  sc->hwpull = sc->swadd = sc->rxqnotus = sc->rxqus = sc->rxoutboth = 0;
  sc->rxdrqout = sc->ttrash = sc->rxmbufout = sc->mfixfail = 0;
  sc->headbyte = sc->tailbyte = sc->tailflush = 0;
#endif
  sc->need_drqs = sc->need_dtqs = 0;

  aprint_normal_dev(sc->sc_dev,
	"%d %dKB receive buffers, %d %dKB transmit buffers allocated\n",
	sc->en_nrx, EN_RXSZ, EN_NTX, EN_TXSZ);

  aprint_normal_dev(sc->sc_dev, "End Station Identifier (mac address) %s\n",
        ether_sprintf(sc->macaddr));

  /*
   * final commit
   */

  if_attach(ifp);
  atm_ifattach(ifp);

#ifdef ATM_PVCEXT
  rrp_add(sc, ifp);
#endif
}


/*
 * en_dmaprobe: helper function for en_attach.
 *
 * see how the card handles DMA by running a few DMA tests.   we need
 * to figure out the largest number of bytes we can DMA in one burst
 * ("bestburstlen"), and if the starting address for a burst needs to
 * be aligned on any sort of boundary or not ("alburst").
 *
 * typical findings:
 * sparc1: bestburstlen=4, alburst=0 (ick, broken DMA!)
 * sparc2: bestburstlen=64, alburst=1
 * p166:   bestburstlen=64, alburst=0
 */

STATIC void en_dmaprobe(struct en_softc *sc)
{
  u_int32_t srcbuf[64], dstbuf[64];
  u_int8_t *sp, *dp;
  int bestalgn, bestnotalgn, lcv, try, fail;

  sc->alburst = 0;

  sp = (u_int8_t *) srcbuf;
  while ((((unsigned long) sp) % MIDDMA_MAXBURST) != 0)
    sp += 4;
  dp = (u_int8_t *) dstbuf;
  while ((((unsigned long) dp) % MIDDMA_MAXBURST) != 0)
    dp += 4;

  bestalgn = bestnotalgn = en_dmaprobe_doit(sc, sp, dp, 0);

  for (lcv = 4 ; lcv < MIDDMA_MAXBURST ; lcv += 4) {
    try = en_dmaprobe_doit(sc, sp+lcv, dp+lcv, 0);
    if (try < bestnotalgn)
      bestnotalgn = try;
  }

  if (bestalgn != bestnotalgn) 		/* need bursts aligned */
    sc->alburst = 1;

  sc->bestburstlen = bestalgn;
  sc->bestburstshift = en_log2(bestalgn);
  sc->bestburstmask = sc->bestburstlen - 1; /* must be power of 2 */
  sc->bestburstcode = en_sz2b(bestalgn);

  if (sc->bestburstlen <= 2*sizeof(u_int32_t))
    return;				/* won't be using WMAYBE */

  /*
   * adaptec does not have (or need) wmaybe.   do not bother testing
   * for it.
   */
  if (sc->is_adaptec) {
    /* XXX, actually don't need a DMA plan: adaptec is smarter than that */
    en_dmaplan = en_dma_planB;
    return;
  }

  /*
   * test that WMAYBE DMA works like we think it should
   * (i.e. no alignment restrictions on host address other than alburst)
   */

  try = sc->bestburstlen - 4;
  fail = 0;
  fail += en_dmaprobe_doit(sc, sp, dp, try);
  for (lcv = 4 ; lcv < sc->bestburstlen ; lcv += 4) {
    fail += en_dmaprobe_doit(sc, sp+lcv, dp+lcv, try);
    if (sc->alburst)
      try -= 4;
  }
  if (EN_NOWMAYBE || fail) {
    if (fail)
      aprint_error_dev(sc->sc_dev, "WARNING: WMAYBE DMA test failed %d time(s)\n",
	fail);
    en_dmaplan = en_dma_planB;		/* fall back to plan B */
  }

}


/*
 * en_dmaprobe_doit: do actual testing
 */

int
en_dmaprobe_doit(struct en_softc *sc, uint8_t *sp, uint8_t *dp, int wmtry)
{
  int lcv, retval = 4, cnt, count;
  u_int32_t reg, bcode, midvloc;

  /*
   * set up a 1k buffer at MID_BUFOFF
   */

  if (sc->en_busreset)
    sc->en_busreset(sc);
  EN_WRITE(sc, MID_RESID, 0x0);	/* reset card before touching RAM */

  midvloc = ((MID_BUFOFF - MID_RAMOFF) / sizeof(u_int32_t)) >> MIDV_LOCTOPSHFT;
  EN_WRITE(sc, MIDX_PLACE(0), MIDX_MKPLACE(en_k2sz(1), midvloc));
  EN_WRITE(sc, MID_VC(0), (midvloc << MIDV_LOCSHIFT)
		| (en_k2sz(1) << MIDV_SZSHIFT) | MIDV_TRASH);
  EN_WRITE(sc, MID_DST_RP(0), 0);
  EN_WRITE(sc, MID_WP_ST_CNT(0), 0);

  for (lcv = 0 ; lcv < 68 ; lcv++) 		/* set up sample data */
    sp[lcv] = lcv+1;
  EN_WRITE(sc, MID_MAST_CSR, MID_MCSR_ENDMA);	/* enable DMA (only) */

  sc->drq_chip = MID_DRQ_REG2A(EN_READ(sc, MID_DMA_RDRX));
  sc->dtq_chip = MID_DTQ_REG2A(EN_READ(sc, MID_DMA_RDTX));

  /*
   * try it now . . .  DMA it out, then DMA it back in and compare
   *
   * note: in order to get the DMA stuff to reverse directions it wants
   * the "end" flag set!   since we are not DMA'ing valid data we may
   * get an ident mismatch interrupt (which we will ignore).
   *
   * note: we've got two different tests rolled up in the same loop
   * if (wmtry)
   *   then we are doing a wmaybe test and wmtry is a byte count
   *   else we are doing a burst test
   */

  for (lcv = 8 ; lcv <= MIDDMA_MAXBURST ; lcv = lcv * 2) {

    /* zero SRAM and dest buffer */
    for (cnt = 0 ; cnt < 1024; cnt += 4)
      EN_WRITE(sc, MID_BUFOFF+cnt, 0);	/* zero memory */
    for (cnt = 0 ; cnt < 68  ; cnt++)
      dp[cnt] = 0;

    if (wmtry) {
      count = (sc->bestburstlen - sizeof(u_int32_t)) / sizeof(u_int32_t);
      bcode = en_dmaplan[count].bcode;
      count = wmtry >> en_dmaplan[count].divshift;
    } else {
      bcode = en_sz2b(lcv);
      count = 1;
    }
    if (sc->is_adaptec)
      EN_WRITE(sc, sc->dtq_chip, MID_MK_TXQ_ADP(lcv, 0, MID_DMA_END, 0));
    else
      EN_WRITE(sc, sc->dtq_chip, MID_MK_TXQ_ENI(count, 0, MID_DMA_END, bcode));
    EN_WRITE(sc, sc->dtq_chip+4, vtophys((vaddr_t)sp));
    EN_WRITE(sc, MID_DMA_WRTX, MID_DTQ_A2REG(sc->dtq_chip+8));
    cnt = 1000;
    while (EN_READ(sc, MID_DMA_RDTX) == MID_DTQ_A2REG(sc->dtq_chip)) {
      DELAY(1);
      cnt--;
      if (cnt == 0) {
	aprint_error_dev(sc->sc_dev, "unexpected timeout in tx DMA test\n");
	return(retval);		/* timeout, give up */
      }
    }
    EN_WRAPADD(MID_DTQOFF, MID_DTQEND, sc->dtq_chip, 8);
    reg = EN_READ(sc, MID_INTACK);
    if ((reg & MID_INT_DMA_TX) != MID_INT_DMA_TX) {
      aprint_error_dev(sc->sc_dev, "unexpected status in tx DMA test: 0x%x\n",
		reg);
      return(retval);
    }
    EN_WRITE(sc, MID_MAST_CSR, MID_MCSR_ENDMA);   /* re-enable DMA (only) */

    /* "return to sender..."  address is known ... */

    if (sc->is_adaptec)
      EN_WRITE(sc, sc->drq_chip, MID_MK_RXQ_ADP(lcv, 0, MID_DMA_END, 0));
    else
      EN_WRITE(sc, sc->drq_chip, MID_MK_RXQ_ENI(count, 0, MID_DMA_END, bcode));
    EN_WRITE(sc, sc->drq_chip+4, vtophys((vaddr_t)dp));
    EN_WRITE(sc, MID_DMA_WRRX, MID_DRQ_A2REG(sc->drq_chip+8));
    cnt = 1000;
    while (EN_READ(sc, MID_DMA_RDRX) == MID_DRQ_A2REG(sc->drq_chip)) {
      DELAY(1);
      cnt--;
      if (cnt == 0) {
	aprint_error_dev(sc->sc_dev, "unexpected timeout in rx DMA test\n");
	return(retval);		/* timeout, give up */
      }
    }
    EN_WRAPADD(MID_DRQOFF, MID_DRQEND, sc->drq_chip, 8);
    reg = EN_READ(sc, MID_INTACK);
    if ((reg & MID_INT_DMA_RX) != MID_INT_DMA_RX) {
      aprint_error_dev(sc->sc_dev, "unexpected status in rx DMA test: 0x%x\n",
		reg);
      return(retval);
    }
    EN_WRITE(sc, MID_MAST_CSR, MID_MCSR_ENDMA);   /* re-enable DMA (only) */

    if (wmtry) {
      return(memcmp(sp, dp, wmtry));  /* wmtry always exits here, no looping */
    }

    if (memcmp(sp, dp, lcv))
      return(retval);		/* failed, use last value */

    retval = lcv;

  }
  return(retval);		/* studly 64 byte DMA present!  oh baby!! */
}

/***********************************************************************/

/*
 * en_ioctl: handle ioctl requests
 *
 * NOTE: if you add an ioctl to set txspeed, you should choose a new
 * TX channel/slot.   Choose the one with the lowest sc->txslot[slot].nref
 * value, subtract one from sc->txslot[0].nref, add one to the
 * sc->txslot[slot].nref, set sc->txvc2slot[vci] = slot, and then set
 * txspeed[vci].
 */

STATIC int en_ioctl(struct ifnet *ifp, EN_IOCTL_CMDT cmd, void *data)
{
#ifdef MISSING_IF_SOFTC
    struct en_softc *sc = (struct en_softc *)device_lookup_private(&en_cd, ifp->if_unit);
#else
    struct en_softc *sc = (struct en_softc *) ifp->if_softc;
#endif
    struct ifaddr *ifa = (struct ifaddr *) data;
    struct ifreq *ifr = (struct ifreq *) data;
    struct atm_pseudoioctl *api = (struct atm_pseudoioctl *)data;
#ifdef NATM
    struct atm_rawioctl *ario = (struct atm_rawioctl *)data;
    int slot;
#endif
    int s, error = 0;

    s = splnet();

    switch (cmd) {
	case SIOCATMENA:		/* enable circuit for recv */
		error = en_rxctl(sc, api, 1);
		break;

	case SIOCATMDIS: 		/* disable circuit for recv */
		error = en_rxctl(sc, api, 0);
		break;

#ifdef NATM
	case SIOCXRAWATM:
		if ((slot = sc->rxvc2slot[ario->npcb->npcb_vci]) == RX_NONE) {
			error = EINVAL;
			break;
		}
		if (ario->rawvalue > EN_RXSZ*1024)
			ario->rawvalue = EN_RXSZ*1024;
		if (ario->rawvalue) {
			sc->rxslot[slot].oth_flags |= ENOTHER_RAW;
			sc->rxslot[slot].raw_threshold = ario->rawvalue;
		} else {
			sc->rxslot[slot].oth_flags &= (~ENOTHER_RAW);
			sc->rxslot[slot].raw_threshold = 0;
		}
#ifdef EN_DEBUG
		printf("%s: rxvci%d: turn %s raw (boodi) mode\n",
			device_xname(sc->sc_dev), ario->npcb->npcb_vci,
			(ario->rawvalue) ? "on" : "off");
#endif
		break;
#endif
	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		en_reset(sc);
		en_init(sc);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ifa->ifa_rtrequest = atm_rtrequest; /* ??? */
			break;
#endif
#ifdef INET6
		case AF_INET6:
			ifa->ifa_rtrequest = atm_rtrequest; /* ??? */
			break;
#endif
		default:
			/* what to do if not INET? */
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
#ifdef ATM_PVCEXT
	  	/* point-2-point pvc is allowed to change if_flags */
		if (((ifp->if_flags & IFF_UP) && !(ifp->if_flags & IFF_RUNNING))
		||  (!(ifp->if_flags & IFF_UP) && (ifp->if_flags & IFF_RUNNING))) {
			en_reset(sc);
			en_init(sc);
		}
#else
		error = EINVAL;
#endif
		break;

#if defined(SIOCSIFMTU)		/* ??? copied from if_de */
#if !defined(ifr_mtu)
#define ifr_mtu ifr_metric
#endif
	case SIOCSIFMTU:
	    /*
	     * Set the interface MTU.
	     */
#ifdef notsure
	    if (ifr->ifr_mtu > ATMMTU) {
		error = EINVAL;
		break;
	    }
#endif
	    if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET) {
		error = 0;
		/* XXXCDC: do we really need to reset on MTU size change? */
		en_reset(sc);
		en_init(sc);
	    }
	    break;
#endif /* SIOCSIFMTU */

#ifdef ATM_PVCEXT
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp == &sc->enif || ifr == 0) {
			error = EAFNOSUPPORT;	/* XXX */
			break;
		}
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCGPVCSIF:
		if (ifp != &sc->enif) {
#ifdef __NetBSD__
		  strlcpy(ifr->ifr_name, sc->enif.if_xname,
		      sizeof(ifr->ifr_name));
#else
		  snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "%s%d",
			  sc->enif.if_name, sc->enif.if_unit);
#endif
		}
		else
		  error = EINVAL;
		break;

	case SIOCSPVCSIF:
		if (ifp == &sc->enif) {
		  struct ifnet *sifp;

		  if ((error = kauth_authorize_network(curlwp->l_cred,
		     KAUTH_NETWORK_INTERFACE_PVC, KAUTH_REQ_NETWORK_INTERFACE_PVC_ADD,
		     NULL, NULL, NULL)) != 0)
		    break;

		  if ((sifp = en_pvcattach(ifp)) != NULL) {
#ifdef __NetBSD__
		    strlcpy(ifr->ifr_name, sifp->if_xname,
		        sizeof(ifr->ifr_name));
#else
		    snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "%s%d",
		        sifp->if_name, sifp->if_unit);
#endif
#if defined(__KAME__) && defined(INET6)
		    /* get EUI64 for PVC, from ATM hardware interface */
		    in6_ifattach(sifp, ifp);
#endif
		  }
		  else
		    error = ENOMEM;
		}
		else
		  error = EINVAL;
		break;

	case SIOCGPVCTX:
		error = en_pvctxget(sc, (struct pvctxreq *)data);
		break;

	case SIOCSPVCTX:
		if ((error = kauth_authorize_network(curlwp->l_cred,
		    KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp, KAUTH_ARG(cmd),
		    NULL)) == 0)
			error = en_pvctx(sc, (struct pvctxreq *)data);
		break;

#endif /* ATM_PVCEXT */

	default:
	    error = ifioctl_common(ifp, cmd, data);
	    break;
    }
    splx(s);
    return error;
}


/*
 * en_rxctl: turn on and off VCs for recv.
 */

STATIC int en_rxctl(struct en_softc *sc, struct atm_pseudoioctl *pi, int on)
{
  u_int s, vci, flags, slot;
  u_int32_t oldmode, newmode;

  vci = ATM_PH_VCI(&pi->aph);
  flags = ATM_PH_FLAGS(&pi->aph);

#ifdef EN_DEBUG
  printf("%s: %s vpi=%d, vci=%d, flags=%d\n", device_xname(sc->sc_dev),
	(on) ? "enable" : "disable", ATM_PH_VPI(&pi->aph), vci, flags);
#endif

  if (ATM_PH_VPI(&pi->aph) || vci >= MID_N_VC)
    return(EINVAL);

  /*
   * turn on VCI!
   */

  if (on) {
    if (sc->rxvc2slot[vci] != RX_NONE)
      return(EINVAL);
    for (slot = 0 ; slot < sc->en_nrx ; slot++)
      if (sc->rxslot[slot].oth_flags & ENOTHER_FREE)
	break;
    if (slot == sc->en_nrx)
      return(ENOSPC);
    sc->rxvc2slot[vci] = slot;
    sc->rxslot[slot].rxhand = NULL;
    oldmode = sc->rxslot[slot].mode;
    newmode = (flags & ATM_PH_AAL5) ? MIDV_AAL5 : MIDV_NOAAL;
    sc->rxslot[slot].mode = MIDV_SETMODE(oldmode, newmode);
    sc->rxslot[slot].atm_vci = vci;
    sc->rxslot[slot].atm_flags = flags;
    sc->rxslot[slot].oth_flags = 0;
    sc->rxslot[slot].rxhand = pi->rxhand;
    if (sc->rxslot[slot].indma.ifq_head || sc->rxslot[slot].q.ifq_head)
      panic("en_rxctl: left over mbufs on enable");
    sc->txspeed[vci] = 0;	/* full speed to start */
    sc->txvc2slot[vci] = 0;	/* init value */
    sc->txslot[0].nref++;	/* bump reference count */
    en_loadvc(sc, vci);		/* does debug printf for us */
    return(0);
  }

  /*
   * turn off VCI
   */

  if (sc->rxvc2slot[vci] == RX_NONE)
    return(EINVAL);
  slot = sc->rxvc2slot[vci];
  if ((sc->rxslot[slot].oth_flags & (ENOTHER_FREE|ENOTHER_DRAIN)) != 0)
    return(EINVAL);
  s = splnet();		/* block out enintr() */
  oldmode = EN_READ(sc, MID_VC(vci));
  newmode = MIDV_SETMODE(oldmode, MIDV_TRASH) & ~MIDV_INSERVICE;
  EN_WRITE(sc, MID_VC(vci), (newmode | (oldmode & MIDV_INSERVICE)));
		/* halt in tracks, be careful to preserve inserivce bit */
  DELAY(27);
  sc->rxslot[slot].rxhand = NULL;
  sc->rxslot[slot].mode = newmode;

  sc->txslot[sc->txvc2slot[vci]].nref--;
  sc->txspeed[vci] = 0;
  sc->txvc2slot[vci] = 0;

  /* if stuff is still going on we are going to have to drain it out */
  if (sc->rxslot[slot].indma.ifq_head ||
		sc->rxslot[slot].q.ifq_head ||
		(sc->rxslot[slot].oth_flags & ENOTHER_SWSL) != 0) {
    sc->rxslot[slot].oth_flags |= ENOTHER_DRAIN;
  } else {
    sc->rxslot[slot].oth_flags = ENOTHER_FREE;
    sc->rxslot[slot].atm_vci = RX_NONE;
    sc->rxvc2slot[vci] = RX_NONE;
  }
  splx(s);		/* enable enintr() */
#ifdef EN_DEBUG
  printf("%s: rx%d: VCI %d is now %s\n", device_xname(sc->sc_dev), slot, vci,
	(sc->rxslot[slot].oth_flags & ENOTHER_DRAIN) ? "draining" : "free");
#endif
  return(0);
}

/***********************************************************************/

/*
 * en_reset: reset the board, throw away work in progress.
 * must en_init to recover.
 */

void en_reset(struct en_softc *sc)
{
  struct mbuf *m;
  int lcv, slot;

#ifdef EN_DEBUG
  printf("%s: reset\n", device_xname(sc->sc_dev));
#endif

  if (sc->en_busreset)
    sc->en_busreset(sc);
  EN_WRITE(sc, MID_RESID, 0x0);	/* reset hardware */

  /*
   * recv: dump any mbufs we are DMA'ing into, if DRAINing, then a reset
   * will free us!
   */

  for (lcv = 0 ; lcv < MID_N_VC ; lcv++) {
    if (sc->rxvc2slot[lcv] == RX_NONE)
      continue;
    slot = sc->rxvc2slot[lcv];
    while (1) {
      IF_DEQUEUE(&sc->rxslot[slot].indma, m);
      if (m == NULL)
	break;		/* >>> exit 'while(1)' here <<< */
      m_freem(m);
    }
    while (1) {
      IF_DEQUEUE(&sc->rxslot[slot].q, m);
      if (m == NULL)
	break;		/* >>> exit 'while(1)' here <<< */
      m_freem(m);
    }
    sc->rxslot[slot].oth_flags &= ~ENOTHER_SWSL;
    if (sc->rxslot[slot].oth_flags & ENOTHER_DRAIN) {
      sc->rxslot[slot].oth_flags = ENOTHER_FREE;
      sc->rxvc2slot[lcv] = RX_NONE;
#ifdef EN_DEBUG
  printf("%s: rx%d: VCI %d is now free\n", device_xname(sc->sc_dev), slot, lcv);
#endif
    }
  }

  /*
   * xmit: dump everything
   */

  for (lcv = 0 ; lcv < EN_NTX ; lcv++) {
    while (1) {
      IF_DEQUEUE(&sc->txslot[lcv].indma, m);
      if (m == NULL)
	break;		/* >>> exit 'while(1)' here <<< */
      m_freem(m);
    }
    while (1) {
      IF_DEQUEUE(&sc->txslot[lcv].q, m);
      if (m == NULL)
	break;		/* >>> exit 'while(1)' here <<< */
      m_freem(m);
    }
    sc->txslot[lcv].mbsize = 0;
  }

  return;
}


/*
 * en_init: init board and sync the card with the data in the softc.
 */

STATIC void en_init(struct en_softc *sc)
{
  int vc, slot;
  u_int32_t loc;
#ifdef ATM_PVCEXT
    struct pvcsif *pvcsif;
#endif

  if ((sc->enif.if_flags & IFF_UP) == 0) {
#ifdef ATM_PVCEXT
    LIST_FOREACH(pvcsif, &sc->sif_list, sif_links) {
      if (pvcsif->sif_if.if_flags & IFF_UP) {
	/*
	 * down the device only when there is no active pvc subinterface.
	 * if there is, we have to go through the init sequence to reflect
	 * the software states to the device.
	 */
	goto up;
      }
    }
#endif
#ifdef EN_DEBUG
    printf("%s: going down\n", device_xname(sc->sc_dev));
#endif
    en_reset(sc);			/* to be safe */
    sc->enif.if_flags &= ~IFF_RUNNING;	/* disable */
    return;
  }

#ifdef ATM_PVCEXT
 up:
#endif
#ifdef EN_DEBUG
  printf("%s: going up\n", device_xname(sc->sc_dev));
#endif
  sc->enif.if_flags |= IFF_RUNNING;	/* enable */
#ifdef ATM_PVCEXT
  LIST_FOREACH(pvcsif, &sc->sif_list, sif_links) {
    pvcsif->sif_if.if_flags |= IFF_RUNNING;
  }
#endif

  if (sc->en_busreset)
    sc->en_busreset(sc);
  EN_WRITE(sc, MID_RESID, 0x0);		/* reset */

  /*
   * init obmem data structures: vc tab, DMA q's, slist.
   *
   * note that we set drq_free/dtq_free to one less than the total number
   * of DTQ/DRQs present.   we do this because the card uses the condition
   * (drq_chip == drq_us) to mean "list is empty"... but if you allow the
   * circular list to be completely full then (drq_chip == drq_us) [i.e.
   * the drq_us pointer will wrap all the way around].   by restricting
   * the number of active requests to (N - 1) we prevent the list from
   * becoming completely full.    note that the card will sometimes give
   * us an interrupt for a DTQ/DRQ we have already processes... this helps
   * keep that interrupt from messing us up.
   */

  for (vc = 0 ; vc < MID_N_VC ; vc++)
    en_loadvc(sc, vc);

  memset(&sc->drq, 0, sizeof(sc->drq));
  sc->drq_free = MID_DRQ_N - 1;		/* N - 1 */
  sc->drq_chip = MID_DRQ_REG2A(EN_READ(sc, MID_DMA_RDRX));
  EN_WRITE(sc, MID_DMA_WRRX, MID_DRQ_A2REG(sc->drq_chip));
						/* ensure zero queue */
  sc->drq_us = sc->drq_chip;

  memset(&sc->dtq, 0, sizeof(sc->dtq));
  sc->dtq_free = MID_DTQ_N - 1;		/* N - 1 */
  sc->dtq_chip = MID_DTQ_REG2A(EN_READ(sc, MID_DMA_RDTX));
  EN_WRITE(sc, MID_DMA_WRTX, MID_DRQ_A2REG(sc->dtq_chip));
						/* ensure zero queue */
  sc->dtq_us = sc->dtq_chip;

  sc->hwslistp = MID_SL_REG2A(EN_READ(sc, MID_SERV_WRITE));
  sc->swsl_size = sc->swsl_head = sc->swsl_tail = 0;

#ifdef EN_DEBUG
  printf("%s: drq free/chip: %d/0x%x, dtq free/chip: %d/0x%x, hwslist: 0x%x\n",
    device_xname(sc->sc_dev), sc->drq_free, sc->drq_chip,
    sc->dtq_free, sc->dtq_chip, sc->hwslistp);
#endif

  for (slot = 0 ; slot < EN_NTX ; slot++) {
    sc->txslot[slot].bfree = EN_TXSZ * 1024;
    EN_WRITE(sc, MIDX_READPTR(slot), 0);
    EN_WRITE(sc, MIDX_DESCSTART(slot), 0);
    loc = sc->txslot[slot].cur = sc->txslot[slot].start;
    loc = loc - MID_RAMOFF;
    loc = (loc & ~((EN_TXSZ*1024) - 1)) >> 2; /* mask, cvt to words */
    loc = loc >> MIDV_LOCTOPSHFT;	/* top 11 bits */
    EN_WRITE(sc, MIDX_PLACE(slot), MIDX_MKPLACE(en_k2sz(EN_TXSZ), loc));
#ifdef EN_DEBUG
    printf("%s: tx%d: place 0x%x\n", device_xname(sc->sc_dev),  slot,
	EN_READ(sc, MIDX_PLACE(slot)));
#endif
  }

  /*
   * enable!
   */

  EN_WRITE(sc, MID_INTENA, MID_INT_TX|MID_INT_DMA_OVR|MID_INT_IDENT|
	MID_INT_LERR|MID_INT_DMA_ERR|MID_INT_DMA_RX|MID_INT_DMA_TX|
	MID_INT_SERVICE| /* >>> MID_INT_SUNI| XXXCDC<<< */ MID_INT_STATS);
  EN_WRITE(sc, MID_MAST_CSR, MID_SETIPL(sc->ipl)|MID_MCSR_ENDMA|
	MID_MCSR_ENTX|MID_MCSR_ENRX);

}


/*
 * en_loadvc: load a vc tab entry from a slot
 */

STATIC void en_loadvc(struct en_softc *sc, int vc)
{
  int slot;
  u_int32_t reg = EN_READ(sc, MID_VC(vc));

  reg = MIDV_SETMODE(reg, MIDV_TRASH);
  EN_WRITE(sc, MID_VC(vc), reg);
  DELAY(27);

  if ((slot = sc->rxvc2slot[vc]) == RX_NONE)
    return;

  /* no need to set CRC */
  EN_WRITE(sc, MID_DST_RP(vc), 0);	/* read pointer = 0, desc. start = 0 */
  EN_WRITE(sc, MID_WP_ST_CNT(vc), 0);	/* write pointer = 0 */
  EN_WRITE(sc, MID_VC(vc), sc->rxslot[slot].mode);  /* set mode, size, loc */
  sc->rxslot[slot].cur = sc->rxslot[slot].start;

#ifdef EN_DEBUG
    printf("%s: rx%d: assigned to VCI %d\n", device_xname(sc->sc_dev), slot, vc);
#endif
}


/*
 * en_start: start transmitting the next packet that needs to go out
 * if there is one.    note that atm_output() has already splnet()'d us.
 */

STATIC void en_start(struct ifnet *ifp)
{
#ifdef MISSING_IF_SOFTC
    struct en_softc *sc = (struct en_softc *)device_lookup_private(&en_cd, ifp->if_unit);
#else
    struct en_softc *sc = (struct en_softc *) ifp->if_softc;
#endif
    struct mbuf *m, *lastm, *prev;
    struct atm_pseudohdr *ap, *new_ap;
    int txchan, mlen, got, need, toadd, cellcnt, first;
    u_int32_t atm_vpi, atm_vci, atm_flags, *dat, aal;
    u_int8_t *cp;

    if ((ifp->if_flags & IFF_RUNNING) == 0)
	return;

    /*
     * remove everything from interface queue since we handle all queueing
     * locally ...
     */

    while (1) {

      IFQ_DEQUEUE(&ifp->if_snd, m);
      if (m == NULL)
	return;		/* EMPTY: >>> exit here <<< */

      /*
       * calculate size of packet (in bytes)
       * also, if we are not doing transmit DMA we eliminate all stupid
       * (non-word) alignments here using en_mfix().   calls to en_mfix()
       * seem to be due to tcp retransmits for the most part.
       *
       * after this loop mlen total length of mbuf chain (including atm_ph),
       * and lastm is a pointer to the last mbuf on the chain.
       */

      lastm = m;
      mlen = 0;
      prev = NULL;
      while (1) {
	/* no DMA? */
        if ((!sc->is_adaptec && EN_ENIDMAFIX) || EN_NOTXDMA || !en_dma) {
	  if ( (mtod(lastm, unsigned long) % sizeof(u_int32_t)) != 0 ||
	    ((lastm->m_len % sizeof(u_int32_t)) != 0 && lastm->m_next)) {
	    first = (lastm == m);
	    if (en_mfix(sc, &lastm, prev) == 0) {	/* failed? */
	      m_freem(m);
	      m = NULL;
              break;
            }
	    if (first)
	      m = lastm;		/* update */
          }
          prev = lastm;
        }
	mlen += lastm->m_len;
	if (lastm->m_next == NULL)
	  break;
	lastm = lastm->m_next;
      }

      if (m == NULL)		/* happens only if mfix fails */
        continue;

      ap = mtod(m, struct atm_pseudohdr *);

      atm_vpi = ATM_PH_VPI(ap);
      atm_vci = ATM_PH_VCI(ap);
      atm_flags = ATM_PH_FLAGS(ap) & ~(EN_OBHDR|EN_OBTRL);
      aal = ((atm_flags & ATM_PH_AAL5) != 0)
			? MID_TBD_AAL5 : MID_TBD_NOAAL5;

      /*
       * check that vpi/vci is one we can use
       */

      if (atm_vpi || atm_vci >= MID_N_VC) {
	printf("%s: output vpi=%d, vci=%d out of card range, dropping...\n",
		device_xname(sc->sc_dev), atm_vpi, atm_vci);
	m_freem(m);
	continue;
      }

      /*
       * computing how much padding we need on the end of the mbuf, then
       * see if we can put the TBD at the front of the mbuf where the
       * link header goes (well behaved protocols will reserve room for us).
       * last, check if room for PDU tail.
       *
       * got = number of bytes of data we have
       * cellcnt = number of cells in this mbuf
       * need = number of bytes of data + padding we need (excludes TBD)
       * toadd = number of bytes of data we need to add to end of mbuf,
       *	[including AAL5 PDU, if AAL5]
       */

      got = mlen - sizeof(struct atm_pseudohdr);
      toadd = (aal == MID_TBD_AAL5) ? MID_PDU_SIZE : 0;	/* PDU */
      cellcnt = (got + toadd + (MID_ATMDATASZ - 1)) / MID_ATMDATASZ;
      need = cellcnt * MID_ATMDATASZ;
      toadd = need - got;		/* recompute, including zero padding */

#ifdef EN_DEBUG
      printf("%s: txvci%d: mlen=%d, got=%d, need=%d, toadd=%d, cell#=%d\n",
	device_xname(sc->sc_dev), atm_vci, mlen, got, need, toadd, cellcnt);
      printf("     leading_space=%d, trailing_space=%d\n",
	M_LEADINGSPACE(m), M_TRAILINGSPACE(lastm));
#endif

#ifdef EN_MBUF_OPT

      /*
       * note: external storage (M_EXT) can be shared between mbufs
       * to avoid copying (see m_copym()).    this means that the same
       * data buffer could be shared by several mbufs, and thus it isn't
       * a good idea to try and write TBDs or PDUs to M_EXT data areas.
       */

      if (M_LEADINGSPACE(m) >= MID_TBD_SIZE && (m->m_flags & M_EXT) == 0) {
	m->m_data -= MID_TBD_SIZE;
	m->m_len += MID_TBD_SIZE;
	mlen += MID_TBD_SIZE;
	new_ap = mtod(m, struct atm_pseudohdr *);
	*new_ap = *ap;			/* move it back */
	ap = new_ap;
	dat = ((u_int32_t *) ap) + 1;
	/* make sure the TBD is in proper byte order */
	*dat++ = htonl(MID_TBD_MK1(aal, sc->txspeed[atm_vci], cellcnt));
	*dat = htonl(MID_TBD_MK2(atm_vci, 0, 0));
	atm_flags |= EN_OBHDR;
      }

      if (toadd && (lastm->m_flags & M_EXT) == 0 &&
					M_TRAILINGSPACE(lastm) >= toadd) {
	cp = mtod(lastm, u_int8_t *) + lastm->m_len;
	lastm->m_len += toadd;
	mlen += toadd;
	if (aal == MID_TBD_AAL5) {
	  memset(cp, 0, toadd - MID_PDU_SIZE);
	  dat = (u_int32_t *)(cp + toadd - MID_PDU_SIZE);
	  /* make sure the PDU is in proper byte order */
	  *dat = htonl(MID_PDU_MK1(0, 0, got));
	} else {
	  memset(cp, 0, toadd);
	}
	atm_flags |= EN_OBTRL;
      }
      ATM_PH_FLAGS(ap) = atm_flags;	/* update EN_OBHDR/EN_OBTRL bits */
#endif	/* EN_MBUF_OPT */

      /*
       * get assigned channel (will be zero unless txspeed[atm_vci] is set)
       */

      txchan = sc->txvc2slot[atm_vci];

      if (sc->txslot[txchan].mbsize > EN_TXHIWAT) {
	EN_COUNT(sc->txmbovr);
	m_freem(m);
#ifdef EN_DEBUG
	printf("%s: tx%d: buffer space shortage\n", device_xname(sc->sc_dev),
		txchan);
#endif
	continue;
      }

      sc->txslot[txchan].mbsize += mlen;

#ifdef EN_DEBUG
      printf("%s: tx%d: VPI=%d, VCI=%d, FLAGS=0x%x, speed=0x%x\n",
	device_xname(sc->sc_dev), txchan, atm_vpi, atm_vci, atm_flags,
	sc->txspeed[atm_vci]);
      printf("     adjusted mlen=%d, mbsize=%d\n", mlen,
		sc->txslot[txchan].mbsize);
#endif

      IF_ENQUEUE(&sc->txslot[txchan].q, m);
      en_txdma(sc, txchan);

  }
  /*NOTREACHED*/
}


/*
 * en_mfix: fix a stupid mbuf
 */

#ifndef __FreeBSD__

STATIC int en_mfix(struct en_softc *sc, struct mbuf **mm, struct mbuf *prev)
{
  struct mbuf *m, *new;
  u_char *d, *cp;
  int off;
  struct mbuf *nxt;

  m = *mm;

  EN_COUNT(sc->mfix);			/* count # of calls */
#ifdef EN_DEBUG
  printf("%s: mfix mbuf m_data=%p, m_len=%d\n", device_xname(sc->sc_dev),
	m->m_data, m->m_len);
#endif

  d = mtod(m, u_char *);
  off = ((unsigned long) d) % sizeof(u_int32_t);

  if (off) {
    if ((m->m_flags & M_EXT) == 0) {
      memmove(d - off, d, m->m_len);   /* ALIGN! (with costly data copy...) */
      d -= off;
      m->m_data = (void *)d;
    } else {
      /* can't write to an M_EXT mbuf since it may be shared */
      MGET(new, M_DONTWAIT, MT_DATA);
      if (!new) {
        EN_COUNT(sc->mfixfail);
        return(0);
      }
      MCLGET(new, M_DONTWAIT);
      if ((new->m_flags & M_EXT) == 0) {
        m_free(new);
        EN_COUNT(sc->mfixfail);
        return(0);
      }
      memcpy(new->m_data, d, m->m_len);	/* ALIGN! (with costly data copy...) */
      new->m_len = m->m_len;
      new->m_next = m->m_next;
      if (prev)
        prev->m_next = new;
      m_free(m);
      *mm = m = new;	/* note: 'd' now invalid */
    }
  }

  off = m->m_len % sizeof(u_int32_t);
  if (off == 0)
    return(1);

  d = mtod(m, u_char *) + m->m_len;
  off = sizeof(u_int32_t) - off;

  nxt = m->m_next;
  while (off--) {
    for ( ; nxt != NULL && nxt->m_len == 0 ; nxt = nxt->m_next)
      /*null*/;
    if (nxt == NULL) {		/* out of data, zero fill */
      *d++ = 0;
      continue;			/* next "off" */
    }
    cp = mtod(nxt, u_char *);
    *d++ = *cp++;
    m->m_len++;
    nxt->m_len--;
    nxt->m_data = (void *)cp;
  }
  return(1);
}

#else /* __FreeBSD__ */

STATIC int en_makeexclusive(struct en_softc *, struct mbuf **, struct mbuf *);

STATIC int en_makeexclusive(sc, mm, prev)
    struct en_softc *sc;
    struct mbuf **mm, *prev;
{
    struct mbuf *m, *new;

    m = *mm;

    if (m->m_flags & M_EXT) {
	if (m->m_ext.ext_free) {
	    /* external buffer isn't an ordinary mbuf cluster! */
	    aprint_error_dev(sc->sc_dev, "mfix: special buffer! can't make a copy!\n");
	    return (0);
	}

	if (mclrefcnt[mtocl(m->m_ext.ext_buf)] > 1) {
	    /* make a real copy of the M_EXT mbuf since it is shared */
	    MGET(new, M_DONTWAIT, MT_DATA);
	    if (!new) {
		EN_COUNT(sc->mfixfail);
		return(0);
	    }
	    if (m->m_flags & M_PKTHDR)
		M_COPY_PKTHDR(new, m);
	    MCLGET(new, M_DONTWAIT);
	    if ((new->m_flags & M_EXT) == 0) {
		m_free(new);
		EN_COUNT(sc->mfixfail);
		return(0);
	    }
	    memcpy(new->m_data, m->m_data, m->m_len);
	    new->m_len = m->m_len;
	    new->m_next = m->m_next;
	    if (prev)
		prev->m_next = new;
	    m_free(m);
	    *mm = new;
	}
	else {
	    /* the buffer is not shared, align the data offset using
	       this buffer. */
	    u_char *d = mtod(m, u_char *);
	    int off = ((u_long)d) % sizeof(u_int32_t);

	    if (off > 0) {
		memmove(d - off, d, m->m_len);
		m->m_data = (void *)d - off;
	    }
	}
    }
    return (1);
}

STATIC int en_mfix(sc, mm, prev)

struct en_softc *sc;
struct mbuf **mm, *prev;

{
  struct mbuf *m;
  u_char *d, *cp;
  int off;
  struct mbuf *nxt;

  m = *mm;

  EN_COUNT(sc->mfix);			/* count # of calls */
#ifdef EN_DEBUG
  printf("%s: mfix mbuf m_data=0x%x, m_len=%d\n", device_xname(sc->sc_dev),
	m->m_data, m->m_len);
#endif

  d = mtod(m, u_char *);
  off = ((unsigned long) d) % sizeof(u_int32_t);

  if (off) {
    if ((m->m_flags & M_EXT) == 0) {
      memmove(d - off, d, m->m_len);   /* ALIGN! (with costly data copy...) */
      d -= off;
      m->m_data = (void *)d;
    } else {
      /* can't write to an M_EXT mbuf since it may be shared */
      if (en_makeexclusive(sc, &m, prev) == 0)
	  return (0);
      *mm = m;	/* note: 'd' now invalid */
    }
  }

  off = m->m_len % sizeof(u_int32_t);
  if (off == 0)
    return(1);

  if (m->m_flags & M_EXT) {
      /* can't write to an M_EXT mbuf since it may be shared */
      if (en_makeexclusive(sc, &m, prev) == 0)
	  return (0);
      *mm = m;	/* note: 'd' now invalid */
  }

  d = mtod(m, u_char *) + m->m_len;
  off = sizeof(u_int32_t) - off;

  nxt = m->m_next;
  while (off--) {
    if (nxt != NULL && nxt->m_len == 0) {
	/* remove an empty mbuf.  this avoids odd byte padding to an empty
	   last mbuf.  */
	m->m_next = nxt = m_free(nxt);
    }
    if (nxt == NULL) {		/* out of data, zero fill */
      *d++ = 0;
      continue;			/* next "off" */
    }
    cp = mtod(nxt, u_char *);
    *d++ = *cp++;
    m->m_len++;
    nxt->m_len--;
    nxt->m_data = (void *)cp;
  }
  if (nxt != NULL && nxt->m_len == 0)
      m->m_next = m_free(nxt);
  return(1);
}

#endif /* __FreeBSD__ */

/*
 * en_txdma: start transmit DMA, if possible
 */

STATIC void en_txdma(struct en_softc *sc, int chan)
{
  struct mbuf *tmp;
  struct atm_pseudohdr *ap;
  struct en_launch launch;
  int datalen = 0, dtqneed, len, ncells;
  u_int8_t *cp;
  struct ifnet *ifp;

  memset(&launch, 0, sizeof launch);	/* XXX gcc */

#ifdef EN_DEBUG
  printf("%s: tx%d: starting...\n", device_xname(sc->sc_dev), chan);
#endif

  /*
   * note: now that txlaunch handles non-word aligned/sized requests
   * the only time you can safely set launch.nodma is if you've en_mfix()'d
   * the mbuf chain.    this happens only if EN_NOTXDMA || !en_dma.
   */

  launch.nodma = (EN_NOTXDMA || !en_dma);

again:

  /*
   * get an mbuf waiting for DMA
   */

  launch.t = sc->txslot[chan].q.ifq_head; /* peek at head of queue */

  if (launch.t == NULL) {
#ifdef EN_DEBUG
    printf("%s: tx%d: ...done!\n", device_xname(sc->sc_dev), chan);
#endif
    return;	/* >>> exit here if no data waiting for DMA <<< */
  }

  /*
   * get flags, vci
   *
   * note: launch.need = # bytes we need to get on the card
   *	   dtqneed = # of DTQs we need for this packet
   *       launch.mlen = # of bytes in in mbuf chain (<= launch.need)
   */

  ap = mtod(launch.t, struct atm_pseudohdr *);
  launch.atm_vci = ATM_PH_VCI(ap);
  launch.atm_flags = ATM_PH_FLAGS(ap);
  launch.aal = ((launch.atm_flags & ATM_PH_AAL5) != 0) ?
		MID_TBD_AAL5 : MID_TBD_NOAAL5;

  /*
   * XXX: have to recompute the length again, even though we already did
   * it in en_start().   might as well compute dtqneed here as well, so
   * this isn't that bad.
   */

  if ((launch.atm_flags & EN_OBHDR) == 0) {
    dtqneed = 1;		/* header still needs to be added */
    launch.need = MID_TBD_SIZE;	/* not included with mbuf */
  } else {
    dtqneed = 0;		/* header on-board, DMA with mbuf */
    launch.need = 0;
  }

  launch.mlen = 0;
  for (tmp = launch.t ; tmp != NULL ; tmp = tmp->m_next) {
    len = tmp->m_len;
    launch.mlen += len;
    cp = mtod(tmp, u_int8_t *);
    if (tmp == launch.t) {
      len -= sizeof(struct atm_pseudohdr); /* don't count this! */
      cp += sizeof(struct atm_pseudohdr);
    }
    launch.need += len;
    if (len == 0)
      continue;			/* atm_pseudohdr alone in first mbuf */

    dtqneed += en_dqneed(sc, (void *) cp, len, 1);
  }

  if ((launch.need % sizeof(u_int32_t)) != 0)
    dtqneed++;			/* need DTQ to FLUSH internal buffer */

  if ((launch.atm_flags & EN_OBTRL) == 0) {
    if (launch.aal == MID_TBD_AAL5) {
      datalen = launch.need - MID_TBD_SIZE;
      launch.need += MID_PDU_SIZE;		/* AAL5: need PDU tail */
    }
    dtqneed++;			/* need to work on the end a bit */
  }

  /*
   * finish calculation of launch.need (need to figure out how much padding
   * we will need).   launch.need includes MID_TBD_SIZE, but we need to
   * remove that to so we can round off properly.     we have to add
   * MID_TBD_SIZE back in after calculating ncells.
   */

  launch.need = roundup(launch.need - MID_TBD_SIZE, MID_ATMDATASZ);
  ncells = launch.need / MID_ATMDATASZ;
  launch.need += MID_TBD_SIZE;

  if (launch.need > EN_TXSZ * 1024) {
    printf("%s: tx%d: packet larger than xmit buffer (%d > %d)\n",
      device_xname(sc->sc_dev), chan, launch.need, EN_TXSZ * 1024);
    goto dequeue_drop;
  }

  /*
   * note: note that we cannot totally fill the circular buffer (i.e.
   * we can't use up all of the remaining sc->txslot[chan].bfree free
   * bytes) because that would cause the circular buffer read pointer
   * to become equal to the write pointer, thus signaling 'empty buffer'
   * to the hardware and stopping the transmitter.
   */
  if (launch.need >= sc->txslot[chan].bfree) {
    EN_COUNT(sc->txoutspace);
#ifdef EN_DEBUG
    printf("%s: tx%d: out of transmit space\n", device_xname(sc->sc_dev), chan);
#endif
    return;		/* >>> exit here if out of obmem buffer space <<< */
  }

  /*
   * ensure we have enough dtqs to go, if not, wait for more.
   */

  if (launch.nodma) {
    dtqneed = 1;
  }
  if (dtqneed > sc->dtq_free) {
    sc->need_dtqs = 1;
    EN_COUNT(sc->txdtqout);
#ifdef EN_DEBUG
    printf("%s: tx%d: out of transmit DTQs\n", device_xname(sc->sc_dev), chan);
#endif
    return;		/* >>> exit here if out of dtqs <<< */
  }

  /*
   * it is a go, commit!  dequeue mbuf start working on the xfer.
   */

  IF_DEQUEUE(&sc->txslot[chan].q, tmp);
#ifdef EN_DIAG
  if (launch.t != tmp)
    panic("en dequeue");
#endif /* EN_DIAG */

  /*
   * launch!
   */

  EN_COUNT(sc->launch);
#ifdef ATM_PVCEXT
  /* if there's a subinterface for this vci, override ifp. */
  ifp = en_vci2ifp(sc, launch.atm_vci);
#else
  ifp = &sc->enif;
#endif
  ifp->if_opackets++;

  if ((launch.atm_flags & EN_OBHDR) == 0) {
    EN_COUNT(sc->lheader);
    /* store tbd1/tbd2 in host byte order */
    launch.tbd1 = MID_TBD_MK1(launch.aal, sc->txspeed[launch.atm_vci], ncells);
    launch.tbd2 = MID_TBD_MK2(launch.atm_vci, 0, 0);
  }
  if ((launch.atm_flags & EN_OBTRL) == 0 && launch.aal == MID_TBD_AAL5) {
    EN_COUNT(sc->ltail);
    launch.pdu1 = MID_PDU_MK1(0, 0, datalen);  /* host byte order */
  }

  en_txlaunch(sc, chan, &launch);

  if (ifp->if_bpf) {
      /*
       * adjust the top of the mbuf to skip the pseudo atm header
       * (and TBD, if present) before passing the packet to bpf,
       * restore it afterwards.
       */
      int size = sizeof(struct atm_pseudohdr);
      if (launch.atm_flags & EN_OBHDR)
	  size += MID_TBD_SIZE;

      launch.t->m_data += size;
      launch.t->m_len -= size;

      bpf_mtap(ifp, launch.t);

      launch.t->m_data -= size;
      launch.t->m_len += size;
  }
  /*
   * do some housekeeping and get the next packet
   */

  sc->txslot[chan].bfree -= launch.need;
  IF_ENQUEUE(&sc->txslot[chan].indma, launch.t);
  goto again;

  /*
   * END of txdma loop!
   */

  /*
   * error handles
   */

dequeue_drop:
  IF_DEQUEUE(&sc->txslot[chan].q, tmp);
  if (launch.t != tmp)
    panic("en dequeue drop");
  m_freem(launch.t);
  sc->txslot[chan].mbsize -= launch.mlen;
  goto again;
}


/*
 * en_txlaunch: launch an mbuf into the DMA pool!
 */

STATIC void en_txlaunch(struct en_softc *sc, int chan, struct en_launch *l)
{
  struct mbuf *tmp;
  u_int32_t cur = sc->txslot[chan].cur,
	    start = sc->txslot[chan].start,
	    stop = sc->txslot[chan].stop,
	    dma, *data, *datastop, count, bcode;
  int pad, addtail, need, len, needalign, cnt, end, mx;


 /*
  * vars:
  *   need = # bytes card still needs (decr. to zero)
  *   len = # of bytes left in current mbuf
  *   cur = our current pointer
  *   dma = last place we programmed into the DMA
  *   data = pointer into data area of mbuf that needs to go next
  *   cnt = # of bytes to transfer in this DTQ
  *   bcode/count = DMA burst code, and chip's version of cnt
  *
  *   a single buffer can require up to 5 DTQs depending on its size
  *   and alignment requirements.   the 5 possible requests are:
  *   [1] 1, 2, or 3 byte DMA to align src data pointer to word boundary
  *   [2] alburst DMA to align src data pointer to bestburstlen
  *   [3] 1 or more bestburstlen DMAs
  *   [4] clean up burst (to last word boundary)
  *   [5] 1, 2, or 3 byte final clean up DMA
  */

 need = l->need;
 dma = cur;
 addtail = (l->atm_flags & EN_OBTRL) == 0;	/* add a tail? */

#ifdef EN_DIAG
  if ((need - MID_TBD_SIZE) % MID_ATMDATASZ)
    printf("%s: tx%d: bogus transmit needs (%d)\n", device_xname(sc->sc_dev), chan,
		need);
#endif
#ifdef EN_DEBUG
  printf("%s: tx%d: launch mbuf %p!   cur=0x%x[%d], need=%d, addtail=%d\n",
	device_xname(sc->sc_dev), chan, l->t, cur, (cur-start)/4, need, addtail);
  count = EN_READ(sc, MIDX_PLACE(chan));
  printf("     HW: base_address=0x%x, size=%d, read=%d, descstart=%d\n",
	MIDX_BASE(count), MIDX_SZ(count), EN_READ(sc, MIDX_READPTR(chan)),
	EN_READ(sc, MIDX_DESCSTART(chan)));
#endif

 /*
  * do we need to insert the TBD by hand?
  * note that tbd1/tbd2/pdu1 are in host byte order.
  */

  if ((l->atm_flags & EN_OBHDR) == 0) {
#ifdef EN_DEBUG
    printf("%s: tx%d: insert header 0x%x 0x%x\n", device_xname(sc->sc_dev),
	chan, l->tbd1, l->tbd2);
#endif
    EN_WRITE(sc, cur, l->tbd1);
    EN_WRAPADD(start, stop, cur, 4);
    EN_WRITE(sc, cur, l->tbd2);
    EN_WRAPADD(start, stop, cur, 4);
    need -= 8;
  }

  /*
   * now do the mbufs...
   */

  for (tmp = l->t ; tmp != NULL ; tmp = tmp->m_next) {

    /* get pointer to data and length */
    data = mtod(tmp, u_int32_t *);
    len = tmp->m_len;
    if (tmp == l->t) {
      data += sizeof(struct atm_pseudohdr)/sizeof(u_int32_t);
      len -= sizeof(struct atm_pseudohdr);
    }

    /* now, determine if we should copy it */
    if (l->nodma || (len < EN_MINDMA &&
       (len % 4) == 0 && ((unsigned long) data % 4) == 0 && (cur % 4) == 0)) {

      /*
       * roundup len: the only time this will change the value of len
       * is when l->nodma is true, tmp is the last mbuf, and there is
       * a non-word number of bytes to transmit.   in this case it is
       * safe to round up because we've en_mfix'd the mbuf (so the first
       * byte is word aligned there must be enough free bytes at the end
       * to round off to the next word boundary)...
       */
      len = roundup(len, sizeof(u_int32_t));
      datastop = data + (len / sizeof(u_int32_t));
      /* copy loop: preserve byte order!!!  use WRITEDAT */
      while (data != datastop) {
	EN_WRITEDAT(sc, cur, *data);
	data++;
	EN_WRAPADD(start, stop, cur, 4);
      }
      need -= len;
#ifdef EN_DEBUG
      printf("%s: tx%d: copied %d bytes (%d left, cur now 0x%x)\n",
		device_xname(sc->sc_dev), chan, len, need, cur);
#endif
      continue;		/* continue on to next mbuf */
    }

    /* going to do DMA, first make sure the dtq is in sync. */
    if (dma != cur) {
      EN_DTQADD(sc, WORD_IDX(start,cur), chan, MIDDMA_JK, 0, 0, 0);
#ifdef EN_DEBUG
      printf("%s: tx%d: dtq_sync: advance pointer to %d\n",
		device_xname(sc->sc_dev), chan, cur);
#endif
    }

    /*
     * if this is the last buffer, and it looks like we are going to need to
     * flush the internal buffer, can we extend the length of this mbuf to
     * avoid the FLUSH?
     */

    if (tmp->m_next == NULL) {
      cnt = (need - len) % sizeof(u_int32_t);
      if (cnt && M_TRAILINGSPACE(tmp) >= cnt)
        len += cnt;			/* pad for FLUSH */
    }

#if !defined(MIDWAY_ENIONLY)

    /*
     * the adaptec DMA engine is smart and handles everything for us.
     */

    if (sc->is_adaptec) {
      /* need to DMA "len" bytes out to card */
      need -= len;
      EN_WRAPADD(start, stop, cur, len);
#ifdef EN_DEBUG
      printf("%s: tx%d: adp_dma %d bytes (%d left, cur now 0x%x)\n",
              device_xname(sc->sc_dev), chan, len, need, cur);
#endif
      end = (need == 0) ? MID_DMA_END : 0;
      EN_DTQADD(sc, len, chan, 0, vtophys((vaddr_t)data), l->mlen, end);
      if (end)
        goto done;
      dma = cur;	/* update DMA pointer */
      continue;
    }
#endif /* !MIDWAY_ENIONLY */

#if !defined(MIDWAY_ADPONLY)

    /*
     * the ENI DMA engine is not so smart and need more help from us
     */

    /* do we need to do a DMA op to align to word boundary? */
    needalign = (unsigned long) data % sizeof(u_int32_t);
    if (needalign) {
      EN_COUNT(sc->headbyte);
      cnt = sizeof(u_int32_t) - needalign;
      if (cnt == 2 && len >= cnt) {
        count = 1;
        bcode = MIDDMA_2BYTE;
      } else {
        cnt = min(cnt, len);		/* prevent overflow */
        count = cnt;
        bcode = MIDDMA_BYTE;
      }
      need -= cnt;
      EN_WRAPADD(start, stop, cur, cnt);
#ifdef EN_DEBUG
      printf("%s: tx%d: small al_dma %d bytes (%d left, cur now 0x%x)\n",
              device_xname(sc->sc_dev), chan, cnt, need, cur);
#endif
      len -= cnt;
      end = (need == 0) ? MID_DMA_END : 0;
      EN_DTQADD(sc, count, chan, bcode, vtophys((vaddr_t)data), l->mlen, end);
      if (end)
        goto done;
      data = (u_int32_t *) ((u_char *)data + cnt);
    }

    /* do we need to do a DMA op to align? */
    if (sc->alburst &&
	(needalign = (((unsigned long) data) & sc->bestburstmask)) != 0
	&& len >= sizeof(u_int32_t)) {
      cnt = sc->bestburstlen - needalign;
      mx = len & ~(sizeof(u_int32_t)-1);	/* don't go past end */
      if (cnt > mx) {
        cnt = mx;
        count = cnt / sizeof(u_int32_t);
        bcode = MIDDMA_WORD;
      } else {
        count = cnt / sizeof(u_int32_t);
        bcode = en_dmaplan[count].bcode;
        count = cnt >> en_dmaplan[count].divshift;
      }
      need -= cnt;
      EN_WRAPADD(start, stop, cur, cnt);
#ifdef EN_DEBUG
      printf("%s: tx%d: al_dma %d bytes (%d left, cur now 0x%x)\n",
		device_xname(sc->sc_dev), chan, cnt, need, cur);
#endif
      len -= cnt;
      end = (need == 0) ? MID_DMA_END : 0;
      EN_DTQADD(sc, count, chan, bcode, vtophys((vaddr_t)data), l->mlen, end);
      if (end)
        goto done;
      data = (u_int32_t *) ((u_char *)data + cnt);
    }

    /* do we need to do a max-sized burst? */
    if (len >= sc->bestburstlen) {
      count = len >> sc->bestburstshift;
      cnt = count << sc->bestburstshift;
      bcode = sc->bestburstcode;
      need -= cnt;
      EN_WRAPADD(start, stop, cur, cnt);
#ifdef EN_DEBUG
      printf("%s: tx%d: best_dma %d bytes (%d left, cur now 0x%x)\n",
		device_xname(sc->sc_dev), chan, cnt, need, cur);
#endif
      len -= cnt;
      end = (need == 0) ? MID_DMA_END : 0;
      EN_DTQADD(sc, count, chan, bcode, vtophys((vaddr_t)data), l->mlen, end);
      if (end)
        goto done;
      data = (u_int32_t *) ((u_char *)data + cnt);
    }

    /* do we need to do a cleanup burst? */
    cnt = len & ~(sizeof(u_int32_t)-1);
    if (cnt) {
      count = cnt / sizeof(u_int32_t);
      bcode = en_dmaplan[count].bcode;
      count = cnt >> en_dmaplan[count].divshift;
      need -= cnt;
      EN_WRAPADD(start, stop, cur, cnt);
#ifdef EN_DEBUG
      printf("%s: tx%d: cleanup_dma %d bytes (%d left, cur now 0x%x)\n",
		device_xname(sc->sc_dev), chan, cnt, need, cur);
#endif
      len -= cnt;
      end = (need == 0) ? MID_DMA_END : 0;
      EN_DTQADD(sc, count, chan, bcode, vtophys((vaddr_t)data), l->mlen, end);
      if (end)
        goto done;
      data = (u_int32_t *) ((u_char *)data + cnt);
    }

    /* any word fragments left? */
    if (len) {
      EN_COUNT(sc->tailbyte);
      if (len == 2) {
        count = 1;
        bcode = MIDDMA_2BYTE;                 /* use 2byte mode */
      } else {
        count = len;
        bcode = MIDDMA_BYTE;                  /* use 1 byte mode */
      }
      need -= len;
      EN_WRAPADD(start, stop, cur, len);
#ifdef EN_DEBUG
      printf("%s: tx%d: byte cleanup_dma %d bytes (%d left, cur now 0x%x)\n",
		device_xname(sc->sc_dev), chan, len, need, cur);
#endif
      end = (need == 0) ? MID_DMA_END : 0;
      EN_DTQADD(sc, count, chan, bcode, vtophys((vaddr_t)data), l->mlen, end);
      if (end)
        goto done;
    }

    dma = cur;		/* update DMA pointer */
#endif /* !MIDWAY_ADPONLY */

  } /* next mbuf, please */

  /*
   * all mbuf data has been copied out to the obmem (or set up to be DMAd).
   * if the trailer or padding needs to be put in, do it now.
   *
   * NOTE: experimental results reveal the following fact:
   *   if you DMA "X" bytes to the card, where X is not a multiple of 4,
   *   then the card will internally buffer the last (X % 4) bytes (in
   *   hopes of getting (4 - (X % 4)) more bytes to make a complete word).
   *   it is imporant to make sure we don't leave any important data in
   *   this internal buffer because it is discarded on the last (end) DTQ.
   *   one way to do this is to DMA in (4 - (X % 4)) more bytes to flush
   *   the darn thing out.
   */

  if (addtail) {

    pad = need % sizeof(u_int32_t);
    if (pad) {
      /*
       * FLUSH internal data buffer.  pad out with random data from the front
       * of the mbuf chain...
       */
      bcode = (sc->is_adaptec) ? 0 : MIDDMA_BYTE;
      EN_COUNT(sc->tailflush);
      EN_WRAPADD(start, stop, cur, pad);
      EN_DTQADD(sc, pad, chan, bcode, vtophys((vaddr_t)l->t->m_data), 0, 0);
      need -= pad;
#ifdef EN_DEBUG
      printf("%s: tx%d: pad/FLUSH DMA %d bytes (%d left, cur now 0x%x)\n",
		device_xname(sc->sc_dev), chan, pad, need, cur);
#endif
    }

    /* copy data */
    pad = need / sizeof(u_int32_t);	/* round *down* */
    if (l->aal == MID_TBD_AAL5)
      pad -= 2;
#ifdef EN_DEBUG
      printf("%s: tx%d: padding %d bytes (cur now 0x%x)\n",
		device_xname(sc->sc_dev), chan, pad * sizeof(u_int32_t), cur);
#endif
    while (pad--) {
      EN_WRITEDAT(sc, cur, 0);	/* no byte order issues with zero */
      EN_WRAPADD(start, stop, cur, 4);
    }
    if (l->aal == MID_TBD_AAL5) {
      EN_WRITE(sc, cur, l->pdu1); /* in host byte order */
      EN_WRAPADD(start, stop, cur, 8);
    }
  }

  if (addtail || dma != cur) {
   /* write final descriptor  */
    EN_DTQADD(sc, WORD_IDX(start,cur), chan, MIDDMA_JK, 0,
				l->mlen, MID_DMA_END);
    /* dma = cur; */ 	/* not necessary since we are done */
  }

done:
  /* update current pointer */
  sc->txslot[chan].cur = cur;
#ifdef EN_DEBUG
      printf("%s: tx%d: DONE!   cur now = 0x%x\n",
		device_xname(sc->sc_dev), chan, cur);
#endif

  return;
}


/*
 * interrupt handler
 */

EN_INTR_TYPE en_intr(void *arg)
{
  struct en_softc *sc = (struct en_softc *) arg;
  struct mbuf *m;
  struct atm_pseudohdr ah;
  struct ifnet *ifp;
  u_int32_t reg, kick, val, mask, chip, vci, slot, dtq, drq;
  int lcv, idx, need_softserv = 0;

  reg = EN_READ(sc, MID_INTACK);

  if ((reg & MID_INT_ANY) == 0)
    EN_INTR_RET(0); /* not us */

#ifdef EN_DEBUG
  {
    char sbuf[256];

    snprintb(sbuf, sizeof(sbuf), MID_INTBITS, reg);
    printf("%s: interrupt=0x%s\n", device_xname(sc->sc_dev), sbuf);
  }
#endif

  /*
   * unexpected errors that need a reset
   */

  if ((reg & (MID_INT_IDENT|MID_INT_LERR|MID_INT_DMA_ERR|MID_INT_SUNI)) != 0) {
    char sbuf[256];

    snprintb(sbuf, sizeof(sbuf), MID_INTBITS, reg);
    printf("%s: unexpected interrupt=0x%s, resetting card\n",
           device_xname(sc->sc_dev), sbuf);
#ifdef EN_DEBUG
#ifdef DDB
#ifdef __FreeBSD__
    Debugger("en: unexpected error");
#else
    Debugger();
#endif
#endif	/* DDB */
    sc->enif.if_flags &= ~IFF_RUNNING; /* FREEZE! */
#else
    en_reset(sc);
    en_init(sc);
#endif
    EN_INTR_RET(1); /* for us */
  }

  /*******************
   * xmit interrupts *
   ******************/

  kick = 0;				/* bitmask of channels to kick */
  if (reg & MID_INT_TX) {		/* TX done! */

    /*
     * check for tx complete, if detected then this means that some space
     * has come free on the card.   we must account for it and arrange to
     * kick the channel to life (in case it is stalled waiting on the card).
     */
    for (mask = 1, lcv = 0 ; lcv < EN_NTX ; lcv++, mask = mask * 2) {
      if (reg & MID_TXCHAN(lcv)) {
	kick = kick | mask;	/* want to kick later */
	val = EN_READ(sc, MIDX_READPTR(lcv));	/* current read pointer */
	val = (val * sizeof(u_int32_t)) + sc->txslot[lcv].start;
						/* convert to offset */
	if (val > sc->txslot[lcv].cur)
	  sc->txslot[lcv].bfree = val - sc->txslot[lcv].cur;
	else
	  sc->txslot[lcv].bfree = (val + (EN_TXSZ*1024)) - sc->txslot[lcv].cur;
#ifdef EN_DEBUG
	printf("%s: tx%d: transmit done.   %d bytes now free in buffer\n",
		device_xname(sc->sc_dev), lcv, sc->txslot[lcv].bfree);
#endif
      }
    }
  }

  if (reg & MID_INT_DMA_TX) {		/* TX DMA done! */

  /*
   * check for TX DMA complete, if detected then this means that some DTQs
   * are now free.   it also means some indma mbufs can be freed.
   * if we needed DTQs, kick all channels.
   */
    val = EN_READ(sc, MID_DMA_RDTX);	/* chip's current location */
    idx = MID_DTQ_A2REG(sc->dtq_chip);/* where we last saw chip */
    if (sc->need_dtqs) {
      kick = MID_NTX_CH - 1;		/* assume power of 2, kick all! */
      sc->need_dtqs = 0;		/* recalculated in "kick" loop below */
#ifdef EN_DEBUG
      printf("%s: cleared need DTQ condition\n", device_xname(sc->sc_dev));
#endif
    }
    while (idx != val) {
      sc->dtq_free++;
      if ((dtq = sc->dtq[idx]) != 0) {
        sc->dtq[idx] = 0;	/* don't forget to zero it out when done */
	slot = EN_DQ_SLOT(dtq);
	IF_DEQUEUE(&sc->txslot[slot].indma, m);
	if (!m) panic("enintr: dtqsync");
	sc->txslot[slot].mbsize -= EN_DQ_LEN(dtq);
#ifdef EN_DEBUG
	printf("%s: tx%d: free %d DMA bytes, mbsize now %d\n",
		device_xname(sc->sc_dev), slot, EN_DQ_LEN(dtq),
		sc->txslot[slot].mbsize);
#endif
	m_freem(m);
      }
      EN_WRAPADD(0, MID_DTQ_N, idx, 1);
    };
    sc->dtq_chip = MID_DTQ_REG2A(val);	/* sync softc */
  }


  /*
   * kick xmit channels as needed
   */

  if (kick) {
#ifdef EN_DEBUG
  printf("%s: tx kick mask = 0x%x\n", device_xname(sc->sc_dev), kick);
#endif
    for (mask = 1, lcv = 0 ; lcv < EN_NTX ; lcv++, mask = mask * 2) {
      if ((kick & mask) && sc->txslot[lcv].q.ifq_head) {
	en_txdma(sc, lcv);		/* kick it! */
      }
    }		/* for each slot */
  }		/* if kick */


  /*******************
   * recv interrupts *
   ******************/

  /*
   * check for RX DMA complete, and pass the data "upstairs"
   */

  if (reg & MID_INT_DMA_RX) {
    val = EN_READ(sc, MID_DMA_RDRX); /* chip's current location */
    idx = MID_DRQ_A2REG(sc->drq_chip);/* where we last saw chip */
    while (idx != val) {
      sc->drq_free++;
      if ((drq = sc->drq[idx]) != 0) {
        sc->drq[idx] = 0;	/* don't forget to zero it out when done */
	slot = EN_DQ_SLOT(drq);
        if (EN_DQ_LEN(drq) == 0) {  /* "JK" trash DMA? */
          m = NULL;
        } else {
	  IF_DEQUEUE(&sc->rxslot[slot].indma, m);
	  if (!m)
	    panic("enintr: drqsync: %s: lost mbuf in slot %d!",
		  device_xname(sc->sc_dev), slot);
        }
	/* do something with this mbuf */
	if (sc->rxslot[slot].oth_flags & ENOTHER_DRAIN) {  /* drain? */
          if (m)
	    m_freem(m);
	  vci = sc->rxslot[slot].atm_vci;
	  if (sc->rxslot[slot].indma.ifq_head == NULL &&
		sc->rxslot[slot].q.ifq_head == NULL &&
		(EN_READ(sc, MID_VC(vci)) & MIDV_INSERVICE) == 0 &&
		(sc->rxslot[slot].oth_flags & ENOTHER_SWSL) == 0) {
	    sc->rxslot[slot].oth_flags = ENOTHER_FREE; /* done drain */
	    sc->rxslot[slot].atm_vci = RX_NONE;
	    sc->rxvc2slot[vci] = RX_NONE;
#ifdef EN_DEBUG
	    printf("%s: rx%d: VCI %d now free\n", device_xname(sc->sc_dev),
			slot, vci);
#endif
	  }
	} else if (m != NULL) {
	  ATM_PH_FLAGS(&ah) = sc->rxslot[slot].atm_flags;
	  ATM_PH_VPI(&ah) = 0;
	  ATM_PH_SETVCI(&ah, sc->rxslot[slot].atm_vci);
#ifdef EN_DEBUG
	  printf("%s: rx%d: rxvci%d: atm_input, mbuf %p, len %d, hand %p\n",
		device_xname(sc->sc_dev), slot, sc->rxslot[slot].atm_vci, m,
		EN_DQ_LEN(drq), sc->rxslot[slot].rxhand);
#endif

#ifdef ATM_PVCEXT
	  /* if there's a subinterface for this vci, override ifp. */
	  ifp = en_vci2ifp(sc, sc->rxslot[slot].atm_vci);
	  ifp->if_ipackets++;
	  m->m_pkthdr.rcvif = ifp;	/* XXX */
#else
	  ifp = &sc->enif;
	  ifp->if_ipackets++;
#endif

	  bpf_mtap(ifp, m);

	  atm_input(ifp, &ah, m, sc->rxslot[slot].rxhand);
	}

      }
      EN_WRAPADD(0, MID_DRQ_N, idx, 1);
    };
    sc->drq_chip = MID_DRQ_REG2A(val);	/* sync softc */

    if (sc->need_drqs) {	/* true if we had a DRQ shortage */
      need_softserv = 1;
      sc->need_drqs = 0;
#ifdef EN_DEBUG
	printf("%s: cleared need DRQ condition\n", device_xname(sc->sc_dev));
#endif
    }
  }

  /*
   * handle service interrupts
   */

  if (reg & MID_INT_SERVICE) {
    chip = MID_SL_REG2A(EN_READ(sc, MID_SERV_WRITE));

    while (sc->hwslistp != chip) {

      /* fetch and remove it from hardware service list */
      vci = EN_READ(sc, sc->hwslistp);
      EN_WRAPADD(MID_SLOFF, MID_SLEND, sc->hwslistp, 4);/* advance hw ptr */
      slot = sc->rxvc2slot[vci];
      if (slot == RX_NONE) {
#ifdef EN_DEBUG
	printf("%s: unexpected rx interrupt on VCI %d\n",
		device_xname(sc->sc_dev), vci);
#endif
	EN_WRITE(sc, MID_VC(vci), MIDV_TRASH);  /* rx off, damn it! */
	continue;				/* next */
      }
      EN_WRITE(sc, MID_VC(vci), sc->rxslot[slot].mode); /* remove from hwsl */
      EN_COUNT(sc->hwpull);

#ifdef EN_DEBUG
      printf("%s: pulled VCI %d off hwslist\n", device_xname(sc->sc_dev), vci);
#endif

      /* add it to the software service list (if needed) */
      if ((sc->rxslot[slot].oth_flags & ENOTHER_SWSL) == 0) {
	EN_COUNT(sc->swadd);
	need_softserv = 1;
	sc->rxslot[slot].oth_flags |= ENOTHER_SWSL;
	sc->swslist[sc->swsl_tail] = slot;
	EN_WRAPADD(0, MID_SL_N, sc->swsl_tail, 1);
	sc->swsl_size++;
#ifdef EN_DEBUG
      printf("%s: added VCI %d to swslist\n", device_xname(sc->sc_dev), vci);
#endif
      }
    };
  }

  /*
   * now service (function too big to include here)
   */

  if (need_softserv)
    en_service(sc);

  /*
   * keep our stats
   */

  if (reg & MID_INT_DMA_OVR) {
    EN_COUNT(sc->dmaovr);
#ifdef EN_DEBUG
    printf("%s: MID_INT_DMA_OVR\n", device_xname(sc->sc_dev));
#endif
  }
  reg = EN_READ(sc, MID_STAT);
#ifdef EN_STAT
  sc->otrash += MID_OTRASH(reg);
  sc->vtrash += MID_VTRASH(reg);
#endif

  EN_INTR_RET(1); /* for us */
}


/*
 * en_service: handle a service interrupt
 *
 * Q: why do we need a software service list?
 *
 * A: if we remove a VCI from the hardware list and we find that we are
 *    out of DRQs we must defer processing until some DRQs become free.
 *    so we must remember to look at this RX VCI/slot later, but we can't
 *    put it back on the hardware service list (since that isn't allowed).
 *    so we instead save it on the software service list.   it would be nice
 *    if we could peek at the VCI on top of the hwservice list without removing
 *    it, however this leads to a race condition: if we peek at it and
 *    decide we are done with it new data could come in before we have a
 *    chance to remove it from the hwslist.   by the time we get it out of
 *    the list the interrupt for the new data will be lost.   oops!
 *
 */

STATIC void en_service(struct en_softc *sc)
{
  struct mbuf *m, *tmp;
  u_int32_t cur, dstart, rbd, pdu, *sav, dma, bcode, count, *data, *datastop;
  u_int32_t start, stop, cnt, needalign;
  int slot, raw, aal5, vci, fill, mlen, tlen, drqneed, need, needfill, end;

  aal5 = 0;		/* Silence gcc */
next_vci:
  if (sc->swsl_size == 0) {
#ifdef EN_DEBUG
    printf("%s: en_service done\n", device_xname(sc->sc_dev));
#endif
    return;		/* >>> exit here if swsl now empty <<< */
  }

  /*
   * get slot/vci to service
   */

  slot = sc->swslist[sc->swsl_head];
  vci = sc->rxslot[slot].atm_vci;
#ifdef EN_DIAG
  if (sc->rxvc2slot[vci] != slot) panic("en_service rx slot/vci sync");
#endif

  /*
   * determine our mode and if we've got any work to do
   */

  raw = sc->rxslot[slot].oth_flags & ENOTHER_RAW;
  start= sc->rxslot[slot].start;
  stop= sc->rxslot[slot].stop;
  cur = sc->rxslot[slot].cur;

#ifdef EN_DEBUG
  printf("%s: rx%d: service vci=%d raw=%d start/stop/cur=0x%x 0x%x 0x%x\n",
	device_xname(sc->sc_dev), slot, vci, raw, start, stop, cur);
#endif

same_vci:
  dstart = MIDV_DSTART(EN_READ(sc, MID_DST_RP(vci)));
  dstart = (dstart * sizeof(u_int32_t)) + start;

  /* check to see if there is any data at all */
  if (dstart == cur) {
defer:					/* defer processing */
    EN_WRAPADD(0, MID_SL_N, sc->swsl_head, 1);
    sc->rxslot[slot].oth_flags &= ~ENOTHER_SWSL;
    sc->swsl_size--;
					/* >>> remove from swslist <<< */
#ifdef EN_DEBUG
    printf("%s: rx%d: remove vci %d from swslist\n",
	device_xname(sc->sc_dev), slot, vci);
#endif
    goto next_vci;
  }

  /*
   * figure out how many bytes we need
   * [mlen = # bytes to go in mbufs, fill = # bytes to dump (MIDDMA_JK)]
   */

  if (raw) {

    /* raw mode (aka boodi mode) */
    fill = 0;
    if (dstart > cur)
      mlen = dstart - cur;
    else
      mlen = (dstart + (EN_RXSZ*1024)) - cur;

    if (mlen < sc->rxslot[slot].raw_threshold)
      goto defer; 		/* too little data to deal with */

  } else {

    /* normal mode */
    aal5 = (sc->rxslot[slot].atm_flags & ATM_PH_AAL5);
    rbd = EN_READ(sc, cur);
    if (MID_RBD_ID(rbd) != MID_RBD_STDID)
      panic("en_service: id mismatch");

    if (rbd & MID_RBD_T) {
      mlen = 0;			/* we've got trash */
      fill = MID_RBD_SIZE;
      EN_COUNT(sc->ttrash);
#ifdef EN_DEBUG
      printf("RX overflow lost %d cells!\n", MID_RBD_CNT(rbd));
#endif
    } else if (!aal5) {
      mlen = MID_RBD_SIZE + MID_CHDR_SIZE + MID_ATMDATASZ; /* 1 cell (ick!) */
      fill = 0;
    } else {
      struct ifnet *ifp;

      tlen = (MID_RBD_CNT(rbd) * MID_ATMDATASZ) + MID_RBD_SIZE;
      pdu = cur + tlen - MID_PDU_SIZE;
      if (pdu >= stop)
	pdu -= (EN_RXSZ*1024);
      pdu = EN_READ(sc, pdu);	/* get PDU in correct byte order */
      fill = tlen - MID_RBD_SIZE - MID_PDU_LEN(pdu);
      if (fill < 0 || (rbd & MID_RBD_CRCERR) != 0) {
	static int first = 1;

	if (first) {
	  printf("%s: %s, dropping frame\n", device_xname(sc->sc_dev),
		 (rbd & MID_RBD_CRCERR) ?
		 "CRC error" : "invalid AAL5 PDU length");
	  printf("%s: got %d cells (%d bytes), AAL5 len is %d bytes (pdu=0x%x)\n",
		 device_xname(sc->sc_dev), MID_RBD_CNT(rbd),
		 tlen - MID_RBD_SIZE, MID_PDU_LEN(pdu), pdu);
#ifndef EN_DEBUG
	  printf("CRC error report disabled from now on!\n");
	  first = 0;
#endif
	}
	fill = tlen;

#ifdef ATM_PVCEXT
	ifp = en_vci2ifp(sc, vci);
#else
	ifp = &sc->enif;
#endif
	ifp->if_ierrors++;

      }
      mlen = tlen - fill;
    }

  }

  /*
   * now allocate mbufs for mlen bytes of data, if out of mbufs, trash all
   *
   * notes:
   *  1. it is possible that we've already allocated an mbuf for this pkt
   *	 but ran out of DRQs, in which case we saved the allocated mbuf on
   *	 "q".
   *  2. if we save an mbuf in "q" we store the "cur" (pointer) in the front
   *     of the mbuf as an identity (that we can check later), and we also
   *     store drqneed (so we don't have to recompute it).
   *  3. after this block of code, if m is still NULL then we ran out of mbufs
   */

  m = sc->rxslot[slot].q.ifq_head;
  drqneed = 1;
  if (m) {
    sav = mtod(m, u_int32_t *);
    if (sav[0] != cur) {
#ifdef EN_DEBUG
      printf("%s: rx%d: q'ed mbuf %p not ours\n",
		device_xname(sc->sc_dev), slot, m);
#endif
      m = NULL;			/* wasn't ours */
      EN_COUNT(sc->rxqnotus);
    } else {
      EN_COUNT(sc->rxqus);
      IF_DEQUEUE(&sc->rxslot[slot].q, m);
      drqneed = sav[1];
#ifdef EN_DEBUG
      printf("%s: rx%d: recovered q'ed mbuf %p (drqneed=%d)\n",
	device_xname(sc->sc_dev), slot, m, drqneed);
#endif
    }
  }

  if (mlen != 0 && m == NULL) {
    m = en_mget(sc, mlen, &drqneed);		/* allocate! */
    if (m == NULL) {
      fill += mlen;
      mlen = 0;
      EN_COUNT(sc->rxmbufout);
#ifdef EN_DEBUG
      printf("%s: rx%d: out of mbufs\n", device_xname(sc->sc_dev), slot);
#endif
    }
#ifdef EN_DEBUG
    printf("%s: rx%d: allocate mbuf %p, mlen=%d, drqneed=%d\n",
	device_xname(sc->sc_dev), slot, m, mlen, drqneed);
#endif
  }

#ifdef EN_DEBUG
  printf("%s: rx%d: VCI %d, mbuf_chain %p, mlen %d, fill %d\n",
	device_xname(sc->sc_dev), slot, vci, m, mlen, fill);
#endif

  /*
   * now check to see if we've got the DRQs needed.    if we are out of
   * DRQs we must quit (saving our mbuf, if we've got one).
   */

  needfill = (fill) ? 1 : 0;
  if (drqneed + needfill > sc->drq_free) {
    sc->need_drqs = 1;	/* flag condition */
    if (m == NULL) {
      EN_COUNT(sc->rxoutboth);
#ifdef EN_DEBUG
      printf("%s: rx%d: out of DRQs *and* mbufs!\n", device_xname(sc->sc_dev), slot);
#endif
      return;		/* >>> exit here if out of both mbufs and DRQs <<< */
    }
    sav = mtod(m, u_int32_t *);
    sav[0] = cur;
    sav[1] = drqneed;
    IF_ENQUEUE(&sc->rxslot[slot].q, m);
    EN_COUNT(sc->rxdrqout);
#ifdef EN_DEBUG
    printf("%s: rx%d: out of DRQs\n", device_xname(sc->sc_dev), slot);
#endif
    return;		/* >>> exit here if out of DRQs <<< */
  }

  /*
   * at this point all resources have been allocated and we are commited
   * to servicing this slot.
   *
   * dma = last location we told chip about
   * cur = current location
   * mlen = space in the mbuf we want
   * need = bytes to xfer in (decrs to zero)
   * fill = how much fill we need
   * tlen = how much data to transfer to this mbuf
   * cnt/bcode/count = <same as xmit>
   *
   * 'needfill' not used after this point
   */

  dma = cur;		/* dma = last location we told chip about */
  need = roundup(mlen, sizeof(u_int32_t));
  fill = fill - (need - mlen);  /* note: may invalidate 'needfill' */

  for (tmp = m ; tmp != NULL && need > 0 ; tmp = tmp->m_next) {
    tlen = roundup(tmp->m_len, sizeof(u_int32_t)); /* m_len set by en_mget */
    data = mtod(tmp, u_int32_t *);

#ifdef EN_DEBUG
    printf("%s: rx%d: load mbuf %p, m_len=%d, m_data=%p, tlen=%d\n",
	device_xname(sc->sc_dev), slot, tmp, tmp->m_len, tmp->m_data, tlen);
#endif

    /* copy data */
    if (EN_NORXDMA || !en_dma || tlen < EN_MINDMA) {
      datastop = (u_int32_t *)((u_char *) data + tlen);
      /* copy loop: preserve byte order!!!  use READDAT */
      while (data != datastop) {
	*data = EN_READDAT(sc, cur);
	data++;
	EN_WRAPADD(start, stop, cur, 4);
      }
      need -= tlen;
#ifdef EN_DEBUG
      printf("%s: rx%d: vci%d: copied %d bytes (%d left)\n",
		device_xname(sc->sc_dev), slot, vci, tlen, need);
#endif
      continue;
    }

    /* DMA data (check to see if we need to sync DRQ first) */
    if (dma != cur) {
      EN_DRQADD(sc, WORD_IDX(start,cur), vci, MIDDMA_JK, 0, 0, 0, 0);
#ifdef EN_DEBUG
      printf("%s: rx%d: vci%d: drq_sync: advance pointer to %d\n",
		device_xname(sc->sc_dev), slot, vci, cur);
#endif
    }

#if !defined(MIDWAY_ENIONLY)

    /*
     * the adaptec DMA engine is smart and handles everything for us.
     */

    if (sc->is_adaptec) {
      need -= tlen;
      EN_WRAPADD(start, stop, cur, tlen);
#ifdef EN_DEBUG
      printf("%s: rx%d: vci%d: adp_dma %d bytes (%d left)\n",
		device_xname(sc->sc_dev), slot, vci, tlen, need);
#endif
      end = (need == 0 && !fill) ? MID_DMA_END : 0;
      EN_DRQADD(sc, tlen, vci, 0, vtophys((vaddr_t)data), mlen, slot, end);
      if (end)
        goto done;
      dma = cur;	/* update DMA pointer */
      continue;
    }
#endif /* !MIDWAY_ENIONLY */


#if !defined(MIDWAY_ADPONLY)

    /*
     * the ENI DMA engine is not so smart and need more help from us
     */

    /* do we need to do a DMA op to align? */
    if (sc->alburst &&
      (needalign = (((unsigned long) data) & sc->bestburstmask)) != 0) {
      cnt = sc->bestburstlen - needalign;
      if (cnt > tlen) {
        cnt = tlen;
        count = cnt / sizeof(u_int32_t);
        bcode = MIDDMA_WORD;
      } else {
        count = cnt / sizeof(u_int32_t);
        bcode = en_dmaplan[count].bcode;
        count = cnt >> en_dmaplan[count].divshift;
      }
      need -= cnt;
      EN_WRAPADD(start, stop, cur, cnt);
#ifdef EN_DEBUG
      printf("%s: rx%d: vci%d: al_dma %d bytes (%d left)\n",
		device_xname(sc->sc_dev), slot, vci, cnt, need);
#endif
      tlen -= cnt;
      end = (need == 0 && !fill) ? MID_DMA_END : 0;
      EN_DRQADD(sc, count, vci, bcode, vtophys((vaddr_t)data), mlen, slot, end);
      if (end)
        goto done;
      data = (u_int32_t *)((u_char *) data + cnt);
    }

    /* do we need a max-sized burst? */
    if (tlen >= sc->bestburstlen) {
      count = tlen >> sc->bestburstshift;
      cnt = count << sc->bestburstshift;
      bcode = sc->bestburstcode;
      need -= cnt;
      EN_WRAPADD(start, stop, cur, cnt);
#ifdef EN_DEBUG
      printf("%s: rx%d: vci%d: best_dma %d bytes (%d left)\n",
		device_xname(sc->sc_dev), slot, vci, cnt, need);
#endif
      tlen -= cnt;
      end = (need == 0 && !fill) ? MID_DMA_END : 0;
      EN_DRQADD(sc, count, vci, bcode, vtophys((vaddr_t)data), mlen, slot, end);
      if (end)
        goto done;
      data = (u_int32_t *)((u_char *) data + cnt);
    }

    /* do we need to do a cleanup burst? */
    if (tlen) {
      count = tlen / sizeof(u_int32_t);
      bcode = en_dmaplan[count].bcode;
      count = tlen >> en_dmaplan[count].divshift;
      need -= tlen;
      EN_WRAPADD(start, stop, cur, tlen);
#ifdef EN_DEBUG
      printf("%s: rx%d: vci%d: cleanup_dma %d bytes (%d left)\n",
		device_xname(sc->sc_dev), slot, vci, tlen, need);
#endif
      end = (need == 0 && !fill) ? MID_DMA_END : 0;
      EN_DRQADD(sc, count, vci, bcode, vtophys((vaddr_t)data), mlen, slot, end);
      if (end)
        goto done;
    }

    dma = cur;		/* update DMA pointer */

#endif /* !MIDWAY_ADPONLY */

  }

  /* skip the end */
  if (fill || dma != cur) {
#ifdef EN_DEBUG
      if (fill)
        printf("%s: rx%d: vci%d: skipping %d bytes of fill\n",
		device_xname(sc->sc_dev), slot, vci, fill);
      else
        printf("%s: rx%d: vci%d: syncing chip from 0x%x to 0x%x [cur]\n",
		device_xname(sc->sc_dev), slot, vci, dma, cur);
#endif
    EN_WRAPADD(start, stop, cur, fill);
    EN_DRQADD(sc, WORD_IDX(start,cur), vci, MIDDMA_JK, 0, mlen,
					slot, MID_DMA_END);
    /* dma = cur; */	/* not necessary since we are done */
  }

  /*
   * done, remove stuff we don't want to pass up:
   *   raw mode (boodi mode): pass everything up for later processing
   *   aal5: remove RBD
   *   aal0: remove RBD + cell header
   */

done:
  if (m) {
    if (!raw) {
      cnt = MID_RBD_SIZE;
      if (!aal5) cnt += MID_CHDR_SIZE;
      m->m_len -= cnt;				/* chop! */
      m->m_pkthdr.len -= cnt;
      m->m_data += cnt;
    }
    IF_ENQUEUE(&sc->rxslot[slot].indma, m);
  }
  sc->rxslot[slot].cur = cur;		/* update master copy of 'cur' */

#ifdef EN_DEBUG
  printf("%s: rx%d: vci%d: DONE!   cur now =0x%x\n",
	device_xname(sc->sc_dev), slot, vci, cur);
#endif

  goto same_vci;	/* get next packet in this slot */
}


#ifdef EN_DDBHOOK
/*
 * functions we can call from ddb
 */

/*
 * en_dump: dump the state
 */

#define END_SWSL	0x00000040		/* swsl state */
#define END_DRQ		0x00000020		/* drq state */
#define END_DTQ		0x00000010		/* dtq state */
#define END_RX		0x00000008		/* rx state */
#define END_TX		0x00000004		/* tx state */
#define END_MREGS	0x00000002		/* registers */
#define END_STATS	0x00000001		/* dump stats */

#define END_BITS "\20\7SWSL\6DRQ\5DTQ\4RX\3TX\2MREGS\1STATS"

int en_dump(int unit, int level)
{
  struct en_softc *sc;
  int lcv, cnt, slot;
  u_int32_t ptr, reg;

  for (lcv = 0 ; lcv < en_cd.cd_ndevs ; lcv++) {
    char sbuf[256];

    sc = device_lookup_private(&en_cd, lcv);
    if (sc == NULL) continue;
    if (unit != -1 && unit != lcv)
      continue;

    snprintb(sbuf, sizeof(sbuf), END_BITS, level);
    printf("dumping device %s at level 0x%s\n", device_xname(sc->sc_dev), sbuf);

    if (sc->dtq_us == 0) {
      printf("<hasn't been en_init'd yet>\n");
      continue;
    }

    if (level & END_STATS) {
      printf("  en_stats:\n");
      printf("    %d mfix (%d failed); %d/%d head/tail byte DMAs, %d flushes\n",
	   sc->mfix, sc->mfixfail, sc->headbyte, sc->tailbyte, sc->tailflush);
      printf("    %d rx DMA overflow interrupts\n", sc->dmaovr);
      printf("    %d times we ran out of TX space and stalled\n",
							sc->txoutspace);
      printf("    %d times we ran out of DTQs\n", sc->txdtqout);
      printf("    %d times we launched a packet\n", sc->launch);
      printf("    %d times we launched without on-board header\n", sc->lheader);
      printf("    %d times we launched without on-board tail\n", sc->ltail);
      printf("    %d times we pulled the hw service list\n", sc->hwpull);
      printf("    %d times we pushed a vci on the sw service list\n",
								sc->swadd);
      printf("    %d times RX pulled an mbuf from Q that wasn't ours\n",
							 sc->rxqnotus);
      printf("    %d times RX pulled a good mbuf from Q\n", sc->rxqus);
      printf("    %d times we ran out of mbufs *and* DRQs\n", sc->rxoutboth);
      printf("    %d times we ran out of DRQs\n", sc->rxdrqout);

      printf("    %d transmit packets dropped due to mbsize\n", sc->txmbovr);
      printf("    %d cells trashed due to turned off rxvc\n", sc->vtrash);
      printf("    %d cells trashed due to totally full buffer\n", sc->otrash);
      printf("    %d cells trashed due almost full buffer\n", sc->ttrash);
      printf("    %d rx mbuf allocation failures\n", sc->rxmbufout);
#ifdef NATM
      printf("    %d drops at natmintrq\n", natmintrq.ifq_drops);
#ifdef NATM_STAT
      printf("    natmintr so_rcv: ok/drop cnt: %d/%d, ok/drop bytes: %d/%d\n",
	natm_sookcnt, natm_sodropcnt, natm_sookbytes, natm_sodropbytes);
#endif
#endif
    }

    if (level & END_MREGS) {
      char ybuf[256];

      printf("mregs:\n");
      printf("resid = 0x%x\n", EN_READ(sc, MID_RESID));

      snprintb(ybuf, sizeof(ybuf), MID_INTBITS, EN_READ(sc, MID_INTSTAT));
      printf("interrupt status = 0x%s\n", ybuf);

      snprintb(ybuf, sizeof(ybuf), MID_INTBITS, EN_READ(sc, MID_INTENA));
      printf("interrupt enable = 0x%s\n", ybuf);

      snprintb(ybuf, sizeof(ybuf), MID_MCSRBITS, EN_READ(sc, MID_MAST_CSR));
      printf("mcsr = 0x%s\n", ybuf);

      printf("serv_write = [chip=%d] [us=%d]\n", EN_READ(sc, MID_SERV_WRITE),
			MID_SL_A2REG(sc->hwslistp));
      printf("DMA addr = 0x%x\n", EN_READ(sc, MID_DMA_ADDR));
      printf("DRQ: chip[rd=0x%x,wr=0x%x], sc[chip=0x%x,us=0x%x]\n",
	MID_DRQ_REG2A(EN_READ(sc, MID_DMA_RDRX)),
	MID_DRQ_REG2A(EN_READ(sc, MID_DMA_WRRX)), sc->drq_chip, sc->drq_us);
      printf("DTQ: chip[rd=0x%x,wr=0x%x], sc[chip=0x%x,us=0x%x]\n",
	MID_DTQ_REG2A(EN_READ(sc, MID_DMA_RDTX)),
	MID_DTQ_REG2A(EN_READ(sc, MID_DMA_WRTX)), sc->dtq_chip, sc->dtq_us);

      printf("  unusal txspeeds: ");
      for (cnt = 0 ; cnt < MID_N_VC ; cnt++)
	if (sc->txspeed[cnt])
	  printf(" vci%d=0x%x", cnt, sc->txspeed[cnt]);
      printf("\n");

      printf("  rxvc slot mappings: ");
      for (cnt = 0 ; cnt < MID_N_VC ; cnt++)
	if (sc->rxvc2slot[cnt] != RX_NONE)
	  printf("  %d->%d", cnt, sc->rxvc2slot[cnt]);
      printf("\n");

    }

    if (level & END_TX) {
      printf("tx:\n");
      for (slot = 0 ; slot < EN_NTX; slot++) {
	printf("tx%d: start/stop/cur=0x%x/0x%x/0x%x [%d]  ", slot,
	  sc->txslot[slot].start, sc->txslot[slot].stop, sc->txslot[slot].cur,
		(sc->txslot[slot].cur - sc->txslot[slot].start)/4);
	printf("mbsize=%d, bfree=%d\n", sc->txslot[slot].mbsize,
		sc->txslot[slot].bfree);
        printf("txhw: base_address=0x%lx, size=%d, read=%d, descstart=%d\n",
	  (u_long)MIDX_BASE(EN_READ(sc, MIDX_PLACE(slot))),
	  MIDX_SZ(EN_READ(sc, MIDX_PLACE(slot))),
	  EN_READ(sc, MIDX_READPTR(slot)), EN_READ(sc, MIDX_DESCSTART(slot)));
      }
    }

    if (level & END_RX) {
      printf("  recv slots:\n");
      for (slot = 0 ; slot < sc->en_nrx; slot++) {
	printf("rx%d: vci=%d: start/stop/cur=0x%x/0x%x/0x%x ", slot,
	  sc->rxslot[slot].atm_vci, sc->rxslot[slot].start,
	  sc->rxslot[slot].stop, sc->rxslot[slot].cur);
	printf("mode=0x%x, atm_flags=0x%x, oth_flags=0x%x\n",
	sc->rxslot[slot].mode, sc->rxslot[slot].atm_flags,
		sc->rxslot[slot].oth_flags);
        printf("RXHW: mode=0x%x, DST_RP=0x%x, WP_ST_CNT=0x%x\n",
	  EN_READ(sc, MID_VC(sc->rxslot[slot].atm_vci)),
	  EN_READ(sc, MID_DST_RP(sc->rxslot[slot].atm_vci)),
	  EN_READ(sc, MID_WP_ST_CNT(sc->rxslot[slot].atm_vci)));
      }
    }

    if (level & END_DTQ) {
      printf("  dtq [need_dtqs=%d,dtq_free=%d]:\n",
					sc->need_dtqs, sc->dtq_free);
      ptr = sc->dtq_chip;
      while (ptr != sc->dtq_us) {
        reg = EN_READ(sc, ptr);
        printf("\t0x%x=[cnt=%d, chan=%d, end=%d, type=%d @ 0x%x]\n",
	    sc->dtq[MID_DTQ_A2REG(ptr)], MID_DMA_CNT(reg), MID_DMA_TXCHAN(reg),
	    (reg & MID_DMA_END) != 0, MID_DMA_TYPE(reg), EN_READ(sc, ptr+4));
        EN_WRAPADD(MID_DTQOFF, MID_DTQEND, ptr, 8);
      }
    }

    if (level & END_DRQ) {
      printf("  drq [need_drqs=%d,drq_free=%d]:\n",
					sc->need_drqs, sc->drq_free);
      ptr = sc->drq_chip;
      while (ptr != sc->drq_us) {
        reg = EN_READ(sc, ptr);
	printf("\t0x%x=[cnt=%d, chan=%d, end=%d, type=%d @ 0x%x]\n",
	  sc->drq[MID_DRQ_A2REG(ptr)], MID_DMA_CNT(reg), MID_DMA_RXVCI(reg),
	  (reg & MID_DMA_END) != 0, MID_DMA_TYPE(reg), EN_READ(sc, ptr+4));
	EN_WRAPADD(MID_DRQOFF, MID_DRQEND, ptr, 8);
      }
    }

    if (level & END_SWSL) {
      printf(" swslist [size=%d]: ", sc->swsl_size);
      for (cnt = sc->swsl_head ; cnt != sc->swsl_tail ;
			cnt = (cnt + 1) % MID_SL_N)
        printf("0x%x ", sc->swslist[cnt]);
      printf("\n");
    }
  }
  return(0);
}

/*
 * en_dumpmem: dump the memory
 */

int en_dumpmem(int unit, int addr, int len)
{
  struct en_softc *sc;
  u_int32_t reg;

  sc = device_lookup_private(&en_cd, unit);
  if (sc == NULL) {
    printf("invalid unit number: %d\n", unit);
    return(0);
  }
  addr = addr & ~3;
  if (addr < MID_RAMOFF || addr + len*4 > MID_MAXOFF || len <= 0) {
    printf("invalid addr/len number: %d, %d\n", addr, len);
    return(0);
  }
  printf("dumping %d words starting at offset 0x%x\n", len, addr);
  while (len--) {
    reg = EN_READ(sc, addr);
    printf("mem[0x%x] = 0x%x\n", addr, reg);
    addr += 4;
  }
  return(0);
}
#endif

#ifdef ATM_PVCEXT
/*
 * ATM PVC extension: shaper control and pvc subinterfaces
 */

/*
 * the list of the interfaces sharing the physical device.
 * in order to avoid starvation, the interfaces are scheduled in
 * a round-robin fashion when en_start is called from tx complete
 * interrupts.
 */
static void rrp_add(struct en_softc *sc, struct ifnet *ifp)
{
	struct rrp *head, *p, *new;

	head = sc->txrrp;
	if ((p = head) != NULL) {
		while (1) {
			if (p->ifp == ifp) {
				/* an entry for this ifp already exits */
				p->nref++;
				return;
			}
			if (p->next == head)
				break;
			p = p->next;
		}
	}

	/* create a new entry */
	new = malloc(sizeof(struct rrp), M_DEVBUF, M_WAITOK);
	if (new == NULL) {
		printf("en_rrp_add: malloc failed!\n");
		return;
	}

	new->ifp = ifp;
	new->nref = 1;

	if (p == NULL) {
		/* this is the only one in the list */
		new->next = new;
		sc->txrrp = new;
	}
	else {
		/* add the new entry at the tail of the list */
		new->next = p->next;
		p->next = new;
	}
}

#if 0 /* not used */
static void rrp_delete(struct en_softc *sc, struct ifnet *ifp)
{
	struct rrp *head, *p, *prev;

	head = sc->txrrp;

	prev = head;
	if (prev == NULL) {
		printf("rrp_delete: no list!\n");
		return;
	}
	p = prev->next;

	while (1) {
		if (p->ifp == ifp) {
			p->nref--;
			if (p->nref > 0)
				return;
			/* remove this entry */
			if (p == prev) {
				/* this is the only entry in the list */
				sc->txrrp = NULL;
			}
			else {
				prev->next = p->next;
				if (head == p)
					sc->txrrp = p->next;
			}
			free(p, M_DEVBUF);
		}
		prev = p;
		p = prev->next;
		if (prev == head) {
			printf("rrp_delete: no matching entry!\n");
			return;
		}
	}
}
#endif

static struct ifnet *
en_vci2ifp(struct en_softc *sc, int vci)
{
	struct pvcsif *pvcsif;

	LIST_FOREACH(pvcsif, &sc->sif_list, sif_links) {
		if (vci == pvcsif->sif_vci)
			return (&pvcsif->sif_if);
	}
	return (&sc->enif);
}

/*
 * create and attach per pvc subinterface
 * (currently detach is not supported)
 */
static struct ifnet *
en_pvcattach(struct ifnet *ifp)
{
	struct en_softc *sc = (struct en_softc *) ifp->if_softc;
	struct ifnet *pvc_ifp;
	int s;

	if ((pvc_ifp = pvcsif_alloc()) == NULL)
		return (NULL);

	pvc_ifp->if_softc = sc;
	pvc_ifp->if_ioctl = en_ioctl;
	pvc_ifp->if_start = en_start;
	pvc_ifp->if_flags = (IFF_POINTOPOINT|IFF_MULTICAST) |
		(ifp->if_flags & (IFF_RUNNING|IFF_SIMPLEX|IFF_NOTRAILERS));

	s = splnet();
	LIST_INSERT_HEAD(&sc->sif_list, (struct pvcsif *)pvc_ifp, sif_links);
	if_attach(pvc_ifp);
	atm_ifattach(pvc_ifp);

#ifdef ATM_PVCEXT
	rrp_add(sc, pvc_ifp);
#endif
	splx(s);

	return (pvc_ifp);
}


/* txspeed conversion derived from linux drivers/atm/eni.c
   by Werner Almesberger, EPFL LRC */
static const int pre_div[] = { 4,16,128,2048 };

static int en_pcr2txspeed(int pcr)
{
	int pre, res, div;

	if (pcr == 0 || pcr > 347222)
		pre = res = 0;	/* max rate */
	else {
		for (pre = 0; pre < 3; pre++)
			if (25000000/pre_div[pre]/64 <= pcr)
				break;
		div = pre_div[pre]*(pcr);
#if 1
		/*
		 * the shaper value should be rounded down,
		 * instead of rounded up.
		 * (which means "res" should be rounded up.)
		 */
		res = (25000000 + div -1)/div - 1;
#else
		res = 25000000/div-1;
#endif
		if (res < 0)
			res = 0;
		if (res > 63)
			res = 63;
	}
	return ((pre << 6) + res);
}

static int en_txspeed2pcr(int txspeed)
{
	int pre, res, pcr;

	pre = (txspeed >> 6) & 0x3;
	res = txspeed & 0x3f;
	pcr = 25000000 / pre_div[pre] / (res+1);
	return (pcr);
}

/*
 * en_txctl selects a hardware transmit channel and sets the shaper value.
 * en_txctl should be called after enabling the vc by en_rxctl
 * since it assumes a transmit channel is already assigned by en_rxctl
 * to the vc.
 */
static int en_txctl(struct en_softc *sc, int vci, int joint_vci, int pcr)
{
	int txspeed, txchan, s;

	if (pcr)
		txspeed = en_pcr2txspeed(pcr);
	else
		txspeed = 0;

	s = splnet();
	txchan = sc->txvc2slot[vci];
	sc->txslot[txchan].nref--;

	/* select a slot */
	if (joint_vci != 0)
		/* use the same channel */
		txchan = sc->txvc2slot[joint_vci];
	else if (pcr == 0)
		txchan = 0;
	else {
		for (txchan = 1; txchan < EN_NTX; txchan++) {
			if (sc->txslot[txchan].nref == 0)
				break;
		}
	}
	if (txchan == EN_NTX) {
#if 1
		/* no free slot! */
		splx(s);
		return (ENOSPC);
#else
		/*
		 * to allow multiple vc's to share a slot,
		 * use a slot with the smallest reference count
		 */
		int slot = 1;
		txchan = 1;
		for (slot = 2; slot < EN_NTX; slot++)
			if (sc->txslot[slot].nref < sc->txslot[txchan].nref)
				txchan = slot;
#endif
	}

	sc->txvc2slot[vci] = txchan;
	sc->txslot[txchan].nref++;

	/* set the shaper parameter */
	sc->txspeed[vci] = (u_int8_t)txspeed;

	splx(s);
#ifdef EN_DEBUG
	printf("VCI:%d PCR set to %d, tx channel %d\n", vci, pcr, txchan);
	if (joint_vci != 0)
		printf("  slot shared with VCI:%d\n", joint_vci);
#endif
	return (0);
}

static int en_pvctx(struct en_softc *sc, struct pvctxreq *pvcreq)
{
	struct ifnet *ifp;
	struct atm_pseudoioctl api;
	struct atm_pseudohdr *pvc_aph, *pvc_joint;
	int vci, joint_vci, pcr;
	int error = 0;

	/* check vpi:vci values */
	pvc_aph = &pvcreq->pvc_aph;
	pvc_joint = &pvcreq->pvc_joint;

	vci = ATM_PH_VCI(pvc_aph);
	joint_vci = ATM_PH_VCI(pvc_joint);
	pcr = pvcreq->pvc_pcr;

	if (ATM_PH_VPI(pvc_aph) != 0 || vci >= MID_N_VC ||
	    ATM_PH_VPI(pvc_joint) != 0 || joint_vci >= MID_N_VC)
		return (EADDRNOTAVAIL);

	if ((ifp = ifunit(pvcreq->pvc_ifname)) == NULL)
		return (ENXIO);

	if (pcr < 0) {
		/* negative pcr means disable the vc. */
		if (sc->rxvc2slot[vci] == RX_NONE)
			/* already disabled */
			return 0;

		ATM_PH_FLAGS(&api.aph) = 0;
		ATM_PH_VPI(&api.aph) = 0;
		ATM_PH_SETVCI(&api.aph, vci);
		api.rxhand = NULL;

		error = en_rxctl(sc, &api, 0);

		if (error == 0 && &sc->enif != ifp) {
			/* clear vc info of this subinterface */
			struct pvcsif *pvcsif = (struct pvcsif *)ifp;

			ATM_PH_SETVCI(&api.aph, 0);
			pvcsif->sif_aph = api.aph;
			pvcsif->sif_vci = 0;
		}
		return (error);
	}

	if (&sc->enif == ifp) {
		/* called for an en interface */
		if (sc->rxvc2slot[vci] == RX_NONE) {
			/* vc is not active */
#ifdef __NetBSD__
			printf("%s: en_pvctx: rx not active! vci=%d\n",
			       ifp->if_xname, vci);
#else
			printf("%s%d: en_pvctx: rx not active! vci=%d\n",
			       ifp->if_name, ifp->if_unit, vci);
#endif
			return (EINVAL);
		}
	}
	else {
		/* called for a pvc subinterface */
		struct pvcsif *pvcsif = (struct pvcsif *)ifp;

#ifdef __NetBSD__
    		strlcpy(pvcreq->pvc_ifname, sc->enif.if_xname,
		    sizeof(pvcreq->pvc_ifname));
#else
    		snprintf(pvcreq->pvc_ifname, sizeof(pvcreq->pvc_ifname), "%s%d",
		    sc->enif.if_name, sc->enif.if_unit);
#endif
		ATM_PH_FLAGS(&api.aph) =
			(ATM_PH_FLAGS(pvc_aph) & (ATM_PH_AAL5|ATM_PH_LLCSNAP));
		ATM_PH_VPI(&api.aph) = 0;
		ATM_PH_SETVCI(&api.aph, vci);
		api.rxhand = NULL;
		pvcsif->sif_aph = api.aph;
		pvcsif->sif_vci = ATM_PH_VCI(&api.aph);

		if (sc->rxvc2slot[vci] == RX_NONE) {
			/* vc is not active, enable rx */
			error = en_rxctl(sc, &api, 1);
			if (error)
				return error;
		}
		else {
			/* vc is already active, update aph in softc */
			sc->rxslot[sc->rxvc2slot[vci]].atm_flags =
				ATM_PH_FLAGS(&api.aph);
		}
	}

	error = en_txctl(sc, vci, joint_vci, pcr);

	if (error == 0) {
		if (sc->txspeed[vci] != 0)
			pvcreq->pvc_pcr = en_txspeed2pcr(sc->txspeed[vci]);
		else
			pvcreq->pvc_pcr = 0;
	}

	return error;
}

static int en_pvctxget(struct en_softc *sc, struct pvctxreq *pvcreq)
{
	struct pvcsif *pvcsif;
	struct ifnet *ifp;
	int vci, slot;

	if ((ifp = ifunit(pvcreq->pvc_ifname)) == NULL)
		return (ENXIO);

	if (ifp == &sc->enif) {
		/* physical interface: assume vci is specified */
		struct atm_pseudohdr *pvc_aph;

		pvc_aph = &pvcreq->pvc_aph;
		vci = ATM_PH_VCI(pvc_aph);
		if ((slot = sc->rxvc2slot[vci]) == RX_NONE)
			ATM_PH_FLAGS(pvc_aph) = 0;
		else
			ATM_PH_FLAGS(pvc_aph) = sc->rxslot[slot].atm_flags;
		ATM_PH_VPI(pvc_aph) = 0;
	}
	else {
		/* pvc subinterface */
#ifdef __NetBSD__
		strlcpy(pvcreq->pvc_ifname, sc->enif.if_xname,
		    sizeof(pvcreq->pvc_ifname));
#else
		snprintf(pvcreq->pvc_ifname, sizeof(pvcreq->pvc_ifname), "%s%d",
		    sc->enif.if_name, sc->enif.if_unit);
#endif

		pvcsif = (struct pvcsif *)ifp;
		pvcreq->pvc_aph = pvcsif->sif_aph;
		vci = pvcsif->sif_vci;
	}

	if ((slot = sc->rxvc2slot[vci]) == RX_NONE) {
		/* vc is not active */
		ATM_PH_FLAGS(&pvcreq->pvc_aph) = 0;
		pvcreq->pvc_pcr = -1;
	}
	else if (sc->txspeed[vci])
		pvcreq->pvc_pcr = en_txspeed2pcr(sc->txspeed[vci]);
	else
		pvcreq->pvc_pcr = 0;

	return (0);
}

#endif /* ATM_PVCEXT */

#endif /* NEN > 0 || !defined(__FreeBSD__) */
