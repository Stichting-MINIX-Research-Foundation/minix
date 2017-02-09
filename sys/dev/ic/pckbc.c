/* $NetBSD: pckbc.c,v 1.58 2015/04/13 16:33:24 riastradh Exp $ */

/*
 * Copyright (c) 2004 Ben Harris.
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pckbc.c,v 1.58 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/queue.h>

#include <sys/bus.h>

#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>

#include <dev/pckbport/pckbportvar.h>

#include "locators.h"

#include <sys/rndsource.h>

/* data per slave device */
struct pckbc_slotdata {
	int polling;	/* don't process data in interrupt handler */
	int poll_data;	/* data read from inr handler if polling */
	int poll_stat;	/* status read from inr handler if polling */
	krndsource_t	rnd_source;
};

static void pckbc_init_slotdata(struct pckbc_slotdata *);
static int pckbc_attach_slot(struct pckbc_softc *, pckbc_slot_t);

struct pckbc_internal pckbc_consdata;
int pckbc_console_attached;

static int pckbc_console;
static struct pckbc_slotdata pckbc_cons_slotdata;

static int pckbc_xt_translation(void *, pckbport_slot_t, int);
static int pckbc_send_devcmd(void *, pckbport_slot_t, u_char);
static void pckbc_slot_enable(void *, pckbport_slot_t, int);
static void pckbc_intr_establish(void *, pckbport_slot_t);
static void pckbc_set_poll(void *,	pckbc_slot_t, int on);

static int pckbc_wait_output(bus_space_tag_t, bus_space_handle_t);

static int pckbc_get8042cmd(struct pckbc_internal *);
static int pckbc_put8042cmd(struct pckbc_internal *);

void pckbc_cleanqueue(struct pckbc_slotdata *);
void pckbc_cleanup(void *);
int pckbc_cmdresponse(struct pckbc_internal *, pckbc_slot_t, u_char);
void pckbc_start(struct pckbc_internal *, pckbc_slot_t);

const char * const pckbc_slot_names[] = { "kbd", "aux" };

static struct pckbport_accessops const pckbc_ops = {
	pckbc_xt_translation,
	pckbc_send_devcmd,
	pckbc_poll_data1,
	pckbc_slot_enable,
	pckbc_intr_establish,
	pckbc_set_poll
};

#define	KBD_DELAY	DELAY(8)

static inline int
pckbc_wait_output(bus_space_tag_t iot, bus_space_handle_t ioh_c)
{
	u_int i;

	for (i = 100000; i; i--)
		if (!(bus_space_read_1(iot, ioh_c, 0) & KBS_IBF)) {
			KBD_DELAY;
			return (1);
		}
	return (0);
}

int
pckbc_send_cmd(bus_space_tag_t iot, bus_space_handle_t ioh_c, u_char val)
{
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_c, 0, val);
	return (1);
}

/*
 * Note: the spl games here are to deal with some strange PC kbd controllers
 * in some system configurations.
 * This is not canonical way to handle polling input.
 */
int
pckbc_poll_data1(void *pt, pckbc_slot_t slot)
{
	struct pckbc_internal *t = pt;
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	int s;
	u_char stat, c;
	int i = 100; /* polls for ~100ms */
	int checkaux = t->t_haveaux;

	s = splhigh();

	if (q && q->polling && q->poll_data != -1 && q->poll_stat != -1) {
		stat	= q->poll_stat;
		c	= q->poll_data;
		q->poll_data = -1;
		q->poll_stat = -1;
		goto process;
	}

	for (; i; i--, delay(1000)) {
		stat = bus_space_read_1(t->t_iot, t->t_ioh_c, 0);
		if (stat & KBS_DIB) {
			KBD_DELAY;
			c = bus_space_read_1(t->t_iot, t->t_ioh_d, 0);

		    process:
			if (checkaux && (stat & 0x20)) { /* aux data */
				if (slot != PCKBC_AUX_SLOT) {
#ifdef PCKBCDEBUG
					printf("pckbc: lost aux 0x%x\n", c);
#endif
					continue;
				}
			} else {
				if (slot == PCKBC_AUX_SLOT) {
#ifdef PCKBCDEBUG
					printf("pckbc: lost kbd 0x%x\n", c);
#endif
					continue;
				}
			}
			splx(s);
			return (c);
		}
	}

	splx(s);
	return (-1);
}

