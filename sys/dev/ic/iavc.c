/*	$NetBSD: iavc.c,v 1.12 2014/03/23 02:44:19 christos Exp $	*/

/*
 * Copyright (c) 2001-2003 Cubical Solutions Ltd. All rights reserved.
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
 *	The AVM ISDN controllers' card specific support routines.
 *
 * $FreeBSD: src/sys/i4b/capi/iavc/iavc_card.c,v 1.1.2.1 2001/08/10 14:08:34 obrien Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iavc.c,v 1.12 2014/03/23 02:44:19 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/reboot.h>
#include <net/if.h>

#include <sys/bus.h>

#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>
#include <netisdn/i4b_global.h>
#include <netisdn/i4b_l3l4.h>
#include <netisdn/i4b_mbuf.h>
#include <netisdn/i4b_capi.h>
#include <netisdn/i4b_capi_msgs.h>

#include <dev/ic/iavcvar.h>
#include <dev/ic/iavcreg.h>

/*
//  AVM B1 (active BRI, PIO mode)
*/

int
iavc_b1_detect(iavc_softc_t *sc)
{
    if ((iavc_read_port(sc, B1_INSTAT) & 0xfc) ||
	(iavc_read_port(sc, B1_OUTSTAT) & 0xfc))
	return (1);

    b1io_outp(sc, B1_INSTAT, 0x02);
    b1io_outp(sc, B1_OUTSTAT, 0x02);
    if ((iavc_read_port(sc, B1_INSTAT) & 0xfe) != 2 ||
	(iavc_read_port(sc, B1_OUTSTAT) & 0xfe) != 2)
	return (2);

    b1io_outp(sc, B1_INSTAT, 0x00);
    b1io_outp(sc, B1_OUTSTAT, 0x00);
    if ((iavc_read_port(sc, B1_INSTAT) & 0xfe) ||
	(iavc_read_port(sc, B1_OUTSTAT) & 0xfe))
	return (3);

    return (0); /* found */
}

void
iavc_b1_disable_irq(iavc_softc_t *sc)
{
    b1io_outp(sc, B1_INSTAT, 0x00);
}

void
iavc_b1_reset(iavc_softc_t *sc)
{
    b1io_outp(sc, B1_RESET, 0);
    DELAY(55*2*1000);

    b1io_outp(sc, B1_RESET, 1);
    DELAY(55*2*1000);

    b1io_outp(sc, B1_RESET, 0);
    DELAY(55*2*1000);
}

/*
//  Newer PCI-based B1's, and T1's, supports DMA
*/

int
iavc_b1dma_detect(iavc_softc_t *sc)
{
    AMCC_WRITE(sc, AMCC_MCSR, 0);
    DELAY(10*1000);
    AMCC_WRITE(sc, AMCC_MCSR, 0x0f000000);
    DELAY(10*1000);
    AMCC_WRITE(sc, AMCC_MCSR, 0);
    DELAY(42*1000);

    AMCC_WRITE(sc, AMCC_RXLEN, 0);
    AMCC_WRITE(sc, AMCC_TXLEN, 0);
    sc->sc_csr = 0;
    AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);

    if (AMCC_READ(sc, AMCC_INTCSR) != 0)
	return 1;

    AMCC_WRITE(sc, AMCC_RXPTR, 0xffffffff);
    AMCC_WRITE(sc, AMCC_TXPTR, 0xffffffff);
    if ((AMCC_READ(sc, AMCC_RXPTR) != 0xfffffffc) ||
	(AMCC_READ(sc, AMCC_TXPTR) != 0xfffffffc))
	return 2;

    AMCC_WRITE(sc, AMCC_RXPTR, 0);
    AMCC_WRITE(sc, AMCC_TXPTR, 0);
    if ((AMCC_READ(sc, AMCC_RXPTR) != 0) ||
	(AMCC_READ(sc, AMCC_TXPTR) != 0))
	return 3;

    iavc_write_port(sc, 0x10, 0x00);
    iavc_write_port(sc, 0x07, 0x00);

    iavc_write_port(sc, 0x02, 0x02);
    iavc_write_port(sc, 0x03, 0x02);

    if (((iavc_read_port(sc, 0x02) & 0xfe) != 0x02) ||
	(iavc_read_port(sc, 0x03) != 0x03))
	return 4;

    iavc_write_port(sc, 0x02, 0x00);
    iavc_write_port(sc, 0x03, 0x00);

    if (((iavc_read_port(sc, 0x02) & 0xfe) != 0x00) ||
	(iavc_read_port(sc, 0x03) != 0x01))
	return 5;

    return (0); /* found */
}

