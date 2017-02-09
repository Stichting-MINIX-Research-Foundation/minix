/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: daic.c,v 1.32 2015/08/30 09:46:57 martin Exp $");

/*
 * daic.c: MI driver for Diehl active ISDN cards (S, SX, SXn, SCOM, QUADRO)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <net/if.h>

#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_l3l4.h>
#include <netisdn/i4b_isdnq931.h>
#include <netisdn/i4b_q931.h>
#include <netisdn/i4b_l3fsm.h>
#include <netisdn/i4b_l4.h>

#include <sys/bus.h>
#include <dev/ic/daicvar.h>
#include <dev/ic/daicreg.h>
#include <dev/microcode/daic/dnload.h>

#ifdef NetBSD1_3
#if NetBSD1_3 < 2
/* the device is MI, only the attach struct is in the bus
   dependent frontend. And only on old versions... */
struct cfdriver daic_cd = {
	NULL, "daic", DV_DULL
};
#endif
#endif

/* local function prototypes */
static const char * cardtypename(int cardtype);
static int daic_download(void *, int portcount, struct isdn_dr_prot *data);
static int daic_diagnostic(void *, struct isdn_diagnostic_request *req);
static void daic_connect_request(struct call_desc *cd);
static void daic_connect_response(struct call_desc *cd, int, int);
static void daic_disconnect_request(struct call_desc *cd, int);
static int daic_reset(bus_space_tag_t bus, bus_space_handle_t io, int port, int *memsize);
static int daic_handle_intr(struct daic_softc *sc, int port);
static void daic_register_port(struct daic_softc *sc, int port);
static void daic_request(struct daic_softc *sc, int port, u_int req, u_int id, bus_size_t parmsize, const u_int8_t *parms);
static u_int daic_assign(struct daic_softc *sc, int port, u_int instance, bus_size_t parmsize, const u_int8_t *parms);
static void daic_indicate_ind(struct daic_softc *sc, int port);
static void daic_bch_config(void *, int channel, int bprot, int updown);
static void daic_bch_tx_start(void *, int channel);
static void daic_set_link(void *softc, int channel,
	const struct isdn_l4_driver_functions *l4_driver, void *l4_inst );
static void daic_mgmt_command(struct isdn_l3_driver *drv, int cmd, void *parm);
static void daic_alert_request(struct call_desc *cd);

static isdn_link_t *daic_ret_linktab(void *softc, int channel);

#ifdef DAIC_DEBUG
static void daic_dump_request(struct daic_softc *sc, int port, u_int req, u_int id, bus_size_t parmsize, u_int8_t *parms);
#endif

/* static data */
static const char * const cardnames[] = {
	"S", "SX", "SCOM", "QUADRO"
};

static const char * const err_codes[DAIC_RC_ERRMASK+1] = {
	"NO ERROR",
	"UNKNOWN COMMAND",
	"WRONG COMMAND",
	"WRONG ID",
	"WRONG CH",
	"UNKNOWN IE",
	"WRONG IE",
	"OUT OF RESOURCES"
};

/* fixed parameters */

/* no parameters */
static u_int8_t parm_none[] = { 0 };
#define	VOIDREQ(sc,port,req,id)	daic_request(sc, port, req, id, sizeof parm_none, parm_none)

/* assign request for the global d-channel instance */
static u_int8_t parm_global_assign[] = {
/*	BC	len	cap	rate	A-law	*/
	0x04,	0x03,	0x80,	0x90,	0xa3,	/* 64k speech */
	0x04,	0x02,	0x88,	0x90,		/* 64k data */
	0x04,	0x03,	0x89,	0x90,	0xa3,	/* restricted digital info */
	0x04,	0x03,	0x90,	0x90,	0xa3,	/* 3.1k speech */
/*	shift6	SIN	len	service		*/
	0x96,	0x01,	0x02,	0x00, 0x00,	/* any service */
/*	end of parms */
	0x00
};

/*---------------------------------------------------------------------------*
 *	Return the name of a card with given cardtype
 *---------------------------------------------------------------------------*/
static const char *
cardtypename(int cardtype)
{
	if (cardtype >= 0 && cardtype < (sizeof(cardnames) / sizeof(cardnames[0])))
		return cardnames[cardtype];
	else
		return "unknown type";
}

/*---------------------------------------------------------------------------*
 * Probe for presence of device at given io space.
 * Return the card type (stupid ISA needs to know this in advance, to
 * calculate the share memory size).
 *---------------------------------------------------------------------------*/
int
daic_probe(bus_space_tag_t bus, bus_space_handle_t io)
{
	return (daic_reset(bus, io, 0, NULL));
}

/*---------------------------------------------------------------------------*
 * Attach and initialize the card at given io space.
 *---------------------------------------------------------------------------*/
