/* $NetBSD: pckbport.c,v 1.17 2014/01/11 20:29:03 jakllsch Exp $ */

/*
 * Copyright (c) 2004 Ben Harris
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
__KERNEL_RCSID(0, "$NetBSD: pckbport.c,v 1.17 2014/01/11 20:29:03 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/queue.h>

#include <dev/pckbport/pckbdreg.h>
#include <dev/pckbport/pckbportvar.h>

#include "locators.h"

#include "pckbd.h"
#if (NPCKBD > 0)
#include <dev/pckbport/pckbdvar.h>
#endif

/* descriptor for one device command */
struct pckbport_devcmd {
	TAILQ_ENTRY(pckbport_devcmd) next;
	int flags;
#define KBC_CMDFLAG_SYNC 1 /* give descriptor back to caller */
#define KBC_CMDFLAG_SLOW 2
	u_char cmd[4];
	int cmdlen, cmdidx, retries;
	u_char response[4];
	int status, responselen, responseidx;
};

/* data per slave device */
struct pckbport_slotdata {
	int polling;	/* don't process data in interrupt handler */
	TAILQ_HEAD(, pckbport_devcmd) cmdqueue; /* active commands */
	TAILQ_HEAD(, pckbport_devcmd) freequeue; /* free commands */
#define NCMD 5
	struct pckbport_devcmd cmds[NCMD];
};

#define CMD_IN_QUEUE(q) (TAILQ_FIRST(&(q)->cmdqueue) != NULL)

static void pckbport_init_slotdata(struct pckbport_slotdata *);
static int pckbportprint(void *, const char *);

static struct pckbport_slotdata pckbport_cons_slotdata;

static int pckbport_poll_data1(pckbport_tag_t, pckbport_slot_t);
static int pckbport_send_devcmd(struct pckbport_tag *, pckbport_slot_t,
				  u_char);
static void pckbport_poll_cmd1(struct pckbport_tag *, pckbport_slot_t,
				 struct pckbport_devcmd *);

static void pckbport_cleanqueue(struct pckbport_slotdata *);
static void pckbport_cleanup(void *);
static int pckbport_cmdresponse(struct pckbport_tag *, pckbport_slot_t,
					u_char);
static void pckbport_start(struct pckbport_tag *, pckbport_slot_t);

static const char * const pckbport_slot_names[] = { "kbd", "aux" };

static struct pckbport_tag pckbport_cntag;

#define	KBD_DELAY	DELAY(8)

#ifdef PCKBPORTDEBUG
#define DPRINTF(a)	printf a
#else
#define DPRINTF(a)
#endif

static int
pckbport_poll_data1(pckbport_tag_t t, pckbport_slot_t slot)
{

	return t->t_ops->t_poll_data1(t->t_cookie, slot);
}

static int
pckbport_send_devcmd(struct pckbport_tag *t, pckbport_slot_t slot, u_char val)
{

	return t->t_ops->t_send_devcmd(t->t_cookie, slot, val);
}