/*
 * Get the current command byte.
 */
static int
pckbc_get8042cmd(struct pckbc_internal *t)
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_c = t->t_ioh_c;
	int data;

	if (!pckbc_send_cmd(iot, ioh_c, K_RDCMDBYTE))
		return (0);
	data = pckbc_poll_data1(t, PCKBC_KBD_SLOT);
	if (data == -1)
		return (0);
	t->t_cmdbyte = data;
	return (1);
}

/*
 * Pass command byte to keyboard controller (8042).
 */
static int
pckbc_put8042cmd(struct pckbc_internal *t)
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;

	if (!pckbc_send_cmd(iot, ioh_c, K_LDCMDBYTE))
		return (0);
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_d, 0, t->t_cmdbyte);
	return (1);
}

static int
pckbc_send_devcmd(void *pt, pckbc_slot_t slot, u_char val)
{
	struct pckbc_internal *t = pt;
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;

	if (slot == PCKBC_AUX_SLOT) {
		if (!pckbc_send_cmd(iot, ioh_c, KBC_AUXWRITE))
			return (0);
	}
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_d, 0, val);
	return (1);
}

int
pckbc_is_console(bus_space_tag_t iot, bus_addr_t addr)
{
	if (pckbc_console && !pckbc_console_attached &&
	    bus_space_is_equal(pckbc_consdata.t_iot, iot) &&
	    pckbc_consdata.t_addr == addr)
		return (1);
	return (0);
}

static int
pckbc_attach_slot(struct pckbc_softc *sc, pckbc_slot_t slot)
{
	struct pckbc_internal *t = sc->id;
	void *sdata;
	device_t child;
	int alloced = 0;

	if (t->t_slotdata[slot] == NULL) {
		sdata = malloc(sizeof(struct pckbc_slotdata),
		    M_DEVBUF, M_NOWAIT);
		if (sdata == NULL) {
			aprint_error_dev(sc->sc_dv, "no memory\n");
			return (0);
		}
		t->t_slotdata[slot] = sdata;
		pckbc_init_slotdata(t->t_slotdata[slot]);
		alloced++;
	}

	child = pckbport_attach_slot(sc->sc_dv, t->t_pt, slot);

	if (child == NULL && alloced) {
		free(t->t_slotdata[slot], M_DEVBUF);
		t->t_slotdata[slot] = NULL;
	}

	if (child != NULL && t->t_slotdata[slot] != NULL)
		rnd_attach_source(&t->t_slotdata[slot]->rnd_source,
		    device_xname(child), RND_TYPE_TTY, RND_FLAG_DEFAULT);

	return child != NULL;
}