void
daic_attach(device_t self, struct daic_softc *sc)
{
	int i, num_ports, memsize = 0;

	/* init sc */
	memset(sc->sc_port, 0, sizeof sc->sc_port);
	memset(sc->sc_con, 0, sizeof sc->sc_con);
	sc->sc_cardtype = -1;

	/* init card */
	sc->sc_cardtype = daic_reset(sc->sc_iot, sc->sc_ioh, 0, &memsize);
	if (sc->sc_cardtype == 0) {
		printf(": unknown card, can not attach.\n");
		return;
	}

	printf("\n");
	printf("%s: EICON.Diehl %s\n", device_xname(sc->sc_dev),
	    cardtypename(sc->sc_cardtype));
	printf("%s: %d kByte on board RAM\n", device_xname(sc->sc_dev), memsize);
	num_ports = sc->sc_cardtype == DAIC_TYPE_QUAD ? 4 : 1;
	for (i = 0; i < num_ports; i++)
		sc->sc_port[i].du_state = DAIC_STATE_DOWNLOAD;

	/* register all ports this card has */
	for (i = 0; i < num_ports; i++)
		daic_register_port(sc, i);
}

/*---------------------------------------------------------------------------*
 * handle interrupts for one port of the card
 *---------------------------------------------------------------------------*/
static int
daic_handle_intr(struct daic_softc *sc, int port)
{
	struct outcallentry *assoc;
	struct daic_unit * du = &sc->sc_port[port];
	int off = port * DAIC_ISA_MEMSIZE;
	u_int8_t rc, rcid;
	u_int8_t ind, indid;
	int chan;

	/* check if we caused the interrupt */
	if (!bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_IRQ+off))
		return 0;	/* nope, exit */

	/* is the card in running state yet? */
	if (du->du_state == DAIC_STATE_TESTING) {
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RC+off, 0);
		du->du_state = DAIC_STATE_RUNNING;
		wakeup(du);
		goto done;
	}

	/* what caused the interrupt? */
	/* (1) Check for a return code */
	rc = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RC+off);
	rcid = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RCID+off);
	if (!rc) goto check_ind;

	/* maybe an assign answer (positive or negative) */
	if (rc == DAIC_RC_ASSIGN_OK) {
		du->du_assign_res = rcid;
		/* assing rc is special, we tell the card it's done */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_REQ+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RC+off, 0);
		/* we handle some types of assigns to global dchannel id's automaticaly */
		if (du->du_assign & DAIC_ASSIGN_GLOBAL) {
			du->du_global_dchan = rcid;
			du->du_assign &= ~(DAIC_ASSIGN_GLOBAL|DAIC_ASSIGN_PENDING);
			if (du->du_assign & DAIC_ASSIGN_SLEEPING) {
				du->du_assign = 0;
				wakeup(&du->du_assign_res);
			}
		} else {
			wakeup(&du->du_assign);
		}
		goto check_ind;
	} else if ((rc & DAIC_RC_ASSIGN_MASK) == DAIC_RC_ASSIGN_RC) {
		aprint_error_dev(sc->sc_dev, "assign request failed, error 0x%02x: %s\n",
			rc & DAIC_RC_ERRMASK,
			err_codes[rc & DAIC_RC_ERRMASK]);
		du->du_assign_res = 0;
		/* assing rc is special, we tell the card it's done */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_REQ+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RC+off, 0);
		/* that's it */
		wakeup(&du->du_assign);
		goto check_ind;
	}
	if (rcid == du->du_global_dchan) {
		du->du_request_res = rc;
		wakeup(&du->du_request_res);
		goto req_done;
	}
	for (chan = 0; chan < 2; chan++) {
		if (rcid == sc->sc_con[port*2+chan].dchan_inst) {
			sc->sc_con[port*2+chan].dchan_rc = rc;
			wakeup(&sc->sc_con[port*2+chan].dchan_rc);
			goto req_done;
		} else if (rcid == sc->sc_con[port*2+chan].bchan_inst) {
			sc->sc_con[port*2+chan].bchan_rc = rc;
			wakeup(&sc->sc_con[port*2+chan].bchan_rc);
			goto req_done;
		}
	}
	TAILQ_FOREACH(assoc, &sc->sc_outcalls[port], queue) {
		if (rcid == assoc->dchan_id) {
			assoc->rc = rc;
			wakeup(assoc);
			goto req_done;
		}
	}

	/* not found? */
	printf("%s: unknown id 0x%02x got rc 0x%02x: %s\n",
		device_xname(sc->sc_dev), rcid, rc,
		err_codes[rc & DAIC_RC_ERRMASK]);

req_done:
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RC+off, 0);