void
iavc_b1dma_reset(iavc_softc_t *sc)
{
    int s = splnet();

    sc->sc_csr = 0;
    AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);
    AMCC_WRITE(sc, AMCC_MCSR, 0);
    AMCC_WRITE(sc, AMCC_RXLEN, 0);
    AMCC_WRITE(sc, AMCC_TXLEN, 0);

    iavc_write_port(sc, 0x10, 0x00); /* XXX magic numbers from */
    iavc_write_port(sc, 0x07, 0x00); /* XXX the linux driver */

    splx(s);

    AMCC_WRITE(sc, AMCC_MCSR, 0);
    DELAY(10 * 1000);
    AMCC_WRITE(sc, AMCC_MCSR, 0x0f000000);
    DELAY(10 * 1000);
    AMCC_WRITE(sc, AMCC_MCSR, 0);
    DELAY(42 * 1000);
}

/*
//  AVM T1 (active PRI)
*/

#define b1dma_tx_empty(sc) (b1io_read_reg((sc), T1_OUTSTAT) & 1)
#define b1dma_rx_full(sc) (b1io_read_reg((sc), T1_INSTAT) & 1)

static int b1dma_tolink(iavc_softc_t *sc, void *buf, int len)
{
    volatile int spin;
    char *s = (char*) buf;
    while (len--) {
	spin = 0;
	while (!b1dma_tx_empty(sc) && spin < 100000)
	    spin++;
	if (!b1dma_tx_empty(sc))
	    return -1;
	t1io_outp(sc, 1, *s++);
    }
    return 0;
}

static int b1dma_fromlink(iavc_softc_t *sc, void *buf, int len)
{
    volatile int spin;
    char *s = (char*) buf;
    while (len--) {
	spin = 0;
	while (!b1dma_rx_full(sc) && spin < 100000)
	    spin++;
	if (!b1dma_rx_full(sc))
	    return -1;
	*s++ = t1io_inp(sc, 0);
    }
    return 0;
}

static int WriteReg(iavc_softc_t *sc, u_int32_t reg, u_int8_t val)
{
    u_int8_t cmd = 0;
    if (b1dma_tolink(sc, &cmd, 1) == 0 &&
	b1dma_tolink(sc, &reg, 4) == 0) {
	u_int32_t tmp = val;
	return b1dma_tolink(sc, &tmp, 4);
    }
    return -1;
}

static u_int8_t ReadReg(iavc_softc_t *sc, u_int32_t reg)
{
    u_int8_t cmd = 1;
    if (b1dma_tolink(sc, &cmd, 1) == 0 &&
	b1dma_tolink(sc, &reg, 4) == 0) {
	u_int32_t tmp;
	if (b1dma_fromlink(sc, &tmp, 4) == 0)
	    return (u_int8_t) tmp;
    }
    return 0xff;
}

int
iavc_t1_detect(iavc_softc_t *sc)
{
    int ret = iavc_b1dma_detect(sc);
    if (ret) return ret;

    if ((WriteReg(sc, 0x80001000, 0x11) != 0) ||
	(WriteReg(sc, 0x80101000, 0x22) != 0) ||
	(WriteReg(sc, 0x80201000, 0x33) != 0) ||
	(WriteReg(sc, 0x80301000, 0x44) != 0))
	return 6;

    if ((ReadReg(sc, 0x80001000) != 0x11) ||
	(ReadReg(sc, 0x80101000) != 0x22) ||
	(ReadReg(sc, 0x80201000) != 0x33) ||
	(ReadReg(sc, 0x80301000) != 0x44))
	return 7;

    if ((WriteReg(sc, 0x80001000, 0x55) != 0) ||
	(WriteReg(sc, 0x80101000, 0x66) != 0) ||
	(WriteReg(sc, 0x80201000, 0x77) != 0) ||
	(WriteReg(sc, 0x80301000, 0x88) != 0))
	return 8;

    if ((ReadReg(sc, 0x80001000) != 0x55) ||
	(ReadReg(sc, 0x80101000) != 0x66) ||
	(ReadReg(sc, 0x80201000) != 0x77) ||
	(ReadReg(sc, 0x80301000) != 0x88))
	return 9;

    return 0; /* found */
}