void
pckbc_attach(struct pckbc_softc *sc)
{
	struct pckbc_internal *t;
	bus_space_tag_t iot;
	bus_space_handle_t ioh_d, ioh_c;
	int res;
	u_char cmdbits = 0;

	t = sc->id;
	iot = t->t_iot;
	ioh_d = t->t_ioh_d;
	ioh_c = t->t_ioh_c;

	t->t_pt = pckbport_attach(t, &pckbc_ops);
	if (t->t_pt == NULL) {
		aprint_error(": attach failed\n");
		return;
	}

	/* flush */
	(void) pckbc_poll_data1(t, PCKBC_KBD_SLOT);

	/* set initial cmd byte */
	if (!pckbc_put8042cmd(t)) {
		printf("pckbc: cmd word write error\n");
		return;
	}

/*
 * XXX Don't check the keyboard port. There are broken keyboard controllers
 * which don't pass the test but work normally otherwise.
 */
#if 0
	/*
	 * check kbd port ok
	 */
	if (!pckbc_send_cmd(iot, ioh_c, KBC_KBDTEST))
		return;
	res = pckbc_poll_data1(t, PCKBC_KBD_SLOT, 0);

	/*
	 * Normally, we should get a "0" here.
	 * But there are keyboard controllers behaving differently.
	 */
	if (res == 0 || res == 0xfa || res == 0x01 || res == 0xab) {
#ifdef PCKBCDEBUG
		if (res != 0)
			printf("pckbc: returned %x on kbd slot test\n", res);
#endif
		if (pckbc_attach_slot(sc, PCKBC_KBD_SLOT))
			cmdbits |= KC8_KENABLE;
	} else {
		printf("pckbc: kbd port test: %x\n", res);
		return;
	}
#else
	if (pckbc_attach_slot(sc, PCKBC_KBD_SLOT))
		cmdbits |= KC8_KENABLE;
#endif /* 0 */

	/*
	 * Check aux port ok.
	 * Avoid KBC_AUXTEST because it hangs some older controllers
	 *  (eg UMC880?).
	 */
	if (!pckbc_send_cmd(iot, ioh_c, KBC_AUXECHO)) {
		printf("pckbc: aux echo error 1\n");
		goto nomouse;
	}
	if (!pckbc_wait_output(iot, ioh_c)) {
		printf("pckbc: aux echo error 2\n");
		goto nomouse;
	}
	t->t_haveaux = 1;
	bus_space_write_1(iot, ioh_d, 0, 0x5a); /* a random value */
	res = pckbc_poll_data1(t, PCKBC_AUX_SLOT);

	/*
	 * The following is needed to find the aux port on the Tadpole
	 * SPARCle.
	 */
	if (res == -1 && ISSET(t->t_flags, PCKBC_NEED_AUXWRITE)) {
		/* Read of aux echo timed out, try again */
		if (!pckbc_send_cmd(iot, ioh_c, KBC_AUXWRITE))
			goto nomouse;
		if (!pckbc_wait_output(iot, ioh_c))
			goto nomouse;
		bus_space_write_1(iot, ioh_d, 0, 0x5a);
		res = pckbc_poll_data1(t, PCKBC_AUX_SLOT);
	}
	if (res != -1) {
		/*
		 * In most cases, the 0x5a gets echoed.
		 * Some older controllers (Gateway 2000 circa 1993)
		 * return 0xfe here.
		 * We are satisfied if there is anything in the
		 * aux output buffer.
		 */
		if (pckbc_attach_slot(sc, PCKBC_AUX_SLOT))
			cmdbits |= KC8_MENABLE;
	} else {

#ifdef PCKBCDEBUG
		printf("pckbc: aux echo test failed\n");
#endif
		t->t_haveaux = 0;
	}

nomouse:
	/* enable needed interrupts */
	t->t_cmdbyte |= cmdbits;
	if (!pckbc_put8042cmd(t))
		printf("pckbc: cmd word write error\n");
}

static void
pckbc_init_slotdata(struct pckbc_slotdata *q)
{

	q->polling = 0;
}

/*
 * switch scancode translation on / off
 * return nonzero on success
 */
static int
pckbc_xt_translation(void *self, pckbc_slot_t slot, int on)
{
	struct pckbc_internal *t = self;
	int ison;

	if (ISSET(t->t_flags, PCKBC_CANT_TRANSLATE))
		return (-1);

	if (slot != PCKBC_KBD_SLOT) {
		/* translation only for kbd slot */
		if (on)
			return (0);
		else
			return (1);
	}

	ison = t->t_cmdbyte & KC8_TRANS;
	if ((on && ison) || (!on && !ison))
		return (1);

	t->t_cmdbyte ^= KC8_TRANS;
	if (!pckbc_put8042cmd(t))
		return (0);

	/* read back to be sure */
	if (!pckbc_get8042cmd(t))
		return (0);

	ison = t->t_cmdbyte & KC8_TRANS;
	if ((on && ison) || (!on && !ison))
		return (1);
	return (0);
}

static const struct pckbc_portcmd {
	u_char cmd_en, cmd_dis;
} pckbc_portcmd[2] = {
	{
		KBC_KBDENABLE, KBC_KBDDISABLE,
	}, {
		KBC_AUXENABLE, KBC_AUXDISABLE,
	}
};