check_ind:
	/* (2) Check for an indication */
	ind = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_IND+off);
	indid = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_INDID+off);
	if (!ind) goto done;

	/* incoming call routed to global dchannel task? */
	if (indid == du->du_global_dchan) {
		if (ind == DAIC_IND_INDICATE) {
			daic_indicate_ind(sc, port);
		} else if (ind == DAIC_IND_INFO) {
			int i;

			printf("%s: got info indication\n",
				device_xname(sc->sc_dev));

			for (i = 0; i < 48; i++) {
				if (!(i % 16))
					printf("\n%02x:", i);
				printf(" %02x", bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RBUFFER+off+i));
			}
			printf("\n");
		} else if (ind == DAIC_IND_HANGUP) {
			printf("%s: got global HANGUP indication\n",
				device_xname(sc->sc_dev));
		} else {
			printf("%s: unknown global indication: 0x%02x\n",
				device_xname(sc->sc_dev), ind);
		}
		goto ind_done;
	}

	for (chan = 0; chan < 2; chan++) {
		if (indid == sc->sc_con[port*2+chan].dchan_inst) {
			printf("%s: D-Channel indication 0x%02x for channel %d\n",
				device_xname(sc->sc_dev), ind, chan);
			goto ind_done;
		} else if (indid == sc->sc_con[port*2+chan].bchan_inst) {
			printf("%s: B-Channel indication 0x%02x for channel %d\n",
				device_xname(sc->sc_dev), ind, chan);
			goto ind_done;
		}
	}

	TAILQ_FOREACH(assoc, &sc->sc_outcalls[port], queue) {
		if (indid == assoc->dchan_id) {
			printf("%s: D-Channel indication 0x%02x for outgoing call with cdid %d\n",
				device_xname(sc->sc_dev), ind, assoc->cdid);
			goto ind_done;
		}
	}

	/* not found - something's wrong! */
	printf("%s: got ind 0x%02x for id 0x%02x\n", device_xname(sc->sc_dev), ind, indid);

ind_done:
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_IND+off, 0);

done:
	/* tell card we're ready for more... */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_IRQ+off, 0);

	return 1;
}

/*---------------------------------------------------------------------------*
 * Handle interrupts
 *---------------------------------------------------------------------------*/
int
daic_intr(struct daic_softc *sc)
{
	int handeld = 0;
	if (sc->sc_cardtype == DAIC_TYPE_QUAD) {
		int i;
		for (i = 0; i < 4; i++)
			handeld |= daic_handle_intr(sc, i);
	} else
		handeld = daic_handle_intr(sc, 0);
	return handeld;
}

/*---------------------------------------------------------------------------*
 * Download primary protocol microcode to on-board processor
 *---------------------------------------------------------------------------*/
static int
daic_download(void *token, int count, struct isdn_dr_prot *data)
{
	struct daic_unit *du = token;
	struct daic_softc *sc = du->du_sc;
	int i;

	if (sc->sc_cardtype != DAIC_TYPE_QUAD)
		count = 1;	/* XXX - or signal error ? */

	for (i = 0; i < count; i++) {
		int off = DAIC_ISA_MEMSIZE * i;
		u_int8_t *p = data[i].microcode;
		size_t s = data[i].bytecount;
		u_int32_t sw_id;
		int cnt, x;
		for (p = data[i].microcode+4, cnt = 0; *p && cnt < 70; p++, cnt++)
			;
		sw_id = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
		if (sc->sc_cardtype == DAIC_TYPE_QUAD)
			printf("%s port %d: downloading %s\n",
				device_xname(sc->sc_dev), i, data[i].microcode+4);
		else
			printf("%s: downloading %s\n",
				device_xname(sc->sc_dev), data[i].microcode+4);
		x = splnet();
		p = data[i].microcode;
		while (s > 0) {
		    size_t size = (s > 256) ? 256 : s;
		    bus_space_write_region_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_BUF+off, p, size);
		    p += size;
		    s -= size;
		    bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_CTRL+off, 1);
		    splx(x);
		    for (cnt = 0; cnt < 2*hz; cnt++) {
		    	x = splnet();
		    	if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_CTRL+off) == 0)
		    	    break;
		    	splx(x);
		    	tsleep(sc, 0, "daic download", 1);
		    }
	    	    if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_CTRL+off) != 0) {
	    	    	aprint_error_dev(sc->sc_dev, "download of microcode failed\n");
	    	    	return EIO;
	    	    }
		}

		/* configure microcode - no parameters yet - XXX */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_TEI+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_NT2+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_ZERO+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_WATCHDOG+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_PERMANENT+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_XINTERFACE+off, 0);

		/* start protocol */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_CTRL+off, 2);

		/* wait for signature */
		for (cnt = 0; cnt < 2*hz; cnt++) {
			u_int16_t signature;
			signature = bus_space_read_2(sc->sc_iot, sc->sc_ioh, DAIC_BOOT_SIGNATURE+off);
			if (signature == DAIC_SIGNATURE_VALUE)
				break;
			if (signature) {
				if (signature != DAIC_SIGNATURE_VALUE) {
					splx(x);
					aprint_error_dev(sc->sc_dev, "microcode signature bad: should be %04x, is %04x\n",
						DAIC_SIGNATURE_VALUE,signature);
					return EIO;
				}
				break;
			}
			splx(x);
			tsleep(&sc->sc_port[i].du_state, 0, "daic protocol init", hz/25);
			x = splnet();
		}

		/* real check: send an invalid request and wait for an interrupt */
		sc->sc_port[i].du_state = DAIC_STATE_TESTING;
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RC+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_REQID+off, 0xff);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_REQ+off, 1);
		splx(x);
		tsleep(&sc->sc_port[i].du_state, 0, "daic irq test", 2*hz);
		x = splnet();
		if (sc->sc_port[i].du_state != DAIC_STATE_RUNNING) {
			splx(x);
			printf("%s: download interrupt test timeout\n",
				device_xname(sc->sc_dev));
			return EIO;
		}

		/* finish card configuration */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, DAIC_SWID+off, sw_id);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_SET_CARD+off, sc->sc_cardtype);
		splx(x);

		/* assign global d-channel id for that port */
		sc->sc_port[i].du_global_dchan =
			daic_assign(sc, i, DAIC_GLOBALID_DCHAN,
				sizeof parm_global_assign, parm_global_assign);

		/* send an INDICATE request to get incoming calls on this id */
		x = splnet();
		VOIDREQ(sc, i, DAIC_REQ_INDICATE, sc->sc_port[i].du_global_dchan);
		splx(x);
		tsleep(&sc->sc_port[i].du_request_res, 0, "daic request", 0);
		x = splnet();
		if (sc->sc_port[i].du_request_res != DAIC_RC_OK) {
			aprint_error_dev(sc->sc_dev, "INDICATE request error (0x%02x): %s\n",
				sc->sc_port[i].du_request_res,
				err_codes[sc->sc_port[i].du_request_res & DAIC_RC_ERRMASK]);
			splx(x);
			return EIO;
		}
		splx(x);
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 *	Reset the card, download primary bootstrap, let it check the
 *	card and return the cardtype identified by the microcode
 *	or -1 if no known card is detected.
 *---------------------------------------------------------------------------*/