void
iavc_t1_disable_irq(iavc_softc_t *sc)
{
    iavc_write_port(sc, T1_IRQMASTER, 0x00);
}

void
iavc_t1_reset(iavc_softc_t *sc)
{
    iavc_b1_reset(sc);
    iavc_write_port(sc, B1_INSTAT, 0x00);
    iavc_write_port(sc, B1_OUTSTAT, 0x00);
    iavc_write_port(sc, T1_IRQMASTER, 0x00);
    iavc_write_port(sc, T1_RESETBOARD, 0x0f);
}

/* Forward declarations of local subroutines... */

static int iavc_send_init(iavc_softc_t *);

static void iavc_handle_rx(iavc_softc_t *);
static void iavc_start_tx(iavc_softc_t *);

static uint32_t iavc_tx_capimsg(iavc_softc_t *, struct mbuf *);
static uint32_t iavc_tx_ctrlmsg(iavc_softc_t *, struct mbuf *);

/*
//  Callbacks from the upper (capi) layer:
//  --------------------------------------
//
//  iavc_load
//      Resets the board and loads the firmware, then initiates
//      board startup.
//
//  iavc_register
//      Registers a CAPI application id.
//
//  iavc_release
//      Releases a CAPI application id.
//
//  iavc_send
//      Sends a capi message.
*/

int iavc_load(capi_softc_t *capi_sc, int len, u_int8_t *cp)
{
    iavc_softc_t *sc = (iavc_softc_t*) capi_sc->ctx;
    u_int8_t val;

    aprint_debug_dev(sc->sc_dev, "reset card ....\n");

    if (sc->sc_dma)
	iavc_b1dma_reset(sc);	/* PCI cards */
    else if (sc->sc_t1)
	iavc_t1_reset(sc);		/* ISA attachment T1 */
    else
	iavc_b1_reset(sc);		/* ISA attachment B1 */

    DELAY(1000);

    aprint_debug_dev(sc->sc_dev, "start loading %d bytes firmware\n", len);

    while (len && b1io_save_put_byte(sc, *cp++) == 0)
	len--;

    if (len) {
	aprint_error_dev(sc->sc_dev, "loading failed, can't write to card, len = %d\n", len);
	return (EIO);
    }

    aprint_debug_dev(sc->sc_dev, "firmware loaded, wait for ACK\n");

    if(sc->sc_capi.card_type == CARD_TYPEC_AVM_B1_ISA)
	    iavc_put_byte(sc, SEND_POLL);
    else
	    iavc_put_byte(sc, SEND_POLLACK);

    for (len = 0; len < 1000 && !iavc_rx_full(sc); len++)
	DELAY(100);

    if (!iavc_rx_full(sc)) {
	aprint_error_dev(sc->sc_dev, "loading failed, no ack\n");
	return (EIO);
    }

    val = iavc_get_byte(sc);

    if ((sc->sc_dma && val != RECEIVE_POLLDWORD) ||
      (!sc->sc_dma && val != RECEIVE_POLL)) {
	aprint_error_dev(sc->sc_dev, "loading failed, bad ack = %02x\n", val);
	return (EIO);
    }

    aprint_debug_dev(sc->sc_dev, "got ACK = 0x%02x\n", val);

    /* Start the DMA engine */
    if (sc->sc_dma) {
	int s;

	s = splnet();

	sc->sc_csr = AVM_FLAG;
	AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);
	AMCC_WRITE(sc, AMCC_MCSR, (EN_A2P_TRANSFERS|EN_P2A_TRANSFERS|
				   A2P_HI_PRIORITY|P2A_HI_PRIORITY|
				   RESET_A2P_FLAGS|RESET_P2A_FLAGS));

	iavc_write_port(sc, 0x07, 0x30); /* XXX magic numbers from */
	iavc_write_port(sc, 0x10, 0xf0); /* XXX the linux driver */

	bus_dmamap_sync(sc->dmat, sc->rx_map, 0, sc->rx_map->dm_mapsize,
	  BUS_DMASYNC_PREREAD);

	sc->sc_recv1 = 0;
	AMCC_WRITE(sc, AMCC_RXPTR, sc->rx_map->dm_segs[0].ds_addr);
	AMCC_WRITE(sc, AMCC_RXLEN, 4);
	sc->sc_csr |= EN_RX_TC_INT|EN_TX_TC_INT;
	AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);

	splx(s);
    }