pckbport_tag_t
pckbport_attach(void *cookie, struct pckbport_accessops const *ops)
{
	pckbport_tag_t t;

	if (cookie == pckbport_cntag.t_cookie &&
	    ops == pckbport_cntag.t_ops)
		return &pckbport_cntag;
	t = malloc(sizeof(struct pckbport_tag), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (t == NULL) return NULL;
	callout_init(&t->t_cleanup, 0);
	t->t_cookie = cookie;
	t->t_ops = ops;
	return t;
}

device_t
pckbport_attach_slot(device_t dev, pckbport_tag_t t,
    pckbport_slot_t slot)
{
	struct pckbport_attach_args pa;
	void *sdata;
	device_t found;
	int alloced = 0;
	int locs[PCKBPORTCF_NLOCS];

	pa.pa_tag = t;
	pa.pa_slot = slot;

	if (t->t_slotdata[slot] == NULL) {
		sdata = malloc(sizeof(struct pckbport_slotdata),
		    M_DEVBUF, M_NOWAIT);
		if (sdata == NULL) {
			aprint_error_dev(dev, "no memory\n");
			return 0;
		}
		t->t_slotdata[slot] = sdata;
		pckbport_init_slotdata(t->t_slotdata[slot]);
		alloced++;
	}

	locs[PCKBPORTCF_SLOT] = slot;

	found = config_found_sm_loc(dev, "pckbport", locs, &pa,
				    pckbportprint, config_stdsubmatch);

	if (found == NULL && alloced) {
		free(t->t_slotdata[slot], M_DEVBUF);
		t->t_slotdata[slot] = NULL;
	}

	return found;
}

int
pckbportprint(void *aux, const char *pnp)
{
	struct pckbport_attach_args *pa = aux;

	if (!pnp)
		aprint_normal(" (%s slot)", pckbport_slot_names[pa->pa_slot]);
	return QUIET;
}

void
pckbport_init_slotdata(struct pckbport_slotdata *q)
{
	int i;

	TAILQ_INIT(&q->cmdqueue);
	TAILQ_INIT(&q->freequeue);

	for (i = 0; i < NCMD; i++)
		TAILQ_INSERT_TAIL(&q->freequeue, &(q->cmds[i]), next);

	q->polling = 0;
}

void
pckbport_flush(pckbport_tag_t t, pckbport_slot_t slot)
{

	(void)pckbport_poll_data1(t, slot);
}

int
pckbport_poll_data(pckbport_tag_t t, pckbport_slot_t slot)
{
	struct pckbport_slotdata *q = t->t_slotdata[slot];
	int c;

	c = pckbport_poll_data1(t, slot);
	if (c != -1 && q && CMD_IN_QUEUE(q))
		/*
		 * we jumped into a running command - try to deliver
		 * the response
		 */
		if (pckbport_cmdresponse(t, slot, c))
			return -1;
	return c;
}

/*
 * switch scancode translation on / off
 * return nonzero on success
 */
int
pckbport_xt_translation(pckbport_tag_t t, pckbport_slot_t slot,	int on)
{

	return t->t_ops->t_xt_translation(t->t_cookie, slot, on);
}

void
pckbport_slot_enable(pckbport_tag_t t, pckbport_slot_t slot, int on)
{

	t->t_ops->t_slot_enable(t->t_cookie, slot, on);
}

void
pckbport_set_poll(pckbport_tag_t t, pckbport_slot_t slot, int on)
{

	t->t_slotdata[slot]->polling = on;
	t->t_ops->t_set_poll(t->t_cookie, slot, on);
}

/*
 * Pass command to device, poll for ACK and data.
 * to be called at spltty()
 */
static void
pckbport_poll_cmd1(struct pckbport_tag *t, pckbport_slot_t slot,
    struct pckbport_devcmd *cmd)
{
	int i, c = 0;

	while (cmd->cmdidx < cmd->cmdlen) {
		if (!pckbport_send_devcmd(t, slot, cmd->cmd[cmd->cmdidx])) {
			printf("pckbport_cmd: send error\n");
			cmd->status = EIO;
			return;
		}
		for (i = 10; i; i--) { /* 1s ??? */
			c = pckbport_poll_data1(t, slot);
			if (c != -1)
				break;
		}
		switch (c) {
		case KBR_ACK:
			cmd->cmdidx++;
			continue;
		case KBR_BAT_DONE:
		case KBR_BAT_FAIL:
		case KBR_RESEND:
			DPRINTF(("%s: %s\n", __func__, c == KBR_RESEND ?
			    "RESEND" : (c == KBR_BAT_DONE ? "BAT_DONE" :
			    "BAT_FAIL")));
			if (cmd->retries++ < 5)
				continue;
			else {
				DPRINTF(("%s: cmd failed\n", __func__));
				cmd->status = EIO;
				return;
			}
		case -1:
			DPRINTF(("%s: timeout\n", __func__));
			cmd->status = EIO;
			return;
		}
		DPRINTF(("%s: lost 0x%x\n", __func__, c));
	}

	while (cmd->responseidx < cmd->responselen) {
		if (cmd->flags & KBC_CMDFLAG_SLOW)
			i = 100; /* 10s ??? */
		else
			i = 10; /* 1s ??? */
		while (i--) {
			c = pckbport_poll_data1(t, slot);
			if (c != -1)
				break;
		}
		if (c == -1) {
			DPRINTF(("%s: no data\n", __func__));
			cmd->status = ETIMEDOUT;
			return;
		} else
			cmd->response[cmd->responseidx++] = c;
	}
}

/* for use in autoconfiguration */
int
pckbport_poll_cmd(pckbport_tag_t t, pckbport_slot_t slot, const u_char *cmd,
    int len, int responselen, u_char *respbuf, int slow)
{
	struct pckbport_devcmd nc;

	if ((len > 4) || (responselen > 4))
		return (EINVAL);

	memset(&nc, 0, sizeof(nc));
	memcpy(nc.cmd, cmd, len);
	nc.cmdlen = len;
	nc.responselen = responselen;
	nc.flags = (slow ? KBC_CMDFLAG_SLOW : 0);

	pckbport_poll_cmd1(t, slot, &nc);

	if (nc.status == 0 && respbuf)
		memcpy(respbuf, nc.response, responselen);

	return nc.status;
}

/*
 * Clean up a command queue, throw away everything.
 */
void
pckbport_cleanqueue(struct pckbport_slotdata *q)
{
	struct pckbport_devcmd *cmd;

	while ((cmd = TAILQ_FIRST(&q->cmdqueue))) {
		TAILQ_REMOVE(&q->cmdqueue, cmd, next);
#ifdef PCKBPORTDEBUG
		printf("%s: removing", __func__);
		for (int i = 0; i < cmd->cmdlen; i++)
			printf(" %02x", cmd->cmd[i]);
		printf("\n");
#endif
		TAILQ_INSERT_TAIL(&q->freequeue, cmd, next);
	}
}

/*
 * Timeout error handler: clean queues and data port.
 * XXX could be less invasive.
 */
void
pckbport_cleanup(void *self)
{
	struct pckbport_tag *t = self;
	int s;
	u_char cmd[1], resp[2];

	printf("pckbport: command timeout\n");

	s = spltty();

	if (t->t_slotdata[PCKBPORT_KBD_SLOT])
		pckbport_cleanqueue(t->t_slotdata[PCKBPORT_KBD_SLOT]);
	if (t->t_slotdata[PCKBPORT_AUX_SLOT])
		pckbport_cleanqueue(t->t_slotdata[PCKBPORT_AUX_SLOT]);

#if 0 /* XXXBJH Move to controller driver? */
	while (bus_space_read_1(t->t_iot, t->t_ioh_c, 0) & KBS_DIB) {
		KBD_DELAY;
		(void) bus_space_read_1(t->t_iot, t->t_ioh_d, 0);
	}
#endif

	cmd[0] = KBC_RESET;
	(void)pckbport_poll_cmd(t, PCKBPORT_KBD_SLOT, cmd, 1, 2, resp, 1);
	pckbport_flush(t, PCKBPORT_KBD_SLOT);

	splx(s);
}

/*
 * Pass command to device during normal operation.
 * to be called at spltty()
 */
void
pckbport_start(struct pckbport_tag *t, pckbport_slot_t slot)
{
	struct pckbport_slotdata *q = t->t_slotdata[slot];
	struct pckbport_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);

	KASSERT(cmd != NULL);
	if (q->polling) {
		do {
			pckbport_poll_cmd1(t, slot, cmd);
			if (cmd->status)
				printf("pckbport_start: command error\n");

			TAILQ_REMOVE(&q->cmdqueue, cmd, next);
			if (cmd->flags & KBC_CMDFLAG_SYNC)
				wakeup(cmd);
			else {
				callout_stop(&t->t_cleanup);
				TAILQ_INSERT_TAIL(&q->freequeue, cmd, next);
			}
			cmd = TAILQ_FIRST(&q->cmdqueue);
		} while (cmd);
		return;
	}

	if (!pckbport_send_devcmd(t, slot, cmd->cmd[cmd->cmdidx])) {
		printf("pckbport_start: send error\n");
		/* XXX what now? */
		return;
	}
}