static int
daic_reset(bus_space_tag_t bus, bus_space_handle_t io, int port, int *memsize)
{
	int i, off = port * DAIC_ISA_MEMSIZE;
	int cardtype, mem, quiet = memsize == NULL;	/* no output if we are only probing */

	/* clear any pending interrupt */
	bus_space_read_1(bus, io, DAIC_IRQ+off);
	/* reset card */
	bus_space_write_1(bus, io, DAIC_BOOT_SET_RESET+off, 0);

	/* download primary bootstrap */
	bus_space_set_region_1(bus, io, DAIC_BOOT_START+off, 0, DAIC_BOOT_CODE-DAIC_BOOT_START);
	bus_space_write_region_1(bus, io, DAIC_BOOT_CODE+off, dnload, DAIC_BOOT_END-DAIC_BOOT_CODE+1);
	if (bus_space_read_1(bus, io, DAIC_BOOT_CTRL+off)
	  || bus_space_read_1(bus, io, DAIC_BOOT_EBIT+off)) {
	  	if (!quiet) printf(": shared memory test failed!\n");
	  	return -1;
	}
	/* let card perform memory test */
	bus_space_write_1(bus, io, DAIC_BOOT_CTRL+off, DAIC_TEST_MEM);
	/* and off we go... */
	bus_space_write_1(bus, io, DAIC_BOOT_CLR_RESET+off, 0);
	/* wait for response from bootstrap */
	for (i = 0; i < 15000 && bus_space_read_1(bus, io, DAIC_BOOT_CTRL+off) != DAIC_TEST_RDY; i++)
		DELAY(100);
	if (i >= 15000) {
		if (!quiet) printf(": on board processor test failed!\n");
		return -1;
	}
	if (bus_space_read_1(bus, io, DAIC_BOOT_EBIT+off)) {
		if (!quiet) printf(": on board memory test failed at %x\n",
			bus_space_read_2(bus, io, DAIC_BOOT_ELOC+off));
		return -1;
	}

	/* fetch info from primary bootstrap code */
	cardtype = bus_space_read_1(bus, io, DAIC_BOOT_CARD+off);
	mem = bus_space_read_1(bus, io, DAIC_BOOT_MSIZE+off) << 4;
	if (memsize)
		*memsize = mem;

	return cardtype;
}

/*---------------------------------------------------------------------------*
 * Generic diagnostic interface - pass through the microcode data
 * without knowing too much about it. This passes a lot work to
 * userland, but hey, this is only a diagnostic tool...
 *---------------------------------------------------------------------------*/