#ifdef notyet
    /* good happy place */
    if(sc->sc_capi.card_type == CARD_TYPEC_AVM_B1_ISA)
	b1isa_setup_irq(sc);
#endif

    iavc_send_init(sc);

    return 0;
}

int iavc_register(capi_softc_t *capi_sc, int applid, int nchan)
{
    iavc_softc_t *sc = (iavc_softc_t*) capi_sc->ctx;
    struct mbuf *m = i4b_Dgetmbuf(23);
    u_int8_t *p;

    if (!m) {
	aprint_error("iavc%d: can't get memory\n", sc->sc_unit);
	return (ENOMEM);
    }

    /*
     * byte  0x12 = SEND_REGISTER
     * dword ApplId
     * dword NumMessages
     * dword NumB3Connections 0..nbch
     * dword NumB3Blocks
     * dword B3Size
     */

    p = amcc_put_byte(mtod(m, u_int8_t*), 0);
    p = amcc_put_byte(p, 0);
    p = amcc_put_byte(p, SEND_REGISTER);
    p = amcc_put_word(p, applid);
#if 0
    p = amcc_put_word(p, 1024 + (nchan + 1));
#else
    p = amcc_put_word(p, 1024 * (nchan + 1));
#endif
    p = amcc_put_word(p, nchan);
    p = amcc_put_word(p, 8);
    p = amcc_put_word(p, 2048);

    IF_ENQUEUE(&sc->sc_txq, m);

    iavc_start_tx(sc);

    return 0;
}

int iavc_release(capi_softc_t *capi_sc, int applid)
{
    iavc_softc_t *sc = (iavc_softc_t*) capi_sc->ctx;
    struct mbuf *m = i4b_Dgetmbuf(7);
    u_int8_t *p;

    if (!m) {
	aprint_error_dev(sc->sc_dev, "can't get memory\n");
	return (ENOMEM);
    }

    /*
     * byte  0x14 = SEND_RELEASE
     * dword ApplId
     */

    p = amcc_put_byte(mtod(m, u_int8_t*), 0);
    p = amcc_put_byte(p, 0);
    p = amcc_put_byte(p, SEND_RELEASE);
    p = amcc_put_word(p, applid);

    IF_ENQUEUE(&sc->sc_txq, m);

    iavc_start_tx(sc);
    return 0;
}

int iavc_send(capi_softc_t *capi_sc, struct mbuf *m)
{
    iavc_softc_t *sc = (iavc_softc_t*) capi_sc->ctx;

    if (sc->sc_state != IAVC_UP) {
	aprint_error_dev(sc->sc_dev, "attempt to send before device up\n");

	if (m->m_next) i4b_Bfreembuf(m->m_next);
	i4b_Dfreembuf(m);

	return (ENXIO);
    }

    if (IF_QFULL(&sc->sc_txq)) {
	IF_DROP(&sc->sc_txq);

	aprint_error_dev(sc->sc_dev, "tx overflow, message dropped\n");

	if (m->m_next) i4b_Bfreembuf(m->m_next);
	i4b_Dfreembuf(m);

    } else {
	IF_ENQUEUE(&sc->sc_txq, m);

	iavc_start_tx(sc);
    }

    return 0;
}

/*
//  Functions called by ourself during the initialization sequence:
//  ---------------------------------------------------------------
//
//  iavc_send_init
//      Sends the system initialization message to a newly loaded
//      board, and sets state to INIT.
*/

static int iavc_send_init(iavc_softc_t *sc)
{
    struct mbuf *m = i4b_Dgetmbuf(15);
    u_int8_t *p;
    int s;

    if (!m) {
	aprint_error_dev(sc->sc_dev, "can't get memory\n");
	return (ENOMEM);
    }

    /*
     * byte  0x11 = SEND_INIT
     * dword NumApplications
     * dword NumNCCIs
     * dword BoardNumber
     */

    p = amcc_put_byte(mtod(m, u_int8_t*), 0);
    p = amcc_put_byte(p, 0);
    p = amcc_put_byte(p, SEND_INIT);
    p = amcc_put_word(p, 1); /* XXX MaxAppl XXX */
    p = amcc_put_word(p, sc->sc_capi.sc_nbch);
    p = amcc_put_word(p, sc->sc_unit);

    s = splnet();
    IF_ENQUEUE(&sc->sc_txq, m);

    iavc_start_tx(sc);

    sc->sc_state = IAVC_INIT;
    splx(s);
    return 0;
}