void
pckbc_slot_enable(void *self, pckbc_slot_t slot, int on)
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;
	const struct pckbc_portcmd *cmd;

	cmd = &pckbc_portcmd[slot];

	if (!pckbc_send_cmd(t->t_iot, t->t_ioh_c,
			    on ? cmd->cmd_en : cmd->cmd_dis))
		printf("pckbc: pckbc_slot_enable(%d) failed\n", on);
}

static void
pckbc_set_poll(void *self, pckbc_slot_t slot, int on)
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;

	t->t_slotdata[slot]->polling = on;

	if (on) {
		t->t_slotdata[slot]->poll_data = -1;
		t->t_slotdata[slot]->poll_stat = -1;
	} else {
		int s;

		/*
		 * If disabling polling on a device that's been configured,
		 * make sure there are no bytes left in the FIFO, holding up
		 * the interrupt line.  Otherwise we won't get any further
		 * interrupts.
		 */
		if (t->t_sc) {
			s = spltty();
			pckbcintr(t->t_sc);
			splx(s);
		}
	}
}

static void
pckbc_intr_establish(void *pt, pckbport_slot_t slot)
{
	struct pckbc_internal *t = pt;

	(*t->t_sc->intr_establish)(t->t_sc, slot);
}

int
pckbcintr_hard(void *vsc)
{
	struct pckbc_softc *sc = (struct pckbc_softc *)vsc;
	struct pckbc_internal *t = sc->id;
	u_char stat;
	pckbc_slot_t slot;
	struct pckbc_slotdata *q;
	int served = 0, data, next, s;

	for(;;) {
		stat = bus_space_read_1(t->t_iot, t->t_ioh_c, 0);
		if (!(stat & KBS_DIB))
			break;

		served = 1;

		slot = (t->t_haveaux && (stat & 0x20)) ?
		    PCKBC_AUX_SLOT : PCKBC_KBD_SLOT;
		q = t->t_slotdata[slot];

		if (!q) {
			/* XXX do something for live insertion? */
			printf("pckbc: no dev for slot %d\n", slot);
			KBD_DELAY;
			(void) bus_space_read_1(t->t_iot, t->t_ioh_d, 0);
			continue;
		}

		KBD_DELAY;
		data = bus_space_read_1(t->t_iot, t->t_ioh_d, 0);

		rnd_add_uint32(&q->rnd_source, (stat<<8)|data);

		if (q->polling) {
			q->poll_data = data;
			q->poll_stat = stat;
			break; /* pckbc_poll_data() will get it */
		}

#if 0 /* XXXBJH */
		if (CMD_IN_QUEUE(q) && pckbc_cmdresponse(t, slot, data))
			continue;
#endif

		s = splhigh();
		next = (t->rbuf_write+1) % PCKBC_RBUF_SIZE;
		if (next == t->rbuf_read) {
			splx(s);
			break;
		}
		t->rbuf[t->rbuf_write].data = data;
		t->rbuf[t->rbuf_write].slot = slot;
		t->rbuf_write = next;
		splx(s);
	}

	return (served);
}

void
pckbcintr_soft(void *vsc)
{
	struct pckbc_softc *sc = vsc;
	struct pckbc_internal *t = sc->id;
	int data, slot, s;
#ifndef __GENERIC_SOFT_INTERRUPTS_ALL_LEVELS
	int st;

	st = spltty();
#endif

	s = splhigh();
	while (t->rbuf_read != t->rbuf_write) {
		slot = t->rbuf[t->rbuf_read].slot;
		data = t->rbuf[t->rbuf_read].data;
		t->rbuf_read = (t->rbuf_read+1) % PCKBC_RBUF_SIZE;
		splx(s);
		pckbportintr(t->t_pt, slot, data);
		s = splhigh();
	}
	splx(s);


#ifndef __GENERIC_SOFT_INTERRUPTS_ALL_LEVELS
	splx(st);
#endif
}