static int
daic_diagnostic(void *token, struct isdn_diagnostic_request *req)
{
	struct daic_unit *du = token;
	struct daic_softc *sc = du->du_sc;
	int port = du->du_port;
	int off = port * DAIC_ISA_MEMSIZE;
	int rc, cnt;
	int s, err = 0;

	/* validate parameters */
	if (req->cmd > DAIC_DIAG_MAXCMD) {
		aprint_error_dev(sc->sc_dev, "daic_diagnostic: illegal cmd %d\n",
			req->cmd);
		return EIO;
	}
	if (req->out_param_len > (DAIC_DIAG_DATA_SIZE+1)) {
		aprint_error_dev(sc->sc_dev, "daic_diagnostic: illegal out_param_len %d\n",
			req->out_param_len);
		return EIO;
	}

	/* XXX - only for debug */
	if (req->cmd == 0x05) {
		/* pass through request from userland */

		u_int8_t id;
		static u_int8_t parms[] = {
			IEI_CALLID, 0x01, 0x81,
			IEI_CALLINGPN, 7, NUMBER_TYPEPLAN, '9', '8', '9',  '0', '2', '0',
			0x96, 0x01, 0x02, 0x01, 0x00,
			0x00
		};

		/* create the d-channel task for this call */
		printf("%s: assigning id for pass-through call\n", device_xname(sc->sc_dev));
		id = daic_assign(sc, port, DAIC_GLOBALID_DCHAN, sizeof(parms), parms);
		printf("%s: got id 0x%02x\n", device_xname(sc->sc_dev), id);

#ifdef DAIC_DEBUG
		daic_dump_request(sc, port, DAIC_REQ_CALL, id, req->in_param_len, req->in_param);
#endif
		daic_request(sc, port, DAIC_REQ_CALL, id, req->in_param_len, req->in_param);
		return 0;
	}

	/* all these need an output parameter */
	if (req->out_param == NULL)
		return EIO;

	/* check state and switch to DIAGNOSTIC */
	s = splnet();
	if (sc->sc_port[port].du_state != DAIC_STATE_RUNNING) {
		splx(s);
		return EWOULDBLOCK;
	}
	sc->sc_port[port].du_state = DAIC_STATE_DIAGNOSTIC;
	splx(s);

	/* set new request */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_DIAG_REQ+off, req->cmd);

	/* sorry, no interrupt on completition - have to poll */
	for (cnt = 0; cnt < 3*hz; cnt++) {
		if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_DIAG_REQ+off) == 0
		   && bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_DIAG_RC+off) != 0)
			break;
		tsleep(sc, 0, "daic diagnostic", 1);
	}
	rc = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_DIAG_RC+off);
	if (rc == 0) {
		/* stop request and return error */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_DIAG_REQ+off, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_DIAG_RC+off, 0);
		err = EIO;
		goto done;
	}
	/* out param gets rc and all the data */
	if (req->out_param_len >= 2) {
		((u_int8_t*)(req->out_param))[0] = (u_int8_t)rc;
		bus_space_read_region_1(sc->sc_iot, sc->sc_ioh, DAIC_DIAG_DATA+off, ((u_int8_t*)req->out_param)+1, req->out_param_len-1);
	}
	/* acknowledge data */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_DIAG_RC+off, 0);

done:	/* back to normal state */
	s = splnet();
	sc->sc_port[port].du_state = DAIC_STATE_RUNNING;
	splx(s);

	return err;
}

static void daic_stat(void *port, int channel, bchan_statistics_t *bsp)
{
}

static const struct isdn_l4_bchannel_functions
daic_l4_driver = {
	daic_bch_config,
	daic_bch_tx_start,
	daic_stat
};

static const struct isdn_l3_driver_functions
daic_l3_functions =  {
	daic_ret_linktab,
	daic_set_link,
	daic_connect_request,
	daic_connect_response,
	daic_disconnect_request,
	daic_alert_request,
	daic_download,
	daic_diagnostic,
	daic_mgmt_command
};

/*---------------------------------------------------------------------------*
 *	Register one port and attach it to the upper layers
 *---------------------------------------------------------------------------*/
static void
daic_register_port(struct daic_softc *sc, int port)
{
	int chan;
	char cardname[80], devname[80];
	struct isdn_l3_driver * l3drv;

	sc->sc_port[port].du_port = port;
	sc->sc_port[port].du_sc = sc;

	/* make sure this hardware driver type is known to layer 4 */
	if (sc->sc_cardtype == DAIC_TYPE_QUAD)
		snprintf(devname, sizeof(devname), "%s port %d",
		    device_xname(sc->sc_dev), port);
	else
		strlcpy(devname, device_xname(sc->sc_dev), sizeof(devname));
	snprintf(cardname, sizeof(cardname), "EICON.Diehl %s",
	    cardtypename(sc->sc_cardtype));
	l3drv = isdn_attach_isdnif(
	    devname, cardname, &sc->sc_port[port], &daic_l3_functions,
	    NBCH_BRI);
	sc->sc_port[port].du_l3 = l3drv;

	/* initialize linktabs for this port */
	for (chan = 0; chan < 2; chan++) {
		isdn_link_t *lt = &sc->sc_con[port*2+chan].isdn_linktab;
		lt->l1token = &sc->sc_port[port];
		lt->channel = chan;
		lt->tx_queue = &sc->sc_con[port*2+chan].tx_queue;
		lt->rx_queue = &sc->sc_con[port*2+chan].rx_queue;
	}
	TAILQ_INIT(&sc->sc_outcalls[port]);

	isdn_isdnif_ready(l3drv->isdnif);
}