/*
//  Functions called during normal operation:
//  -----------------------------------------
//
//  iavc_receive_init
//      Reads the initialization reply and calls capi_ll_control().
//
//  iavc_receive_new_ncci
//      Reads a new NCCI notification and calls capi_ll_control().
//
//  iavc_receive_free_ncci
//      Reads a freed NCCI notification and calls capi_ll_control().
//
//  iavc_receive_task_ready
//      Reads a task ready message -- which should not occur XXX.
//
//  iavc_receive_debugmsg
//      Reads a debug message -- which should not occur XXX.
//
//  iavc_receive_start
//      Reads a START TRANSMIT message and unblocks device.
//
//  iavc_receive_stop
//      Reads a STOP TRANSMIT message and blocks device.
//
//  iavc_receive
//      Reads an incoming message and calls capi_ll_receive().
*/

static int iavc_receive_init(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t Length;
    u_int8_t *p;
    u_int8_t *cardtype, *serial, *profile, *vers, *caps, *prot;

    if (sc->sc_dma) {
	p = amcc_get_word(dmabuf, &Length);
    } else {
	Length = iavc_get_slice(sc, sc->sc_recvbuf);
	p = sc->sc_recvbuf;
    }

#if 0
    {
	int len = 0;
	printf("%s: rx_init: ", device_xname(sc->sc_dev));
	    while (len < Length) {
		printf(" %02x", p[len]);
		if (len && (len % 16) == 0) printf("\n");
		len++;
	    }
	    if (len % 16) printf("\n");
    }
#endif

    vers = (p + 1);
    p += (*p + 1); /* driver version */
    cardtype = (p + 1);
    p += (*p + 1); /* card type */
    p += (*p + 1); /* hardware ID */
    serial = (p + 1);
    p += (*p + 1); /* serial number */
    caps = (p + 1);
    p += (*p + 1); /* supported options */
    prot = (p + 1);
    p += (*p + 1); /* supported protocols */
    profile = (p + 1);

    if (cardtype && serial && profile) {
	int nbch = ((profile[3]<<8) | profile[2]);

	aprint_normal_dev(sc->sc_dev, "AVM %s, s/n %s, %d chans, f/w rev %s, prot %s\n",
		cardtype, serial, nbch, vers, prot);
	aprint_verbose_dev(sc->sc_dev, "%s\n", caps);

        capi_ll_control(&sc->sc_capi, CAPI_CTRL_PROFILE, (intptr_t) profile);

    } else {
	printf("%s: no profile data in info response?\n", device_xname(sc->sc_dev));
    }

    sc->sc_blocked = 1; /* controller will send START when ready */
    return 0;
}

static int iavc_receive_start(iavc_softc_t *sc)
{
    struct mbuf *m = i4b_Dgetmbuf(3);
    u_int8_t *p;

    if (sc->sc_blocked && sc->sc_state == IAVC_UP)
	printf("%s: receive_start\n", device_xname(sc->sc_dev));

    if (!m) {
	aprint_error_dev(sc->sc_dev, "can't get memory\n");
	return (ENOMEM);
    }

    /*
     * byte  0x73 = SEND_POLLACK
     */

    p = amcc_put_byte(mtod(m, u_int8_t*), 0);
    p = amcc_put_byte(p, 0);
    p = amcc_put_byte(p, SEND_POLLACK);

    IF_PREPEND(&sc->sc_txq, m);

    NDBGL4(L4_IAVCDBG, "%s: blocked = %d, state = %d",
      device_xname(sc->sc_dev), sc->sc_blocked, sc->sc_state);

    sc->sc_blocked = 0;
    iavc_start_tx(sc);

    /* If this was our first START, register our readiness */
    if (sc->sc_state != IAVC_UP) {
	sc->sc_state = IAVC_UP;
	capi_ll_control(&sc->sc_capi, CAPI_CTRL_READY, 1);
    }

    return 0;
}

static int iavc_receive_stop(iavc_softc_t *sc)
{
    printf("%s: receive_stop\n", device_xname(sc->sc_dev));
    sc->sc_blocked = 1;
    return 0;
}