/*
 * Handle command responses coming in asynchronously,
 * return nonzero if valid response.
 * to be called at spltty()
 */
int
pckbport_cmdresponse(struct pckbport_tag *t, pckbport_slot_t slot, u_char data)
{
	struct pckbport_slotdata *q = t->t_slotdata[slot];
	struct pckbport_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);

	KASSERT(cmd != NULL);
	if (cmd->cmdidx < cmd->cmdlen) {
		if (data != KBR_ACK && data != KBR_RESEND)
			return 0;

		if (data == KBR_RESEND) {
			if (cmd->retries++ < 5)
				/* try again last command */
				goto restart;
			else {
				DPRINTF(("%s: cmd failed\n", __func__));
				cmd->status = EIO;
				/* dequeue */
			}
		} else {
			if (++cmd->cmdidx < cmd->cmdlen)
				goto restart;
			if (cmd->responselen)
				return 1;
			/* else dequeue */
		}
	} else if (cmd->responseidx < cmd->responselen) {
		cmd->response[cmd->responseidx++] = data;
		if (cmd->responseidx < cmd->responselen)
			return 1;
		/* else dequeue */
	} else
		return 0;

	/* dequeue: */
	TAILQ_REMOVE(&q->cmdqueue, cmd, next);
	if (cmd->flags & KBC_CMDFLAG_SYNC)
		wakeup(cmd);
	else {
		callout_stop(&t->t_cleanup);
		TAILQ_INSERT_TAIL(&q->freequeue, cmd, next);
	}
	if (!CMD_IN_QUEUE(q))
		return 1;