/*---------------------------------------------------------------------------*
 *	return the address of daic drivers linktab
 *---------------------------------------------------------------------------*/
static isdn_link_t *
daic_ret_linktab(void *token, int channel)
{
	struct daic_unit *du = token;
	struct daic_softc *sc = du->du_sc;
	int port = du->du_port;
	struct daic_connection *con = &sc->sc_con[port*2+channel];

	return(&con->isdn_linktab);
}

/*---------------------------------------------------------------------------*
 *	set the driver linktab in the b channel softc
 *---------------------------------------------------------------------------*/
static void
daic_set_link(void *token, int channel, const struct isdn_l4_driver_functions *l4_driver, void *l4_inst)
{
	struct daic_unit *du = token;
	struct daic_softc *sc = du->du_sc;
	int port = du->du_port;
	struct daic_connection *con = &sc->sc_con[port*2+channel];

	con->l4_driver = l4_driver;
	con->l4_driver_softc = l4_inst;
}

/*---------------------------------------------------------------------------*
 *	Send a request to the card.
 *---------------------------------------------------------------------------*/
static void
daic_request(
	struct daic_softc *sc,		/* ourself */
	int port, 			/* and the port on this card */
	u_int req, 			/* the request to send */
	u_int id, 			/* id of communication task */
	bus_size_t parmsize, 		/* size of parms including the terminating zero */
	const u_int8_t *parms)		/* pointer to parms to pass */
{
	int off = port*DAIC_ISA_MEMSIZE;

	/* spin while card is yet busy */
	while (bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_REQ))
		;	/* unlikely to happen with this driver */

	/* output parameters */
	bus_space_write_region_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_XBUFFER+off, parms, parmsize);

	/* output request and id */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_REQID+off, id);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_REQ+off, req);
}

/*---------------------------------------------------------------------------*
 *	Assign a unique instance id for some communication class
 *	on the card. Only one assign request may be running on a
 *	port at any time, handle this and return the instance id.
 *---------------------------------------------------------------------------*/
static u_int
daic_assign(
	struct daic_softc *sc,	/* our state and port no */
	int port,
	u_int classid,		/* Diehl calls this "global instance id" */
	bus_size_t parmsize, 	/* sizeof parameter arra */
	const u_int8_t *parms)	/* task instance parameters */
{
	static char wchan[] = "daic assign";
	u_int8_t id;
	int x;

	/* there only may be one assignment running concurrently */
		x = splnet();
	for (;;) {
		if (!(sc->sc_port[port].du_assign & DAIC_ASSIGN_PENDING))
			break;	/* we got it! */

		/* somebody else is assigning, record state and sleep */
		sc->sc_port[port].du_assign |= DAIC_ASSIGN_SLEEPING;
		tsleep(&sc->sc_port[port].du_assign_res, 0, wchan, 0);
	}

	/* put parameters and request to card */
	sc->sc_port[port].du_assign |= DAIC_ASSIGN_PENDING;
	daic_request(sc, port, DAIC_REQ_ASSIGN, classid, parmsize, parms);

	/* wait for completition of assignment by the card */
	tsleep(&sc->sc_port[port].du_assign, 0, wchan, 0);
	id = sc->sc_port[port].du_assign_res;

	/* have we lost our global dchannel id in the meantime? */
	if (sc->sc_port[port].du_assign & DAIC_ASSIGN_NOGLOBAL) {
		/* start an assign request and let the result
		   be handled by the interrupt handler - we don't
		   have to wait for it here. As the assign lock
		   isn't freed, we don't wake up others... */
		sc->sc_port[port].du_assign &= ~DAIC_ASSIGN_NOGLOBAL;
		sc->sc_port[port].du_assign |= DAIC_ASSIGN_PENDING|DAIC_ASSIGN_GLOBAL;
		daic_request(sc, port, DAIC_REQ_ASSIGN, DAIC_GLOBALID_DCHAN,
			sizeof parm_global_assign, parm_global_assign);
		splx(x);
		return id;
	}

	/* XXX - review this, can't remember why I did it this complicated */

	/* unlock and wakup others, if any */
	if (sc->sc_port[port].du_assign & DAIC_ASSIGN_SLEEPING) {
		sc->sc_port[port].du_assign = 0;
		wakeup(&sc->sc_port[port].du_assign_res);
	} else
		sc->sc_port[port].du_assign = 0;
	splx(x);

	return id;
}