static int iavc_receive_new_ncci(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t ApplId, NCCI, WindowSize;

    if (sc->sc_dma) {
	dmabuf = amcc_get_word(dmabuf, &ApplId);
	dmabuf = amcc_get_word(dmabuf, &NCCI);
	dmabuf = amcc_get_word(dmabuf, &WindowSize);
    } else {
	ApplId = iavc_get_word(sc);
	NCCI   = iavc_get_word(sc);
	WindowSize = iavc_get_word(sc);
    }

    capi_ll_control(&sc->sc_capi, CAPI_CTRL_NEW_NCCI, NCCI);
    return 0;
}

static int iavc_receive_free_ncci(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t ApplId, NCCI;

    if (sc->sc_dma) {
	dmabuf = amcc_get_word(dmabuf, &ApplId);
	dmabuf = amcc_get_word(dmabuf, &NCCI);
    } else {
	ApplId = iavc_get_word(sc);
	NCCI   = iavc_get_word(sc);
    }

    capi_ll_control(&sc->sc_capi, CAPI_CTRL_FREE_NCCI, NCCI);
    return 0;
}

static int iavc_receive_task_ready(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t TaskId, Length;
    u_int8_t *p;
    printf("%s: receive_task_ready\n", device_xname(sc->sc_dev));

    if (sc->sc_dma) {
	p = amcc_get_word(dmabuf, &TaskId);
	p = amcc_get_word(p, &Length);
    } else {
	TaskId = iavc_get_word(sc);
	Length = iavc_get_slice(sc, sc->sc_recvbuf);
	p = sc->sc_recvbuf;
    }

    /* XXX could show the message if trace enabled? XXX */
    return 0;
}

static int iavc_receive_debugmsg(iavc_softc_t *sc, u_int8_t *dmabuf)
{
    u_int32_t Length;
    printf("%s: receive_debugmsg\n", device_xname(sc->sc_dev));

    if (sc->sc_dma) {
	amcc_get_word(dmabuf, &Length);
    } else {
	Length = iavc_get_slice(sc, sc->sc_recvbuf);
    }

    /* XXX could show the message if trace enabled? XXX */
    return 0;
}

static int iavc_receive(iavc_softc_t *sc, u_int8_t *dmabuf, int b3data)
{
    struct mbuf *m;
    u_int32_t ApplId, Length;

    /*
     * byte  0x21 = RECEIVE_MESSAGE
     * dword ApplId
     * dword length
     * ...   CAPI msg
     *
     * --or--
     *
     * byte  0x22 = RECEIVE_DATA_B3_IND
     * dword ApplId
     * dword length
     * ...   CAPI msg
     * dword datalen
     * ...   B3 data
     */

    if (sc->sc_dma) {
	dmabuf = amcc_get_word(dmabuf, &ApplId);
	dmabuf = amcc_get_word(dmabuf, &Length);
    } else {
	ApplId = iavc_get_word(sc);
	Length = iavc_get_slice(sc, sc->sc_recvbuf);
	dmabuf = sc->sc_recvbuf;
    }

    m = i4b_Dgetmbuf(Length);
    if (!m) {
	aprint_error_dev(sc->sc_dev, "can't get memory for receive\n");
	return (ENOMEM);
    }

    memcpy(mtod(m, u_int8_t*), dmabuf, Length);

#if 0
	{
	    u_int8_t *p = mtod(m, u_int8_t*);
	    int len = 0;
	    printf("%s: applid=%d, len=%d\n", device_xname(sc->sc_dev),
	      ApplId, Length);
	    while (len < m->m_len) {
		printf(" %02x", p[len]);
		if (len && (len % 16) == 0) printf("\n");
		len++;
	    }
	    if (len % 16) printf("\n");
	}
#endif

    if (b3data) {
	if (sc->sc_dma) {
	    dmabuf = amcc_get_word(dmabuf + Length, &Length);
	} else {
	    Length = iavc_get_slice(sc, sc->sc_recvbuf);
	    dmabuf = sc->sc_recvbuf;
	}

	m->m_next = i4b_Bgetmbuf(Length);
	if (!m->m_next) {
	    aprint_error_dev(sc->sc_dev, "can't get memory for receive\n");
	    i4b_Dfreembuf(m);
	    return (ENOMEM);
	}

	memcpy(mtod(m->m_next, u_int8_t*), dmabuf, Length);
    }

    capi_ll_receive(&sc->sc_capi, m);
    return 0;
}