int
pckbcintr(void *vsc)
{
	struct pckbc_softc *sc = (struct pckbc_softc *)vsc;
	struct pckbc_internal *t = sc->id;
	u_char stat;
	pckbc_slot_t slot;
	struct pckbc_slotdata *q;
	int served = 0, data;

	for(;;) {
		stat = bus_space_read_1(t->t_iot, t->t_ioh_c, 0);
		if (!(stat & KBS_DIB))
			break;

		slot = (t->t_haveaux && (stat & 0x20)) ?
		    PCKBC_AUX_SLOT : PCKBC_KBD_SLOT;
		q = t->t_slotdata[slot];

		if (q != NULL && q->polling)
			return 0;

		served = 1;
		KBD_DELAY;
		data = bus_space_read_1(t->t_iot, t->t_ioh_d, 0);

		rnd_add_uint32(&q->rnd_source, (stat<<8)|data);

		pckbportintr(t->t_pt, slot, data);
	}

	return (served);
}

int
pckbc_cnattach(bus_space_tag_t iot, bus_addr_t addr,
	bus_size_t cmd_offset, pckbc_slot_t slot, int flags)
{
	bus_space_handle_t ioh_d, ioh_c;
#ifdef PCKBC_CNATTACH_SELFTEST
	int reply;
#endif
	int res = 0;

	if (bus_space_map(iot, addr + KBDATAP, 1, 0, &ioh_d))
		return (ENXIO);
	if (bus_space_map(iot, addr + cmd_offset, 1, 0, &ioh_c)) {
		bus_space_unmap(iot, ioh_d, 1);
		return (ENXIO);
	}

	memset(&pckbc_consdata, 0, sizeof(pckbc_consdata));
	pckbc_consdata.t_iot = iot;
	pckbc_consdata.t_ioh_d = ioh_d;
	pckbc_consdata.t_ioh_c = ioh_c;
	pckbc_consdata.t_addr = addr;
	pckbc_consdata.t_flags = flags;
	callout_init(&pckbc_consdata.t_cleanup, 0);

	/* flush */
	(void) pckbc_poll_data1(&pckbc_consdata, PCKBC_KBD_SLOT);

#ifdef PCKBC_CNATTACH_SELFTEST
	/*
	 * In some machines (e.g. netwinder) pckbc refuses to talk at
	 * all until we request a self-test.
	 */
	if (!pckbc_send_cmd(iot, ioh_c, KBC_SELFTEST)) {
		printf("pckbc: unable to request selftest\n");
		res = EIO;
		goto out;
	}

	reply = pckbc_poll_data1(&pckbc_consdata, PCKBC_KBD_SLOT);
	if (reply != 0x55) {
		printf("pckbc: selftest returned 0x%02x\n", reply);
		res = EIO;
		goto out;
	}
#endif /* PCKBC_CNATTACH_SELFTEST */

	/* init cmd byte, enable ports */
	pckbc_consdata.t_cmdbyte = KC8_CPU;
	if (!pckbc_put8042cmd(&pckbc_consdata)) {
		printf("pckbc: cmd word write error\n");
		res = EIO;
		goto out;
	}

	res = pckbport_cnattach(&pckbc_consdata, &pckbc_ops, slot);

  out:
	if (res) {
		bus_space_unmap(iot, pckbc_consdata.t_ioh_d, 1);
		bus_space_unmap(iot, pckbc_consdata.t_ioh_c, 1);
	} else {
		pckbc_consdata.t_slotdata[slot] = &pckbc_cons_slotdata;
		pckbc_init_slotdata(&pckbc_cons_slotdata);
		pckbc_console = 1;
	}

	return (res);
}

bool
pckbc_resume(device_t dv, const pmf_qual_t *qual)
{
	struct pckbc_softc *sc = device_private(dv);
	struct pckbc_internal *t;

	t = sc->id;
	(void)pckbc_poll_data1(t, PCKBC_KBD_SLOT);
	if (!pckbc_send_cmd(t->t_iot, t->t_ioh_c, KBC_SELFTEST))
		return false;
	(void)pckbc_poll_data1(t, PCKBC_KBD_SLOT);
	(void)pckbc_put8042cmd(t);
	pckbcintr(t->t_sc);

	return true;
}