restart:
	pckbport_start(t, slot);
	return 1;
}

/*
 * Put command into the device's command queue, return zero or errno.
 */
int
pckbport_enqueue_cmd(pckbport_tag_t t, pckbport_slot_t slot, const u_char *cmd,
    int len, int responselen, int sync, u_char *respbuf)
{
	struct pckbport_slotdata *q = t->t_slotdata[slot];
	struct pckbport_devcmd *nc;
	int s, isactive, res = 0;

	if ((len > 4) || (responselen > 4))
		return EINVAL;
	s = spltty();
	nc = TAILQ_FIRST(&q->freequeue);
	if (nc)
		TAILQ_REMOVE(&q->freequeue, nc, next);
	splx(s);
	if (!nc)
		return ENOMEM;

	memset(nc, 0, sizeof(*nc));
	memcpy(nc->cmd, cmd, len);
	nc->cmdlen = len;
	nc->responselen = responselen;
	nc->flags = (sync ? KBC_CMDFLAG_SYNC : 0);

	s = spltty();

	if (q->polling && sync)
		/*
		 * XXX We should poll until the queue is empty.
		 * But we don't come here normally, so make
		 * it simple and throw away everything.
		 */
		pckbport_cleanqueue(q);

	isactive = CMD_IN_QUEUE(q);
	TAILQ_INSERT_TAIL(&q->cmdqueue, nc, next);
	if (!isactive)
		pckbport_start(t, slot);

	if (q->polling)
		res = (sync ? nc->status : 0);
	else if (sync) {
		if ((res = tsleep(nc, 0, "kbccmd", 1*hz))) {
			TAILQ_REMOVE(&q->cmdqueue, nc, next);
			pckbport_cleanup(t);
		} else
			res = nc->status;
	} else
		callout_reset(&t->t_cleanup, hz, pckbport_cleanup, t);

	if (sync) {
		if (respbuf)
			memcpy(respbuf, nc->response, responselen);
		TAILQ_INSERT_TAIL(&q->freequeue, nc, next);
	}

	splx(s);

	return res;
}

void
pckbport_set_inputhandler(pckbport_tag_t t, pckbport_slot_t slot,
    pckbport_inputfcn func, void *arg, const char *name)
{

	if (slot >= PCKBPORT_NSLOTS)
		panic("pckbport_set_inputhandler: bad slot %d", slot);

	t->t_ops->t_intr_establish(t->t_cookie, slot);

	t->t_inputhandler[slot] = func;
	t->t_inputarg[slot] = arg;
	t->t_subname[slot] = name;
}

void
pckbportintr(pckbport_tag_t t, pckbport_slot_t slot, int data)
{
	struct pckbport_slotdata *q;

	q = t->t_slotdata[slot];

	if (!q) {
		/* XXX do something for live insertion? */
		printf("pckbportintr: no dev for slot %d\n", slot);
		return;
	}

	if (CMD_IN_QUEUE(q) && pckbport_cmdresponse(t, slot, data))
		return;

	if (t->t_inputhandler[slot]) {
		(*t->t_inputhandler[slot])(t->t_inputarg[slot], data);
		return;
	}
	DPRINTF(("%s: slot %d lost %d\n", __func__, slot, data));
}

int
pckbport_cnattach(void *cookie, struct pckbport_accessops const *ops,
    pckbport_slot_t slot)
{
	int res = 0;
	pckbport_tag_t t = &pckbport_cntag;

	callout_init(&t->t_cleanup, 0);
	t->t_cookie = cookie;
	t->t_ops = ops;

	/* flush */
	pckbport_flush(t, slot);

#if (NPCKBD > 0)
	res = pckbd_cnattach(t, slot);
#elif (NPCKBPORT_MACHDEP_CNATTACH > 0)
	res = pckbport_machdep_cnattach(t, slot);
#else
	res = ENXIO;
#endif /* NPCKBPORT_MACHDEP_CNATTACH > 0 */

	if (res == 0) {
		t->t_slotdata[slot] = &pckbport_cons_slotdata;
		pckbport_init_slotdata(&pckbport_cons_slotdata);
	}

	return res;
}