#ifdef DAIC_DEBUG
/*---------------------------------------------------------------------------*
 *	Debug output of request parameters
 *---------------------------------------------------------------------------*/
static void
daic_dump_request(struct daic_softc *sc, int port, u_int req, u_int id, bus_size_t parmsize, u_int8_t *parms)
{
	int i;
	printf("%s: request 0x%02x to task id 0x%02x:",
		device_xname(sc->sc_dev), req, id);
	for (i = 0; i < parmsize; i++) {
		if (i % 16 == 0)
			printf("\n%02x:", i);
		printf(" %02x", parms[i]);
	}
	printf("\n");
}
#endif

/*---------------------------------------------------------------------------*
 *	Decode parameters of an INDICATE indication from the card
 *	and pass them to layer 4. Called from within an interrupt
 *	context.
 *---------------------------------------------------------------------------*/
static void
daic_indicate_ind(struct daic_softc *sc, int port)
{
	int offset = port*DAIC_ISA_MEMSIZE;
	int i;
	u_int8_t ie, ielen;
	call_desc_t *cd;

	/* get and init new calldescriptor */
	cd = reserve_cd();	/* cdid filled in */
	cd->bprot = BPROT_NONE;
	cd->cause_in = 0;
	cd->cause_out = 0;
	cd->dst_telno[0] = '\0';
	cd->src_telno[0] = '\0';
	cd->channelid = CHAN_NO;
	cd->channelexcl = 0;
	cd->cr = -1;
	cd->crflag = CRF_DEST;
	cd->ilt = NULL;		/* reset link tab ptrs */

	i = 0;
	for (;;) {
		ie = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RBUFFER+offset+i);
		if (!ie) break;
		i++;
		if (ie & 0x80) continue;
		ielen = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RBUFFER+offset+i);
		i++;
		switch (ie) {
		  case IEI_BEARERCAP:
		  	ie = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RBUFFER+offset+i);
		  	if (ie == 0x80 || ie == 0x89 || ie == 0x90)
		  		cd->bprot = BPROT_NONE;
		  	else if (ie == 0x88)
		  		cd->bprot = BPROT_RHDLC;
		  	break;
		  case IEI_CALLINGPN:
		  	{
		  		int off;
			  	ie = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RBUFFER+offset+i);
			  	if (ie & 0x80)
			  		off = 1;
			  	else
			  		off = 2;
			  	bus_space_read_region_1(sc->sc_iot, sc->sc_ioh,
					DAIC_COM_RBUFFER+offset+i+off, cd->src_telno,
					ielen - off);
				cd->src_telno[ielen-off+1] = '\0';
			}
		  	break;
		  case IEI_CALLEDPN:
		  	bus_space_read_region_1(sc->sc_iot, sc->sc_ioh,
		  		DAIC_COM_RBUFFER+offset+i+1,
				cd->dst_telno, ielen-1);
			cd->dst_telno[ielen] = '\0';
		  	break;
		  case IEI_CHANNELID:
		  	ie = bus_space_read_1(sc->sc_iot, sc->sc_ioh, DAIC_COM_RBUFFER+offset+i);
		  	if ((ie & 0xf4) != 0x80)
		  		cd->channelid = CHAN_NO;
		  	else {
		  		switch(ie & 0x03) {
		  		  case IE_CHAN_ID_NO:	cd->channelid = CHAN_NO; break;
		  		  case IE_CHAN_ID_B1:	cd->channelid = CHAN_B1; break;
		  		  case IE_CHAN_ID_B2:	cd->channelid = CHAN_B2; break;
		  		  case IE_CHAN_ID_ANY:	cd->channelid = CHAN_ANY; break;
		  		}
		  		cd->channelexcl = (ie & 0x08) >> 3;
		  	}
		}
		i += ielen;
	}
	cd->event = EV_SETUP;
	/* ctrl_desc[cd->controller].bch_state[cd->channelid] = BCH_ST_RSVD; */

	/* record the dchannel id for this call and the call descriptor */
	sc->sc_con[port*2+cd->channelid].dchan_inst = sc->sc_port[port].du_global_dchan;
	sc->sc_con[port*2+cd->channelid].cdid = cd->cdid;

	/* this task is busy now, we need a new global dchan id */
	if (sc->sc_port[port].du_assign & DAIC_ASSIGN_PENDING) {
		/* argh - can't assign right now */
		sc->sc_port[port].du_assign |= DAIC_ASSIGN_NOGLOBAL;
	} else {
		/* yeah - can request the assign right away, but let the
		   interrupt handler autohandle the result */
		sc->sc_port[port].du_assign |= DAIC_ASSIGN_PENDING|DAIC_ASSIGN_GLOBAL;
		daic_request(sc, port, DAIC_REQ_ASSIGN, DAIC_GLOBALID_DCHAN,
			sizeof parm_global_assign, parm_global_assign);
	}

	if (cd->bprot == BPROT_NONE)
		printf("\nincoming voice call from \"%s\" to \"%s\"\n",
			cd->src_telno, cd->dst_telno);
	else
		printf("\nincoming data call from \"%s\" to \"%s\"\n",
			cd->src_telno, cd->dst_telno);

	/* hand up call to layer 4 */
	i4b_l4_connect_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	Layer 4 request a call setup
 *---------------------------------------------------------------------------*/