/*
//  iavc_handle_intr
//      Checks device interrupt status and calls iavc_handle_{rx,tx}()
//      as necessary.
//
//  iavc_handle_rx
//      Reads in the command byte and calls the subroutines above.
//
//  iavc_start_tx
//      Initiates DMA on the next queued message if possible.
*/

int iavc_handle_intr(iavc_softc_t *sc)
{
    u_int32_t status;
    u_int32_t newcsr;

    if (!sc->sc_dma) {
	while (iavc_rx_full(sc))
	    iavc_handle_rx(sc);
	return 0;
    }

    status = AMCC_READ(sc, AMCC_INTCSR);
    if ((status & ANY_S5933_INT) == 0)
	return 0;

    newcsr = sc->sc_csr | (status & ALL_INT);
    if (status & TX_TC_INT) newcsr &= ~EN_TX_TC_INT;
    if (status & RX_TC_INT) newcsr &= ~EN_RX_TC_INT;
    AMCC_WRITE(sc, AMCC_INTCSR, newcsr);
    sc->sc_intr = 1;

    if (status & RX_TC_INT) {
	u_int32_t rxlen;

	bus_dmamap_sync(sc->dmat, sc->rx_map, 0, sc->rx_map->dm_mapsize,
	  BUS_DMASYNC_POSTREAD);

	if (sc->sc_recv1 == 0) {
	    sc->sc_recv1 = *(u_int32_t*)(sc->sc_recvbuf);
	    rxlen = (sc->sc_recv1 + 3) & ~3;

	    AMCC_WRITE(sc, AMCC_RXPTR, sc->rx_map->dm_segs[0].ds_addr);
	    AMCC_WRITE(sc, AMCC_RXLEN, rxlen ? rxlen : 4);
	} else {
	    iavc_handle_rx(sc);
	    sc->sc_recv1 = 0;
	    AMCC_WRITE(sc, AMCC_RXPTR, sc->rx_map->dm_segs[0].ds_addr);
	    AMCC_WRITE(sc, AMCC_RXLEN, 4);
	}
    }

    if (status & TX_TC_INT) {
	bus_dmamap_sync(sc->dmat, sc->tx_map, 0, sc->tx_map->dm_mapsize,
	  BUS_DMASYNC_POSTWRITE);
	sc->sc_csr &= ~EN_TX_TC_INT;
	iavc_start_tx(sc);
    }

    AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);
    sc->sc_intr = 0;

    return 0;
}

static void iavc_handle_rx(iavc_softc_t *sc)
{
    u_int8_t *dmabuf = 0, cmd;

    if (sc->sc_dma) {
	dmabuf = amcc_get_byte(sc->sc_recvbuf, &cmd);
    } else {
	cmd = iavc_get_byte(sc);
    }

    NDBGL4(L4_IAVCDBG, "iavc%d: command = 0x%02x", sc->sc_unit, cmd);

    switch (cmd) {
    case RECEIVE_DATA_B3_IND:
	iavc_receive(sc, dmabuf, 1);
	break;

    case RECEIVE_MESSAGE:
	iavc_receive(sc, dmabuf, 0);
	break;

    case RECEIVE_NEW_NCCI:
	iavc_receive_new_ncci(sc, dmabuf);
	break;

    case RECEIVE_FREE_NCCI:
	iavc_receive_free_ncci(sc, dmabuf);
	break;

    case RECEIVE_START:
	iavc_receive_start(sc);
	break;

    case RECEIVE_STOP:
	iavc_receive_stop(sc);
	break;

    case RECEIVE_INIT:
	iavc_receive_init(sc, dmabuf);
	break;

    case RECEIVE_TASK_READY:
	iavc_receive_task_ready(sc, dmabuf);
	break;

    case RECEIVE_DEBUGMSG:
	iavc_receive_debugmsg(sc, dmabuf);
	break;

    default:
	aprint_error_dev(sc->sc_dev, "unknown msg %02x\n", cmd);
    }
}