static void
daic_connect_request(struct call_desc *cd)
{
	u_int8_t id, cpn[TELNO_MAX+4], parms[TELNO_MAX+16], *p;
	struct daic_unit *du = cd->ilt->l1token;
	struct daic_softc *sc = du->du_sc;
	int port = du->du_port;
	int x, len;
	struct outcallentry *assoc;

	/* to associate the cdid with the communication task
	   we are going to create for this outgoing call,
	   we maintain a queue of pending outgoing calls.
	   As soon as a SETUP response is received, we move
	   the association to the allocated b-channel. */

	/* configure d-channel task parameters */
	p = parms;
	*p++ = IEI_CALLID; *p++ = 0x01;
	if (cd->bprot == BPROT_NONE) {
		*p++ = 0x82;
	} else if (cd->bprot == BPROT_RHDLC) {
		*p++ = 0x85;
	} else {
		printf("%s: daic_connect_request for unknown bchan protocol 0x%x\n",
			device_xname(sc->sc_dev), cd->bprot);
		return;
	}
	if (cd->src_telno[0]) {
		*p++ = IEI_CALLINGPN;
		*p++ = strlen(cd->src_telno)+1;
		*p++ = NUMBER_TYPEPLAN;
		strcpy(p, cd->src_telno);
		p += strlen(p);
	}
	if (cd->channelid == CHAN_B1 || cd->channelid == CHAN_B2) {
		*p++ = IEI_CHANNELID;
		*p++ = 0x01;
		*p++ = 0x81 + cd->channelid;
	}
	if (cd->bprot == BPROT_NONE) {
		*p++ = 0x96;	/* shift6 */
		*p++ = 0x01;	/* SIN */
		*p++ = 0x02;	/* len */
		*p++ = 0x01;	/* Telephony */
		*p++ = 0x00;	/* add.info */
	}
	*p++ = 0;

	/* create the d-channel task for this call */
	id = daic_assign(sc, port, DAIC_GLOBALID_DCHAN, p - parms, parms);

	/* map it to the call descriptor id */
	assoc = malloc(sizeof(struct outcallentry), M_DEVBUF, 0);
	assoc->cdid = cd->cdid;
	assoc->dchan_id = id;
	x = splnet();
	TAILQ_INSERT_TAIL(&sc->sc_outcalls[port], assoc, queue);

	/* send a call request */
	len = strlen(cd->dst_telno);
	cpn[0] = IEI_CALLEDPN;
	cpn[1] = len+1;
	cpn[2] = NUMBER_TYPEPLAN;
	strcpy(cpn+3, cd->dst_telno);
#ifdef DAIC_DEBUG
	daic_dump_request(sc, port, DAIC_REQ_CALL, id, len+4, cpn);
#endif
	daic_request(sc, port, DAIC_REQ_CALL, id, len+4, cpn);
	splx(x);
	tsleep(assoc, 0, "daic call", 0);
	if (assoc->rc != DAIC_RC_OK) {
		aprint_error_dev(sc->sc_dev, "call request failed, error 0x%02x: %s\n",
			assoc->rc & DAIC_RC_ERRMASK,
			err_codes[assoc->rc & DAIC_RC_ERRMASK]);
	}
}

/*---------------------------------------------------------------------------*
 *	TODO:
 *---------------------------------------------------------------------------*/
static void daic_connect_response(struct call_desc *cd, int response, int cause)
{
}

/*---------------------------------------------------------------------------*
 *	TODO:
 *---------------------------------------------------------------------------*/
static void daic_disconnect_request(struct call_desc *cd, int cause)
{
}

/*---------------------------------------------------------------------------*
 *	TODO:
 *---------------------------------------------------------------------------*/
static void daic_bch_config(void *token, int channel, int bprot, int updown)
{
	printf("daic: bch_config\n");
}

/*---------------------------------------------------------------------------*
 *	TODO:
 *---------------------------------------------------------------------------*/
static void daic_bch_tx_start(void *token, int channel)
{
	printf("daic: bch_tx_start\n");
}

/*---------------------------------------------------------------------------*
 *	TODO:
 *---------------------------------------------------------------------------*/
static void
daic_mgmt_command(struct isdn_l3_driver *drv, int cmd, void *parm)
{
}

/*---------------------------------------------------------------------------*
 *	TODO:
 *---------------------------------------------------------------------------*/
static void
daic_alert_request(struct call_desc *cd)
{
}