static void iavc_start_tx(iavc_softc_t *sc)
{
    struct mbuf *m;
    u_int32_t txlen;

    /* If device has put us on hold, punt. */

    if (sc->sc_blocked) {
	return;
    }

    /* If using DMA and transmitter busy, punt. */
    if (sc->sc_dma && (sc->sc_csr & EN_TX_TC_INT)) {
	return;
    }

    /* Else, see if we have messages to send. */
    IF_DEQUEUE(&sc->sc_txq, m);
    if (!m) {
	return;
    }

    /* Have message, will send. */
    if (CAPIMSG_LEN(m->m_data)) {
	/* A proper CAPI message, possibly with B3 data */
	txlen = iavc_tx_capimsg(sc, m);
    } else {
	/* A board control message to be sent as is */
	txlen = iavc_tx_ctrlmsg(sc, m);
    }

    if (m->m_next) {
	i4b_Bfreembuf(m->m_next);
	m->m_next = NULL;
    }
    i4b_Dfreembuf(m);

    /* Kick DMA into motion if applicable */
    if (sc->sc_dma) {
	txlen = (txlen + 3) & ~3;

	bus_dmamap_sync(sc->dmat, sc->tx_map, 0, txlen,
	  BUS_DMASYNC_PREWRITE);

	AMCC_WRITE(sc, AMCC_TXPTR, sc->tx_map->dm_segs[0].ds_addr);
	AMCC_WRITE(sc, AMCC_TXLEN, txlen);
	sc->sc_csr |= EN_TX_TC_INT;

	if (!sc->sc_intr)
	    AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);
    }
}

static uint32_t
iavc_tx_capimsg(iavc_softc_t *sc, struct mbuf *m)
{
    uint32_t txlen = 0;
    u_int8_t *dmabuf;

    if (sc->sc_dma) {
	/* Copy message to DMA buffer. */

	if (m->m_next)
	    dmabuf = amcc_put_byte(sc->sc_sendbuf, SEND_DATA_B3_REQ);
	else
	    dmabuf = amcc_put_byte(sc->sc_sendbuf, SEND_MESSAGE);

	dmabuf = amcc_put_word(dmabuf, m->m_len);
	memcpy(dmabuf, m->m_data, m->m_len);
	dmabuf += m->m_len;
	txlen = 5 + m->m_len;

	if (m->m_next) {
	    dmabuf = amcc_put_word(dmabuf, m->m_next->m_len);
	    memcpy(dmabuf, m->m_next->m_data, m->m_next->m_len);
	    txlen += 4 + m->m_next->m_len;
	}

    } else {
	/* Use PIO. */

	if (m->m_next) {
	    iavc_put_byte(sc, SEND_DATA_B3_REQ);
	    NDBGL4(L4_IAVCDBG, "iavc%d: tx SDB3R msg, len = %d",
	      sc->sc_unit, m->m_len);
	} else {
	    iavc_put_byte(sc, SEND_MESSAGE);
	    NDBGL4(L4_IAVCDBG, "iavc%d: tx SM msg, len = %d",
	      sc->sc_unit, m->m_len);
	}
#if 0
    {
	u_int8_t *p = mtod(m, u_int8_t*);
	int len;
	for (len = 0; len < m->m_len; len++) {
	    printf(" %02x", *p++);
	    if (len && (len % 16) == 0)
		printf("\n");
	}
	if (len % 16)
	    printf("\n");
    }
#endif

	iavc_put_slice(sc, m->m_data, m->m_len);

	if (m->m_next)
	    iavc_put_slice(sc, m->m_next->m_data, m->m_next->m_len);
    }

    return txlen;
}

static uint32_t
iavc_tx_ctrlmsg(iavc_softc_t *sc, struct mbuf *m)
{
    uint32_t txlen = 0;
    uint8_t *dmabuf;

    if (sc->sc_dma) {
	memcpy(sc->sc_sendbuf, m->m_data + 2, m->m_len - 2);
	txlen = m->m_len - 2;
    } else {

#if 0
	{
	u_int8_t *p = mtod(m, u_int8_t*) + 2;
	int len;

	printf("%s: tx BDC msg, len = %d, msg =", device_xname(sc->sc_dev),
	  m->m_len-2);
	for (len = 0; len < m->m_len-2; len++) {
		printf(" %02x", *p++);
		if (len && (len % 16) == 0) printf("\n");
	}
	if (len % 16)
		printf("\n");
	}
#endif

	/* no DMA */
	txlen = m->m_len - 2;
	dmabuf = mtod(m, char*) + 2;
	while(txlen--)
	    b1io_put_byte(sc, *dmabuf++);
    }

    return txlen;
}
