/*	$NetBSD: firewire.c,v 1.45 2014/10/18 08:33:28 snj Exp $	*/
/*-
 * Copyright (c) 2003 Hidetoshi Shimokawa
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/firewire/firewire.c,v 1.110 2009/04/07 02:33:46 sbruno Exp $
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: firewire.c,v 1.45 2014/10/18 08:33:28 snj Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwmem.h>
#include <dev/ieee1394/iec13213.h>
#include <dev/ieee1394/iec68113.h>

#include "locators.h"

struct crom_src_buf {
	struct crom_src	src;
	struct crom_chunk root;
	struct crom_chunk vendor;
	struct crom_chunk hw;
};

int firewire_debug = 0, try_bmr = 1, hold_count = 0;
/*
 * Setup sysctl(3) MIB, hw.ieee1394if.*
 *
 * TBD condition CTLFLAG_PERMANENT on being a module or not
 */
SYSCTL_SETUP(sysctl_ieee1394if, "sysctl ieee1394if(4) subtree setup")
{
	int rc, ieee1394if_node_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "ieee1394if",
	    SYSCTL_DESCR("ieee1394if controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	ieee1394if_node_num = node->sysctl_num;

	/* ieee1394if try bus manager flag */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "try_bmr", SYSCTL_DESCR("Try to be a bus manager"),
	    NULL, 0, &try_bmr,
	    0, CTL_HW, ieee1394if_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	/* ieee1394if hold count */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "hold_count", SYSCTL_DESCR("Number of count of "
	    "bus resets for removing lost device information"),
	    NULL, 0, &hold_count,
	    0, CTL_HW, ieee1394if_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	/* ieee1394if driver debug flag */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "ieee1394_debug", SYSCTL_DESCR("ieee1394if driver debug flag"),
	    NULL, 0, &firewire_debug,
	    0, CTL_HW, ieee1394if_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	return;

err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

MALLOC_DEFINE(M_FW, "ieee1394", "IEEE1394");

#define FW_MAXASYRTY 4

#define FW_GENERATION_CHANGEABLE	2

static int firewirematch (device_t, cfdata_t, void *);
static void firewireattach (device_t, device_t, void *);
static int firewiredetach (device_t, int);
static int firewire_print (void *, const char *);

int firewire_resume (struct firewire_comm *);

static void fw_asystart(struct fw_xfer *);
static void firewire_xfer_timeout(struct firewire_comm *);
static void firewire_watchdog(void *);
static void fw_xferq_drain(struct fw_xferq *);
static void fw_reset_csr(struct firewire_comm *);
static void fw_init_crom(struct firewire_comm *);
static void fw_reset_crom(struct firewire_comm *);
static void fw_dump_hdr(struct fw_pkt *, const char *);
static void fw_tl_free(struct firewire_comm *, struct fw_xfer *);
static struct fw_xfer *fw_tl2xfer(struct firewire_comm *, int, int, int);
static void fw_phy_config(struct firewire_comm *, int, int);
static void fw_print_sid(uint32_t);
static void fw_bus_probe(struct firewire_comm *);
static int fw_explore_read_quads(struct fw_device *, int, uint32_t *, int);
static int fw_explore_csrblock(struct fw_device *, int, int);
static int fw_explore_node(struct fw_device *);
static union fw_self_id *fw_find_self_id(struct firewire_comm *, int);
static void fw_explore(struct firewire_comm *);
static void fw_bus_probe_thread(void *);
static void fw_attach_dev(struct firewire_comm *);
static int fw_get_tlabel(struct firewire_comm *, struct fw_xfer *);
static void fw_rcv_copy(struct fw_rcv_buf *);
static void fw_try_bmr_callback(struct fw_xfer *);
static void fw_try_bmr(void *);
static int fw_bmr(struct firewire_comm *);


CFATTACH_DECL_NEW(ieee1394if, sizeof(struct firewire_softc),
    firewirematch, firewireattach, firewiredetach, NULL);


const char *fw_linkspeed[] = {
	"S100", "S200", "S400", "S800",
	"S1600", "S3200", "undef", "undef"
};

static const char *tcode_str[] = {
	"WREQQ", "WREQB", "WRES",   "undef",
	"RREQQ", "RREQB", "RRESQ",  "RRESB",
	"CYCS",  "LREQ",  "STREAM", "LRES",
	"undef", "undef", "PHY",    "undef"
};

/* IEEE-1394a Table C-2 Gap count as a function of hops*/
#define MAX_GAPHOP 15
u_int gap_cnt[] = { 5,  5,  7,  8, 10, 13, 16, 18,
		   21, 24, 26, 29, 32, 35, 37, 40};


static int
firewirematch(device_t parent, cfdata_t cf, void *aux)
{

	return 1;	/* always match */
}

static void
firewireattach(device_t parent, device_t self, void *aux)
{
	struct firewire_softc *sc = device_private(self);
	struct firewire_comm *fc = device_private(parent);
	struct fw_attach_args faa;
	struct firewire_dev_list *devlist;

	aprint_naive("\n");
	aprint_normal(": IEEE1394 bus\n");

	fc->bdev = sc->dev = self;
	sc->fc = fc;
	SLIST_INIT(&sc->devlist);

	fc->status = FWBUSNOTREADY;

	if (fc->nisodma > FWMAXNDMA)
	    fc->nisodma = FWMAXNDMA;

	fc->crom_src_buf =
	    (struct crom_src_buf *)malloc(sizeof(struct crom_src_buf),
	    M_FW, M_NOWAIT | M_ZERO);
	if (fc->crom_src_buf == NULL) {
		aprint_error_dev(fc->bdev, "Malloc Failure crom src buff\n");
		return;
	}
	fc->topology_map =
	    (struct fw_topology_map *)malloc(sizeof(struct fw_topology_map),
	    M_FW, M_NOWAIT | M_ZERO);
	if (fc->topology_map == NULL) {
		aprint_error_dev(fc->dev, "Malloc Failure topology map\n");
		free(fc->crom_src_buf, M_FW);
		return;
	}
	fc->speed_map =
	    (struct fw_speed_map *)malloc(sizeof(struct fw_speed_map),
	    M_FW, M_NOWAIT | M_ZERO);
	if (fc->speed_map == NULL) {
		aprint_error_dev(fc->dev, "Malloc Failure speed map\n");
		free(fc->crom_src_buf, M_FW);
		free(fc->topology_map, M_FW);
		return;
	}

	mutex_init(&fc->tlabel_lock, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&fc->fc_mtx, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&fc->wait_lock, MUTEX_DEFAULT, IPL_VM);
	cv_init(&fc->fc_cv, "ieee1394");

	callout_init(&fc->timeout_callout, CALLOUT_MPSAFE);
	callout_setfunc(&fc->timeout_callout, firewire_watchdog, fc);
	callout_init(&fc->bmr_callout, CALLOUT_MPSAFE);
	callout_setfunc(&fc->bmr_callout, fw_try_bmr, fc);
	callout_init(&fc->busprobe_callout, CALLOUT_MPSAFE);
	callout_setfunc(&fc->busprobe_callout, (void *)fw_bus_probe, fc);

	callout_schedule(&fc->timeout_callout, hz);

	/* Tell config we will have started a thread to scan the bus.  */
	config_pending_incr(self);

	/* create thread */
	if (kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL, fw_bus_probe_thread,
	    fc, &fc->probe_thread, "fw%dprobe", device_unit(fc->bdev))) {
		aprint_error_dev(self, "kthread_create failed\n");
		config_pending_decr(self);
	}

	devlist = malloc(sizeof(struct firewire_dev_list), M_DEVBUF, M_NOWAIT);
	if (devlist == NULL) {
		aprint_error_dev(self, "device list allocation failed\n");
		return;
	}

	faa.name = "fwip";
	faa.fc = fc;
	faa.fwdev = NULL;
	devlist->dev = config_found(sc->dev, &faa, firewire_print);
	if (devlist->dev == NULL)
		free(devlist, M_DEVBUF);
	else
		SLIST_INSERT_HEAD(&sc->devlist, devlist, link);

	/* bus_reset */
	fw_busreset(fc, FWBUSNOTREADY);
	fc->ibr(fc);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;
}

static int
firewiredetach(device_t self, int flags)
{
	struct firewire_softc *sc = device_private(self);
	struct firewire_comm *fc;
	struct fw_device *fwdev, *fwdev_next;
	struct firewire_dev_list *devlist;
	int err;

	fc = sc->fc;
	mutex_enter(&fc->wait_lock);
	fc->status = FWBUSDETACH;
	cv_signal(&fc->fc_cv);
	while (fc->status != FWBUSDETACHOK) {
		err = cv_timedwait_sig(&fc->fc_cv, &fc->wait_lock, hz * 60);
		if (err == EWOULDBLOCK) {
			aprint_error_dev(self,
			    "firewire probe thread didn't die\n");
			break;
		}
	}
	mutex_exit(&fc->wait_lock);


	while ((devlist = SLIST_FIRST(&sc->devlist)) != NULL) {
		if ((err = config_detach(devlist->dev, flags)) != 0)
			return err;
		SLIST_REMOVE(&sc->devlist, devlist, firewire_dev_list, link);
		free(devlist, M_DEVBUF);
	}

	callout_stop(&fc->timeout_callout);
	callout_stop(&fc->bmr_callout);
	callout_stop(&fc->busprobe_callout);

	/* XXX xfer_free and untimeout on all xfers */
	for (fwdev = STAILQ_FIRST(&fc->devices); fwdev != NULL;
	    fwdev = fwdev_next) {
		fwdev_next = STAILQ_NEXT(fwdev, link);
		free(fwdev, M_FW);
	}
	free(fc->topology_map, M_FW);
	free(fc->speed_map, M_FW);
	free(fc->crom_src_buf, M_FW);

	cv_destroy(&fc->fc_cv);
	mutex_destroy(&fc->wait_lock);
	mutex_destroy(&fc->fc_mtx);
	mutex_destroy(&fc->tlabel_lock);
	return 0;
}

static int
firewire_print(void *aux, const char *pnp)
{
	struct fw_attach_args *fwa = (struct fw_attach_args *)aux;

	if (pnp)
		aprint_normal("%s at %s", fwa->name, pnp);

	return UNCONF;
}

int
firewire_resume(struct firewire_comm *fc)
{

	fc->status = FWBUSNOTREADY;
	return 0;
}


/*
 * Lookup fwdev by node id.
 */
struct fw_device *
fw_noderesolve_nodeid(struct firewire_comm *fc, int dst)
{
	struct fw_device *fwdev;

	mutex_enter(&fc->fc_mtx);
	STAILQ_FOREACH(fwdev, &fc->devices, link)
		if (fwdev->dst == dst && fwdev->status != FWDEVINVAL)
			break;
	mutex_exit(&fc->fc_mtx);

	return fwdev;
}

/*
 * Lookup fwdev by EUI64.
 */
struct fw_device *
fw_noderesolve_eui64(struct firewire_comm *fc, struct fw_eui64 *eui)
{
	struct fw_device *fwdev;

	mutex_enter(&fc->fc_mtx);
	STAILQ_FOREACH(fwdev, &fc->devices, link)
		if (FW_EUI64_EQUAL(fwdev->eui, *eui))
			break;
	mutex_exit(&fc->fc_mtx);

	if (fwdev == NULL)
		return NULL;
	if (fwdev->status == FWDEVINVAL)
		return NULL;
	return fwdev;
}

/*
 * Async. request procedure for userland application.
 */
int
fw_asyreq(struct firewire_comm *fc, int sub, struct fw_xfer *xfer)
{
	struct fw_xferq *xferq;
	int len;
	struct fw_pkt *fp;
	int tcode;
	const struct tcode_info *info;

	if (xfer == NULL)
		return EINVAL;
	if (xfer->hand == NULL) {
		aprint_error_dev(fc->bdev, "hand == NULL\n");
		return EINVAL;
	}
	fp = &xfer->send.hdr;

	tcode = fp->mode.common.tcode & 0xf;
	info = &fc->tcode[tcode];
	if (info->flag == 0) {
		aprint_error_dev(fc->bdev, "invalid tcode=%x\n", tcode);
		return EINVAL;
	}

	/* XXX allow bus explore packets only after bus rest */
	if ((fc->status < FWBUSEXPLORE) &&
	    ((tcode != FWTCODE_RREQQ) || (fp->mode.rreqq.dest_hi != 0xffff) ||
	    (fp->mode.rreqq.dest_lo < 0xf0000000) ||
	    (fp->mode.rreqq.dest_lo >= 0xf0001000))) {
		xfer->resp = EAGAIN;
		xfer->flag = FWXF_BUSY;
		return EAGAIN;
	}

	if (info->flag & FWTI_REQ)
		xferq = fc->atq;
	else
		xferq = fc->ats;
	len = info->hdr_len;
	if (xfer->send.pay_len > MAXREC(fc->maxrec)) {
		aprint_error_dev(fc->bdev, "send.pay_len > maxrec\n");
		return EINVAL;
	}
	if (info->flag & FWTI_BLOCK_STR)
		len = fp->mode.stream.len;
	else if (info->flag & FWTI_BLOCK_ASY)
		len = fp->mode.rresb.len;
	else
		len = 0;
	if (len != xfer->send.pay_len) {
		aprint_error_dev(fc->bdev,
		    "len(%d) != send.pay_len(%d) %s(%x)\n",
		    len, xfer->send.pay_len, tcode_str[tcode], tcode);
		return EINVAL;
	}

	if (xferq->start == NULL) {
		aprint_error_dev(fc->bdev, "xferq->start == NULL\n");
		return EINVAL;
	}
	if (!(xferq->queued < xferq->maxq)) {
		aprint_error_dev(fc->bdev, "Discard a packet (queued=%d)\n",
			xferq->queued);
		return EAGAIN;
	}

	xfer->tl = -1;
	if (info->flag & FWTI_TLABEL)
		if (fw_get_tlabel(fc, xfer) < 0)
			return EAGAIN;

	xfer->resp = 0;
	xfer->fc = fc;
	xfer->q = xferq;

	fw_asystart(xfer);
	return 0;
}

/*
 * Wakeup blocked process.
 */
void
fw_xferwake(struct fw_xfer *xfer)
{

	mutex_enter(&xfer->fc->wait_lock);
	xfer->flag |= FWXF_WAKE;
	cv_signal(&xfer->cv);
	mutex_exit(&xfer->fc->wait_lock);

	return;
}

int
fw_xferwait(struct fw_xfer *xfer)
{
	struct firewire_comm *fc = xfer->fc;
	int err = 0;

	mutex_enter(&fc->wait_lock);
	while (!(xfer->flag & FWXF_WAKE))
		err = cv_wait_sig(&xfer->cv, &fc->wait_lock);
	mutex_exit(&fc->wait_lock);

	return err;
}

void
fw_drain_txq(struct firewire_comm *fc)
{
	struct fw_xfer *xfer;
	STAILQ_HEAD(, fw_xfer) xfer_drain;
	int i;

	STAILQ_INIT(&xfer_drain);

	mutex_enter(&fc->atq->q_mtx);
	fw_xferq_drain(fc->atq);
	mutex_exit(&fc->atq->q_mtx);
	mutex_enter(&fc->ats->q_mtx);
	fw_xferq_drain(fc->ats);
	mutex_exit(&fc->ats->q_mtx);
	for (i = 0; i < fc->nisodma; i++)
		fw_xferq_drain(fc->it[i]);

	mutex_enter(&fc->tlabel_lock);
	for (i = 0; i < 0x40; i++)
		while ((xfer = STAILQ_FIRST(&fc->tlabels[i])) != NULL) {
			if (firewire_debug)
				printf("tl=%d flag=%d\n", i, xfer->flag);
			xfer->resp = EAGAIN;
			STAILQ_REMOVE_HEAD(&fc->tlabels[i], tlabel);
			STAILQ_INSERT_TAIL(&xfer_drain, xfer, tlabel);
		}
	mutex_exit(&fc->tlabel_lock);

	STAILQ_FOREACH(xfer, &xfer_drain, tlabel)
		xfer->hand(xfer);
}

/*
 * Called after bus reset.
 */
void
fw_busreset(struct firewire_comm *fc, uint32_t new_status)
{
	struct firewire_softc *sc = device_private(fc->bdev);
	struct firewire_dev_list *devlist;
	struct firewire_dev_comm *fdc;
	struct crom_src *src;
	uint32_t *newrom;

	if (fc->status == FWBUSMGRELECT)
		callout_stop(&fc->bmr_callout);

	fc->status = new_status;
	fw_reset_csr(fc);

	if (fc->status == FWBUSNOTREADY)
		fw_init_crom(fc);

	fw_reset_crom(fc);

	/* How many safe this access? */
	SLIST_FOREACH(devlist, &sc->devlist, link) {
		fdc = device_private(devlist->dev);
		if (fdc->post_busreset != NULL)
			fdc->post_busreset(fdc);
	}

	/*
	 * If the old config rom needs to be overwritten,
	 * bump the businfo.generation indicator to
	 * indicate that we need to be reprobed
	 * See 1394a-2000 8.3.2.5.4 for more details.
	 * generation starts at 2 and rolls over at 0xF
	 * back to 2.
	 *
	 * A generation of 0 indicates a device
	 * that is not 1394a-2000 compliant.
	 * A generation of 1 indicates a device that
	 * does not change its Bus Info Block or
	 * Configuration ROM.
	 */
#define FW_MAX_GENERATION	0xF
	newrom = malloc(CROMSIZE, M_FW, M_NOWAIT | M_ZERO);
	src = &fc->crom_src_buf->src;
	crom_load(src, newrom, CROMSIZE);
	if (memcmp(newrom, fc->config_rom, CROMSIZE) != 0) {
		if (src->businfo.generation++ > FW_MAX_GENERATION)
			src->businfo.generation = FW_GENERATION_CHANGEABLE;
		memcpy((void *)fc->config_rom, newrom, CROMSIZE);
	}
	free(newrom, M_FW);
}

/* Call once after reboot */
void
fw_init(struct firewire_comm *fc)
{
	int i;

	fc->arq->queued = 0;
	fc->ars->queued = 0;
	fc->atq->queued = 0;
	fc->ats->queued = 0;

	fc->arq->buf = NULL;
	fc->ars->buf = NULL;
	fc->atq->buf = NULL;
	fc->ats->buf = NULL;

	fc->arq->flag = 0;
	fc->ars->flag = 0;
	fc->atq->flag = 0;
	fc->ats->flag = 0;

	STAILQ_INIT(&fc->atq->q);
	STAILQ_INIT(&fc->ats->q);
	mutex_init(&fc->arq->q_mtx, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&fc->ars->q_mtx, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&fc->atq->q_mtx, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&fc->ats->q_mtx, MUTEX_DEFAULT, IPL_VM);

	for (i = 0; i < fc->nisodma; i++) {
		fc->it[i]->queued = 0;
		fc->ir[i]->queued = 0;

		fc->it[i]->start = NULL;
		fc->ir[i]->start = NULL;

		fc->it[i]->buf = NULL;
		fc->ir[i]->buf = NULL;

		fc->it[i]->flag = FWXFERQ_STREAM;
		fc->ir[i]->flag = FWXFERQ_STREAM;

		STAILQ_INIT(&fc->it[i]->q);
		STAILQ_INIT(&fc->ir[i]->q);
	}

	fc->arq->maxq = FWMAXQUEUE;
	fc->ars->maxq = FWMAXQUEUE;
	fc->atq->maxq = FWMAXQUEUE;
	fc->ats->maxq = FWMAXQUEUE;

	for (i = 0; i < fc->nisodma; i++) {
		fc->ir[i]->maxq = FWMAXQUEUE;
		fc->it[i]->maxq = FWMAXQUEUE;
	}

	CSRARC(fc, TOPO_MAP) = 0x3f1 << 16;
	CSRARC(fc, TOPO_MAP + 4) = 1;
	CSRARC(fc, SPED_MAP) = 0x3f1 << 16;
	CSRARC(fc, SPED_MAP + 4) = 1;

	STAILQ_INIT(&fc->devices);

/* Initialize Async handlers */
	STAILQ_INIT(&fc->binds);
	for (i = 0; i < 0x40; i++)
		STAILQ_INIT(&fc->tlabels[i]);

/* DV depend CSRs see blue book */
#if 0
	CSRARC(fc, oMPR) = 0x3fff0001; /* # output channel = 1 */
	CSRARC(fc, oPCR) = 0x8000007a;
	for (i = 4; i < 0x7c/4; i+=4)
		CSRARC(fc, i + oPCR) = 0x8000007a;

	CSRARC(fc, iMPR) = 0x00ff0001; /* # input channel = 1 */
	CSRARC(fc, iPCR) = 0x803f0000;
	for (i = 4; i < 0x7c/4; i+=4)
		CSRARC(fc, i + iPCR) = 0x0;
#endif

	fc->crom_src_buf = NULL;
}

void
fw_destroy(struct firewire_comm *fc)
{
	mutex_destroy(&fc->arq->q_mtx);
	mutex_destroy(&fc->ars->q_mtx);
	mutex_destroy(&fc->atq->q_mtx);
	mutex_destroy(&fc->ats->q_mtx);
}

#define BIND_CMP(addr, fwb) \
	(((addr) < (fwb)->start) ? -1 : ((fwb)->end < (addr)) ? 1 : 0)

/*
 * To lookup bound process from IEEE1394 address.
 */
struct fw_bind *
fw_bindlookup(struct firewire_comm *fc, uint16_t dest_hi, uint32_t dest_lo)
{
	u_int64_t addr;
	struct fw_bind *tfw, *r = NULL;

	addr = ((u_int64_t)dest_hi << 32) | dest_lo;
	mutex_enter(&fc->fc_mtx);
	STAILQ_FOREACH(tfw, &fc->binds, fclist)
		if (BIND_CMP(addr, tfw) == 0) {
			r = tfw;
			break;
		}
	mutex_exit(&fc->fc_mtx);
	return r;
}

/*
 * To bind IEEE1394 address block to process.
 */
int
fw_bindadd(struct firewire_comm *fc, struct fw_bind *fwb)
{
	struct fw_bind *tfw, *prev = NULL;
	int r = 0;

	if (fwb->start > fwb->end) {
		aprint_error_dev(fc->bdev, "invalid range\n");
		return EINVAL;
	}

	mutex_enter(&fc->fc_mtx);
	STAILQ_FOREACH(tfw, &fc->binds, fclist) {
		if (fwb->end < tfw->start)
			break;
		prev = tfw;
	}
	if (prev == NULL)
		STAILQ_INSERT_HEAD(&fc->binds, fwb, fclist);
	else if (prev->end < fwb->start)
		STAILQ_INSERT_AFTER(&fc->binds, prev, fwb, fclist);
	else {
		aprint_error_dev(fc->bdev, "bind failed\n");
		r = EBUSY;
	}
	mutex_exit(&fc->fc_mtx);
	return r;
}

/*
 * To free IEEE1394 address block.
 */
int
fw_bindremove(struct firewire_comm *fc, struct fw_bind *fwb)
{
#if 0
	struct fw_xfer *xfer, *next;
#endif
	struct fw_bind *tfw;

	mutex_enter(&fc->fc_mtx);
	STAILQ_FOREACH(tfw, &fc->binds, fclist)
		if (tfw == fwb) {
			STAILQ_REMOVE(&fc->binds, fwb, fw_bind, fclist);
			mutex_exit(&fc->fc_mtx);
			goto found;
		}

	mutex_exit(&fc->fc_mtx);
	aprint_error_dev(fc->bdev, "no such binding\n");
	return 1;
found:
#if 0
	/* shall we do this? */
	for (xfer = STAILQ_FIRST(&fwb->xferlist); xfer != NULL; xfer = next) {
		next = STAILQ_NEXT(xfer, link);
		fw_xfer_free(xfer);
	}
	STAILQ_INIT(&fwb->xferlist);
#endif

	return 0;
}

int
fw_xferlist_add(struct fw_xferlist *q, struct malloc_type *type, int slen,
		int rlen, int n, struct firewire_comm *fc, void *sc,
		void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	int i;

	for (i = 0; i < n; i++) {
		xfer = fw_xfer_alloc_buf(type, slen, rlen);
		if (xfer == NULL)
			return n;
		xfer->fc = fc;
		xfer->sc = sc;
		xfer->hand = hand;
		STAILQ_INSERT_TAIL(q, xfer, link);
	}
	return n;
}

void
fw_xferlist_remove(struct fw_xferlist *q)
{
	struct fw_xfer *xfer, *next;

	for (xfer = STAILQ_FIRST(q); xfer != NULL; xfer = next) {
		next = STAILQ_NEXT(xfer, link);
		fw_xfer_free_buf(xfer);
	}
	STAILQ_INIT(q);
}

/*
 * To allocate IEEE1394 XFER structure.
 */
struct fw_xfer *
fw_xfer_alloc(struct malloc_type *type)
{
	struct fw_xfer *xfer;

	xfer = malloc(sizeof(struct fw_xfer), type, M_NOWAIT | M_ZERO);
	if (xfer == NULL)
		return xfer;

	xfer->malloc = type;
	cv_init(&xfer->cv, "fwxfer");

	return xfer;
}

struct fw_xfer *
fw_xfer_alloc_buf(struct malloc_type *type, int send_len, int recv_len)
{
	struct fw_xfer *xfer;

	xfer = fw_xfer_alloc(type);
	if (xfer == NULL)
		return NULL;
	xfer->send.pay_len = send_len;
	xfer->recv.pay_len = recv_len;
	if (send_len > 0) {
		xfer->send.payload = malloc(send_len, type, M_NOWAIT | M_ZERO);
		if (xfer->send.payload == NULL) {
			fw_xfer_free(xfer);
			return NULL;
		}
	}
	if (recv_len > 0) {
		xfer->recv.payload = malloc(recv_len, type, M_NOWAIT);
		if (xfer->recv.payload == NULL) {
			if (xfer->send.payload != NULL)
				free(xfer->send.payload, type);
			fw_xfer_free(xfer);
			return NULL;
		}
	}
	return xfer;
}

/*
 * IEEE1394 XFER post process.
 */
void
fw_xfer_done(struct fw_xfer *xfer)
{

	if (xfer->hand == NULL) {
		aprint_error_dev(xfer->fc->bdev, "hand == NULL\n");
		return;
	}

	if (xfer->fc == NULL)
		panic("fw_xfer_done: why xfer->fc is NULL?");

	fw_tl_free(xfer->fc, xfer);
	xfer->hand(xfer);
}

void
fw_xfer_unload(struct fw_xfer* xfer)
{

	if (xfer == NULL)
		return;
	if (xfer->flag & FWXF_INQ) {
		aprint_error_dev(xfer->fc->bdev, "fw_xfer_free FWXF_INQ\n");
		mutex_enter(&xfer->q->q_mtx);
		STAILQ_REMOVE(&xfer->q->q, xfer, fw_xfer, link);
#if 0
		xfer->q->queued--;
#endif
		mutex_exit(&xfer->q->q_mtx);
	}
	if (xfer->fc != NULL) {
#if 1
		if (xfer->flag == FWXF_START)
			/*
			 * This could happen if:
			 *  1. We call fwohci_arcv() before fwohci_txd().
			 *  2. firewire_watch() is called.
			 */
			aprint_error_dev(xfer->fc->bdev,
			    "fw_xfer_free FWXF_START\n");
#endif
	}
	xfer->flag = FWXF_INIT;
	xfer->resp = 0;
}

/*
 * To free IEEE1394 XFER structure.
 */
void
fw_xfer_free(struct fw_xfer* xfer)
{

	if (xfer == NULL) {
		aprint_error("fw_xfer_free: xfer == NULL\n");
		return;
	}
	fw_xfer_unload(xfer);
	cv_destroy(&xfer->cv);
	free(xfer, xfer->malloc);
}

void
fw_xfer_free_buf(struct fw_xfer* xfer)
{

	if (xfer == NULL) {
		aprint_error("fw_xfer_free_buf: xfer == NULL\n");
		return;
	}
	fw_xfer_unload(xfer);
	if (xfer->send.payload != NULL) {
		free(xfer->send.payload, xfer->malloc);
	}
	if (xfer->recv.payload != NULL) {
		free(xfer->recv.payload, xfer->malloc);
	}
	cv_destroy(&xfer->cv);
	free(xfer, xfer->malloc);
}

void
fw_asy_callback_free(struct fw_xfer *xfer)
{

#if 0
	printf("asyreq done flag=%d resp=%d\n", xfer->flag, xfer->resp);
#endif
	fw_xfer_free(xfer);
}

/*
 * To receive self ID.
 */
void
fw_sidrcv(struct firewire_comm* fc, uint32_t *sid, u_int len)
{
	uint32_t *p;
	union fw_self_id *self_id;
	u_int i, j, node, c_port = 0, i_branch = 0;

	fc->sid_cnt = len / (sizeof(uint32_t) * 2);
	fc->max_node = fc->nodeid & 0x3f;
	CSRARC(fc, NODE_IDS) = ((uint32_t)fc->nodeid) << 16;
	fc->status = FWBUSCYMELECT;
	fc->topology_map->crc_len = 2;
	fc->topology_map->generation++;
	fc->topology_map->self_id_count = 0;
	fc->topology_map->node_count = 0;
	fc->speed_map->generation++;
	fc->speed_map->crc_len = 1 + (64*64 + 3) / 4;
	self_id = fc->topology_map->self_id;
	for (i = 0; i < fc->sid_cnt; i++) {
		if (sid[1] != ~sid[0]) {
			aprint_error_dev(fc->bdev,
			    "ERROR invalid self-id packet\n");
			sid += 2;
			continue;
		}
		*self_id = *((union fw_self_id *)sid);
		fc->topology_map->crc_len++;
		if (self_id->p0.sequel == 0) {
			fc->topology_map->node_count++;
			c_port = 0;
			if (firewire_debug)
				fw_print_sid(sid[0]);
			node = self_id->p0.phy_id;
			if (fc->max_node < node)
				fc->max_node = self_id->p0.phy_id;
			/* XXX I'm not sure this is the right speed_map */
			fc->speed_map->speed[node][node] =
			    self_id->p0.phy_speed;
			for (j = 0; j < node; j++)
				fc->speed_map->speed[j][node] =
				    fc->speed_map->speed[node][j] =
				    min(fc->speed_map->speed[j][j],
							self_id->p0.phy_speed);
			if ((fc->irm == -1 || self_id->p0.phy_id > fc->irm) &&
			    (self_id->p0.link_active && self_id->p0.contender))
				fc->irm = self_id->p0.phy_id;
			if (self_id->p0.port0 >= 0x2)
				c_port++;
			if (self_id->p0.port1 >= 0x2)
				c_port++;
			if (self_id->p0.port2 >= 0x2)
				c_port++;
		}
		if (c_port > 2)
			i_branch += (c_port - 2);
		sid += 2;
		self_id++;
		fc->topology_map->self_id_count++;
	}
	/* CRC */
	fc->topology_map->crc =
	    fw_crc16((uint32_t *)&fc->topology_map->generation,
						fc->topology_map->crc_len * 4);
	fc->speed_map->crc = fw_crc16((uint32_t *)&fc->speed_map->generation,
	    fc->speed_map->crc_len * 4);
	/* byteswap and copy to CSR */
	p = (uint32_t *)fc->topology_map;
	for (i = 0; i <= fc->topology_map->crc_len; i++)
		CSRARC(fc, TOPO_MAP + i * 4) = htonl(*p++);
	p = (uint32_t *)fc->speed_map;
	CSRARC(fc, SPED_MAP) = htonl(*p++);
	CSRARC(fc, SPED_MAP + 4) = htonl(*p++);
	/* don't byte-swap uint8_t array */
	memcpy(&CSRARC(fc, SPED_MAP + 8), p, (fc->speed_map->crc_len - 1) * 4);

	fc->max_hop = fc->max_node - i_branch;
	aprint_normal_dev(fc->bdev, "%d nodes, maxhop <= %d %s irm(%d)%s\n",
	    fc->max_node + 1, fc->max_hop,
	    (fc->irm == -1) ? "Not IRM capable" : "cable IRM",
	    fc->irm,
	    (fc->irm == fc->nodeid) ? " (me)" : "");

	if (try_bmr && (fc->irm != -1) && (CSRARC(fc, BUS_MGR_ID) == 0x3f)) {
		if (fc->irm == fc->nodeid) {
			fc->status = FWBUSMGRDONE;
			CSRARC(fc, BUS_MGR_ID) = fc->set_bmr(fc, fc->irm);
			fw_bmr(fc);
		} else {
			fc->status = FWBUSMGRELECT;
			callout_schedule(&fc->bmr_callout, hz/8);
		}
	} else
		fc->status = FWBUSMGRDONE;

	callout_schedule(&fc->busprobe_callout, hz/4);
}

/*
 * Generic packet receiving process.
 */
void
fw_rcv(struct fw_rcv_buf *rb)
{
	struct fw_pkt *fp, *resfp;
	struct fw_bind *bind;
	int tcode;
	int i, len, oldstate;
#if 0
	{
		uint32_t *qld;
		int i;
		qld = (uint32_t *)buf;
		printf("spd %d len:%d\n", spd, len);
		for (i = 0; i <= len && i < 32; i+= 4) {
			printf("0x%08x ", ntohl(qld[i/4]));
			if ((i % 16) == 15) printf("\n");
		}
		if ((i % 16) != 15) printf("\n");
	}
#endif
	fp = (struct fw_pkt *)rb->vec[0].iov_base;
	tcode = fp->mode.common.tcode;
	switch (tcode) {
	case FWTCODE_WRES:
	case FWTCODE_RRESQ:
	case FWTCODE_RRESB:
	case FWTCODE_LRES:
		rb->xfer = fw_tl2xfer(rb->fc, fp->mode.hdr.src,
		    fp->mode.hdr.tlrt >> 2, tcode);
		if (rb->xfer == NULL) {
			aprint_error_dev(rb->fc->bdev, "unknown response"
			    " %s(%x) src=0x%x tl=0x%x rt=%d data=0x%x\n",
			    tcode_str[tcode], tcode,
			    fp->mode.hdr.src,
			    fp->mode.hdr.tlrt >> 2,
			    fp->mode.hdr.tlrt & 3,
			    fp->mode.rresq.data);
#if 0
			printf("try ad-hoc work around!!\n");
			rb->xfer = fw_tl2xfer(rb->fc, fp->mode.hdr.src,
			    (fp->mode.hdr.tlrt >> 2) ^ 3);
			if (rb->xfer == NULL) {
				printf("no use...\n");
				return;
			}
#else
			return;
#endif
		}
		fw_rcv_copy(rb);
		if (rb->xfer->recv.hdr.mode.wres.rtcode != RESP_CMP)
			rb->xfer->resp = EIO;
		else
			rb->xfer->resp = 0;
		/* make sure the packet is drained in AT queue */
		oldstate = rb->xfer->flag;
		rb->xfer->flag = FWXF_RCVD;
		switch (oldstate) {
		case FWXF_SENT:
			fw_xfer_done(rb->xfer);
			break;
		case FWXF_START:
#if 0
			if (firewire_debug)
				printf("not sent yet tl=%x\n", rb->xfer->tl);
#endif
			break;
		default:
			aprint_error_dev(rb->fc->bdev,
			    "unexpected flag 0x%02x\n", rb->xfer->flag);
		}
		return;
	case FWTCODE_WREQQ:
	case FWTCODE_WREQB:
	case FWTCODE_RREQQ:
	case FWTCODE_RREQB:
	case FWTCODE_LREQ:
		bind = fw_bindlookup(rb->fc, fp->mode.rreqq.dest_hi,
		    fp->mode.rreqq.dest_lo);
		if (bind == NULL) {
#if 1
			aprint_error_dev(rb->fc->bdev, "Unknown service addr"
			    " 0x%04x:0x%08x %s(%x) src=0x%x data=%x\n",
			    fp->mode.wreqq.dest_hi, fp->mode.wreqq.dest_lo,
			    tcode_str[tcode], tcode,
			    fp->mode.hdr.src, ntohl(fp->mode.wreqq.data));
#endif
			if (rb->fc->status == FWBUSINIT) {
				aprint_error_dev(rb->fc->bdev,
				    "cannot respond(bus reset)!\n");
				return;
			}
			rb->xfer = fw_xfer_alloc(M_FW);
			if (rb->xfer == NULL)
				return;
			rb->xfer->send.spd = rb->spd;
			rb->xfer->send.pay_len = 0;
			resfp = &rb->xfer->send.hdr;
			switch (tcode) {
			case FWTCODE_WREQQ:
			case FWTCODE_WREQB:
				resfp->mode.hdr.tcode = FWTCODE_WRES;
				break;
			case FWTCODE_RREQQ:
				resfp->mode.hdr.tcode = FWTCODE_RRESQ;
				break;
			case FWTCODE_RREQB:
				resfp->mode.hdr.tcode = FWTCODE_RRESB;
				break;
			case FWTCODE_LREQ:
				resfp->mode.hdr.tcode = FWTCODE_LRES;
				break;
			}
			resfp->mode.hdr.dst = fp->mode.hdr.src;
			resfp->mode.hdr.tlrt = fp->mode.hdr.tlrt;
			resfp->mode.hdr.pri = fp->mode.hdr.pri;
			resfp->mode.rresb.rtcode = RESP_ADDRESS_ERROR;
			resfp->mode.rresb.extcode = 0;
			resfp->mode.rresb.len = 0;
/*
			rb->xfer->hand = fw_xferwake;
*/
			rb->xfer->hand = fw_xfer_free;
			if (fw_asyreq(rb->fc, -1, rb->xfer)) {
				fw_xfer_free(rb->xfer);
				return;
			}
			return;
		}
		len = 0;
		for (i = 0; i < rb->nvec; i++)
			len += rb->vec[i].iov_len;
		mutex_enter(&bind->fwb_mtx);
		rb->xfer = STAILQ_FIRST(&bind->xferlist);
		if (rb->xfer == NULL) {
			mutex_exit(&bind->fwb_mtx);
#if 1
			aprint_error_dev(rb->fc->bdev,
			    "Discard a packet for this bind.\n");
#endif
			return;
		}
		STAILQ_REMOVE_HEAD(&bind->xferlist, link);
		mutex_exit(&bind->fwb_mtx);
		fw_rcv_copy(rb);
		rb->xfer->hand(rb->xfer);
		return;

	default:
		aprint_error_dev(rb->fc->bdev, "unknow tcode %d\n", tcode);
		break;
	}
}

/*
 * CRC16 check-sum for IEEE1394 register blocks.
 */
uint16_t
fw_crc16(uint32_t *ptr, uint32_t len)
{
	uint32_t i, sum, crc = 0;
	int shift;

	len = (len + 3) & ~3;
	for (i = 0; i < len; i+= 4) {
		for (shift = 28; shift >= 0; shift -= 4) {
			sum = ((crc >> 12) ^ (ptr[i/4] >> shift)) & 0xf;
			crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ sum;
		}
		crc &= 0xffff;
	}
	return (uint16_t)crc;
}

int
fw_open_isodma(struct firewire_comm *fc, int tx)
{
	struct fw_xferq **xferqa;
	struct fw_xferq *xferq;
	int i;

	if (tx)
		xferqa = fc->it;
	else
		xferqa = fc->ir;

	mutex_enter(&fc->fc_mtx);
	for (i = 0; i < fc->nisodma; i++) {
		xferq = xferqa[i];
		if (!(xferq->flag & FWXFERQ_OPEN)) {
			xferq->flag |= FWXFERQ_OPEN;
			break;
		}
	}
	if (i == fc->nisodma) {
		aprint_error_dev(fc->bdev, "no free dma channel (tx=%d)\n", tx);
		i = -1;
	}
	mutex_exit(&fc->fc_mtx);
	return i;
}

/*
 * Async. request with given xfer structure.
 */
static void
fw_asystart(struct fw_xfer *xfer)
{
	struct firewire_comm *fc = xfer->fc;

	/* Protect from interrupt/timeout */
	mutex_enter(&xfer->q->q_mtx);
	xfer->flag = FWXF_INQ;
	STAILQ_INSERT_TAIL(&xfer->q->q, xfer, link);
#if 0
	xfer->q->queued++;
#endif
	mutex_exit(&xfer->q->q_mtx);
	/* XXX just queue for mbuf */
	if (xfer->mbuf == NULL)
		xfer->q->start(fc);
	return;
}

static void
firewire_xfer_timeout(struct firewire_comm *fc)
{
	struct fw_xfer *xfer;
	struct timeval tv;
	struct timeval split_timeout;
	STAILQ_HEAD(, fw_xfer) xfer_timeout;
	int i;

	split_timeout.tv_sec = 0;
	split_timeout.tv_usec = 200 * 1000;	 /* 200 msec */

	microtime(&tv);
	timersub(&tv, &split_timeout, &tv);
	STAILQ_INIT(&xfer_timeout);

	mutex_enter(&fc->tlabel_lock);
	for (i = 0; i < 0x40; i++) {
		while ((xfer = STAILQ_FIRST(&fc->tlabels[i])) != NULL) {
			if ((xfer->flag & FWXF_SENT) == 0)
				/* not sent yet */
				break;
			if (timercmp(&xfer->tv, &tv, >))
				/* the rests are newer than this */
				break;
			aprint_error_dev(fc->bdev,
			    "split transaction timeout: tl=0x%x flag=0x%02x\n",
			    i, xfer->flag);
			fw_dump_hdr(&xfer->send.hdr, "send");
			xfer->resp = ETIMEDOUT;
			STAILQ_REMOVE_HEAD(&fc->tlabels[i], tlabel);
			STAILQ_INSERT_TAIL(&xfer_timeout, xfer, tlabel);
		}
	}
	mutex_exit(&fc->tlabel_lock);
	fc->timeout(fc);

	STAILQ_FOREACH(xfer, &xfer_timeout, tlabel)
	    xfer->hand(xfer);
}

#define WATCHDOG_HZ 10
static void
firewire_watchdog(void *arg)
{
	struct firewire_comm *fc;
	static int watchdog_clock = 0;

	fc = (struct firewire_comm *)arg;

	/*
	 * At boot stage, the device interrupt is disabled and
	 * We encounter a timeout easily. To avoid this,
	 * ignore clock interrupt for a while.
	 */
	if (watchdog_clock > WATCHDOG_HZ * 15)
		firewire_xfer_timeout(fc);
	else
		watchdog_clock++;

	callout_schedule(&fc->timeout_callout, hz / WATCHDOG_HZ);
}

static void
fw_xferq_drain(struct fw_xferq *xferq)
{
	struct fw_xfer *xfer;

	while ((xfer = STAILQ_FIRST(&xferq->q)) != NULL) {
		STAILQ_REMOVE_HEAD(&xferq->q, link);
#if 0
		xferq->queued--;
#endif
		xfer->resp = EAGAIN;
		xfer->flag = FWXF_SENTERR;
		fw_xfer_done(xfer);
	}
}

static void
fw_reset_csr(struct firewire_comm *fc)
{
	int i;

	CSRARC(fc, STATE_CLEAR) =
	    1 << 23 | 0 << 17 | 1 << 16 | 1 << 15 | 1 << 14;
	CSRARC(fc, STATE_SET) = CSRARC(fc, STATE_CLEAR);
	CSRARC(fc, NODE_IDS) = 0x3f;

	CSRARC(fc, TOPO_MAP + 8) = 0;
	fc->irm = -1;

	fc->max_node = -1;

	for (i = 2; i < 0x100/4 - 2; i++)
		CSRARC(fc, SPED_MAP + i * 4) = 0;
	CSRARC(fc, STATE_CLEAR) =
	    1 << 23 | 0 << 17 | 1 << 16 | 1 << 15 | 1 << 14;
	CSRARC(fc, STATE_SET) = CSRARC(fc, STATE_CLEAR);
	CSRARC(fc, RESET_START) = 0;
	CSRARC(fc, SPLIT_TIMEOUT_HI) = 0;
	CSRARC(fc, SPLIT_TIMEOUT_LO) = 800 << 19;
	CSRARC(fc, CYCLE_TIME) = 0x0;
	CSRARC(fc, BUS_TIME) = 0x0;
	CSRARC(fc, BUS_MGR_ID) = 0x3f;
	CSRARC(fc, BANDWIDTH_AV) = 4915;
	CSRARC(fc, CHANNELS_AV_HI) = 0xffffffff;
	CSRARC(fc, CHANNELS_AV_LO) = 0xffffffff;
	CSRARC(fc, IP_CHANNELS) = (1 << 31);

	CSRARC(fc, CONF_ROM) = 0x04 << 24;
	CSRARC(fc, CONF_ROM + 4) = 0x31333934; /* means strings 1394 */
	CSRARC(fc, CONF_ROM + 8) =
	    1 << 31 | 1 << 30 | 1 << 29 | 1 << 28 | 0xff << 16 | 0x09 << 8;
	CSRARC(fc, CONF_ROM + 0xc) = 0;

/* DV depend CSRs see blue book */
	CSRARC(fc, oPCR) &= ~DV_BROADCAST_ON;
	CSRARC(fc, iPCR) &= ~DV_BROADCAST_ON;

	CSRARC(fc, STATE_CLEAR) &= ~(1 << 23 | 1 << 15 | 1 << 14);
	CSRARC(fc, STATE_SET) = CSRARC(fc, STATE_CLEAR);
}

static void
fw_init_crom(struct firewire_comm *fc)
{
	struct crom_src *src;

	src = &fc->crom_src_buf->src;
	memset(src, 0, sizeof(struct crom_src));

	/* BUS info sample */
	src->hdr.info_len = 4;

	src->businfo.bus_name = CSR_BUS_NAME_IEEE1394;

	src->businfo.irmc = 1;
	src->businfo.cmc = 1;
	src->businfo.isc = 1;
	src->businfo.bmc = 1;
	src->businfo.pmc = 0;
	src->businfo.cyc_clk_acc = 100;
	src->businfo.max_rec = fc->maxrec;
	src->businfo.max_rom = MAXROM_4;
	src->businfo.generation = FW_GENERATION_CHANGEABLE;
	src->businfo.link_spd = fc->speed;

	src->businfo.eui64.hi = fc->eui.hi;
	src->businfo.eui64.lo = fc->eui.lo;

	STAILQ_INIT(&src->chunk_list);

	fc->crom_src = src;
	fc->crom_root = &fc->crom_src_buf->root;
}

static void
fw_reset_crom(struct firewire_comm *fc)
{
	struct crom_src_buf *buf;
	struct crom_src *src;
	struct crom_chunk *root;

	buf = fc->crom_src_buf;
	src = fc->crom_src;
	root = fc->crom_root;

	STAILQ_INIT(&src->chunk_list);

	memset(root, 0, sizeof(struct crom_chunk));
	crom_add_chunk(src, NULL, root, 0);
	crom_add_entry(root, CSRKEY_NCAP, 0x0083c0); /* XXX */
	/* private company_id */
	crom_add_entry(root, CSRKEY_VENDOR, CSRVAL_VENDOR_PRIVATE);
	crom_add_simple_text(src, root, &buf->vendor, PROJECT_STR);
	crom_add_entry(root, CSRKEY_HW, __NetBSD_Version__);
	crom_add_simple_text(src, root, &buf->hw, hostname);
}

/*
 * dump packet header
 */
static void
fw_dump_hdr(struct fw_pkt *fp, const char *prefix)
{

	printf("%s: dst=0x%02x tl=0x%02x rt=%d tcode=0x%x pri=0x%x "
	    "src=0x%03x\n", prefix,
	     fp->mode.hdr.dst & 0x3f,
	     fp->mode.hdr.tlrt >> 2, fp->mode.hdr.tlrt & 3,
	     fp->mode.hdr.tcode, fp->mode.hdr.pri,
	     fp->mode.hdr.src);
}

/*
 * To free transaction label.
 */
static void
fw_tl_free(struct firewire_comm *fc, struct fw_xfer *xfer)
{
	struct fw_xfer *txfer;

	if (xfer->tl < 0)
		return;

	mutex_enter(&fc->tlabel_lock);
#if 1 /* make sure the label is allocated */
	STAILQ_FOREACH(txfer, &fc->tlabels[xfer->tl], tlabel)
		if (txfer == xfer)
			break;
	if (txfer == NULL) {
		mutex_exit(&fc->tlabel_lock);
		aprint_error_dev(fc->bdev,
		    "the xfer is not in the queue (tlabel=%d, flag=0x%x)\n",
		    xfer->tl, xfer->flag);
		fw_dump_hdr(&xfer->send.hdr, "send");
		fw_dump_hdr(&xfer->recv.hdr, "recv");
		KASSERT(FALSE);
		return;
	}
#endif

	STAILQ_REMOVE(&fc->tlabels[xfer->tl], xfer, fw_xfer, tlabel);
	mutex_exit(&fc->tlabel_lock);
	return;
}

/*
 * To obtain XFER structure by transaction label.
 */
static struct fw_xfer *
fw_tl2xfer(struct firewire_comm *fc, int node, int tlabel, int tcode)
{
	struct fw_xfer *xfer;
	int req;

	mutex_enter(&fc->tlabel_lock);
	STAILQ_FOREACH(xfer, &fc->tlabels[tlabel], tlabel)
		if (xfer->send.hdr.mode.hdr.dst == node) {
			mutex_exit(&fc->tlabel_lock);
			KASSERT(xfer->tl == tlabel);
			/* extra sanity check */
			req = xfer->send.hdr.mode.hdr.tcode;
			if (xfer->fc->tcode[req].valid_res != tcode) {
				aprint_error_dev(fc->bdev,
				    "invalid response tcode (0x%x for 0x%x)\n",
				    tcode, req);
				return NULL;
			}

			if (firewire_debug > 2)
				printf("fw_tl2xfer: found tl=%d\n", tlabel);
			return xfer;
		}
	mutex_exit(&fc->tlabel_lock);
	if (firewire_debug > 1)
		printf("fw_tl2xfer: not found tl=%d\n", tlabel);
	return NULL;
}

/*
 * To configure PHY.
 */
static void
fw_phy_config(struct firewire_comm *fc, int root_node, int gap_count)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	fc->status = FWBUSPHYCONF;

	xfer = fw_xfer_alloc(M_FW);
	if (xfer == NULL)
		return;
	xfer->fc = fc;
	xfer->hand = fw_asy_callback_free;

	fp = &xfer->send.hdr;
	fp->mode.ld[1] = 0;
	if (root_node >= 0)
		fp->mode.ld[1] |= (root_node & 0x3f) << 24 | 1 << 23;
	if (gap_count >= 0)
		fp->mode.ld[1] |= 1 << 22 | (gap_count & 0x3f) << 16;
	fp->mode.ld[2] = ~fp->mode.ld[1];
/* XXX Dangerous, how to pass PHY packet to device driver */
	fp->mode.common.tcode |= FWTCODE_PHY;

	if (firewire_debug)
		printf("root_node=%d gap_count=%d\n", root_node, gap_count);
	fw_asyreq(fc, -1, xfer);
}

/*
 * Dump self ID.
 */
static void
fw_print_sid(uint32_t sid)
{
	union fw_self_id *s;

	s = (union fw_self_id *) &sid;
	if (s->p0.sequel) {
		if (s->p1.sequence_num == FW_SELF_ID_PAGE0)
			printf("node:%d p3:%d p4:%d p5:%d p6:%d p7:%d"
			    "p8:%d p9:%d p10:%d\n",
			    s->p1.phy_id, s->p1.port3, s->p1.port4,
			    s->p1.port5, s->p1.port6, s->p1.port7,
			    s->p1.port8, s->p1.port9, s->p1.port10);
		else if (s->p2.sequence_num == FW_SELF_ID_PAGE1)
			printf("node:%d p11:%d p12:%d p13:%d p14:%d p15:%d\n",
			    s->p2.phy_id, s->p2.port11, s->p2.port12,
			    s->p2.port13, s->p2.port14, s->p2.port15);
		else
			printf("node:%d Unknown Self ID Page number %d\n",
			    s->p1.phy_id, s->p1.sequence_num);
	} else
		printf("node:%d link:%d gap:%d spd:%d con:%d pwr:%d"
		    " p0:%d p1:%d p2:%d i:%d m:%d\n",
		    s->p0.phy_id, s->p0.link_active, s->p0.gap_count,
		    s->p0.phy_speed, s->p0.contender,
		    s->p0.power_class, s->p0.port0, s->p0.port1,
		    s->p0.port2, s->p0.initiated_reset, s->p0.more_packets);
}

/*
 * To probe devices on the IEEE1394 bus.
 */
static void
fw_bus_probe(struct firewire_comm *fc)
{
	struct fw_device *fwdev;

	mutex_enter(&fc->wait_lock);
	fc->status = FWBUSEXPLORE;

	/* Invalidate all devices, just after bus reset. */
	if (firewire_debug)
		printf("iterate and invalidate all nodes\n");
	mutex_enter(&fc->fc_mtx);
	STAILQ_FOREACH(fwdev, &fc->devices, link)
		if (fwdev->status != FWDEVINVAL) {
			fwdev->status = FWDEVINVAL;
			fwdev->rcnt = 0;
			if (firewire_debug)
				printf("Invalidate Dev ID: %08x%08x\n",
				    fwdev->eui.hi, fwdev->eui.lo);
		} else
			if (firewire_debug)
				printf("Dev ID: %08x%08x already invalid\n",
				    fwdev->eui.hi, fwdev->eui.lo);
	mutex_exit(&fc->fc_mtx);

	cv_signal(&fc->fc_cv);
	mutex_exit(&fc->wait_lock);
}

static int
fw_explore_read_quads(struct fw_device *fwdev, int offset, uint32_t *quad,
		      int length)
{
	struct fw_xfer *xfer;
	uint32_t tmp;
	int i, error;

	for (i = 0; i < length; i++, offset += sizeof(uint32_t)) {
		xfer = fwmem_read_quad(fwdev, NULL, -1, 0xffff,
		    0xf0000000 | offset, (void *)&tmp, fw_xferwake);
		if (xfer == NULL)
			return -1;
		fw_xferwait(xfer);

		if (xfer->resp == 0)
			quad[i] = ntohl(tmp);

		error = xfer->resp;
		fw_xfer_free(xfer);
		if (error)
			return error;
	}
	return 0;
}


static int
fw_explore_csrblock(struct fw_device *fwdev, int offset, int recur)
{
	int err, i, off;
	struct csrdirectory *dir;
	struct csrreg *reg;


	dir = (struct csrdirectory *)&fwdev->csrrom[offset/sizeof(uint32_t)];
	err = fw_explore_read_quads(fwdev, CSRROMOFF + offset, (uint32_t *)dir,
	    1);
	if (err)
		return -1;

	offset += sizeof(uint32_t);
	reg = (struct csrreg *)&fwdev->csrrom[offset / sizeof(uint32_t)];
	err = fw_explore_read_quads(fwdev, CSRROMOFF + offset, (uint32_t *)reg,
	    dir->crc_len);
	if (err)
		return -1;

	/* XXX check CRC */

	off = CSRROMOFF + offset + sizeof(uint32_t) * (dir->crc_len - 1);
	if (fwdev->rommax < off)
		fwdev->rommax = off;

	if (recur == 0)
		return 0;

	for (i = 0; i < dir->crc_len; i++, offset += sizeof(uint32_t)) {
		if ((reg[i].key & CSRTYPE_MASK) == CSRTYPE_D)
			recur = 1;
		else if ((reg[i].key & CSRTYPE_MASK) == CSRTYPE_L)
			recur = 0;
		else
			continue;

		off = offset + reg[i].val * sizeof(uint32_t);
		if (off > CROMSIZE) {
			aprint_error_dev(fwdev->fc->bdev, "invalid offset %d\n",
			    off);
			return -1;
		}
		err = fw_explore_csrblock(fwdev, off, recur);
		if (err)
			return -1;
	}
	return 0;
}

static int
fw_explore_node(struct fw_device *dfwdev)
{
	struct firewire_comm *fc;
	struct fw_device *fwdev, *pfwdev, *tfwdev;
	struct csrhdr *hdr;
	struct bus_info *binfo;
	uint32_t *csr, speed_test = 0;
	int err, node;

	fc = dfwdev->fc;
	csr = dfwdev->csrrom;
	node = dfwdev->dst;

	/* First quad */
	err = fw_explore_read_quads(dfwdev, CSRROMOFF, csr, 1);
	if (err) {
		aprint_error_dev(fc->bdev,
		    "node%d: explore_read_quads failure\n", node);
		dfwdev->status = FWDEVINVAL;
		return -1;
	}
	hdr = (struct csrhdr *)csr;
	if (hdr->info_len != 4) {
		if (firewire_debug)
			printf("node%d: wrong bus info len(%d)\n",
			    node, hdr->info_len);
		dfwdev->status = FWDEVINVAL;
		return -1;
	}

	/* bus info */
	err = fw_explore_read_quads(dfwdev, CSRROMOFF + 0x04, &csr[1], 4);
	if (err) {
		aprint_error_dev(fc->bdev, "node%d: error reading 0x04\n",
		    node);
		dfwdev->status = FWDEVINVAL;
		return -1;
	}
	binfo = (struct bus_info *)&csr[1];
	if (binfo->bus_name != CSR_BUS_NAME_IEEE1394) {
		aprint_error_dev(fc->bdev, "node%d: invalid bus name 0x%08x\n",
		    node, binfo->bus_name);
		dfwdev->status = FWDEVINVAL;
		return -1;
	}
	if (firewire_debug)
		printf("node(%d) BUS INFO BLOCK:\n"
		    "irmc(%d) cmc(%d) isc(%d) bmc(%d) pmc(%d) "
		    "cyc_clk_acc(%d) max_rec(%d) max_rom(%d) "
		    "generation(%d) link_spd(%d)\n",
		    node, binfo->irmc, binfo->cmc, binfo->isc,
		    binfo->bmc, binfo->pmc, binfo->cyc_clk_acc,
		    binfo->max_rec, binfo->max_rom,
		    binfo->generation, binfo->link_spd);

	mutex_enter(&fc->fc_mtx);
	STAILQ_FOREACH(fwdev, &fc->devices, link)
		if (FW_EUI64_EQUAL(fwdev->eui, binfo->eui64))
			break;
	mutex_exit(&fc->fc_mtx);
	if (fwdev == NULL) {
		/* new device */
		fwdev =
		    malloc(sizeof(struct fw_device), M_FW, M_NOWAIT | M_ZERO);
		if (fwdev == NULL) {
			if (firewire_debug)
				printf("node%d: no memory\n", node);
			return -1;
		}
		fwdev->fc = fc;
		fwdev->eui = binfo->eui64;
		fwdev->dst = dfwdev->dst;
		fwdev->maxrec = dfwdev->maxrec;
		fwdev->status = FWDEVNEW;
		/*
		 * Pre-1394a-2000 didn't have link_spd in
		 * the Bus Info block, so try and use the 
		 * speed map value.
		 * 1394a-2000 compliant devices only use
		 * the Bus Info Block link spd value, so
		 * ignore the speed map alltogether. SWB
		 */
		if (binfo->link_spd == FWSPD_S100 /* 0 */) {
			aprint_normal_dev(fc->bdev,
			    "Pre 1394a-2000 detected\n");
			fwdev->speed = fc->speed_map->speed[fc->nodeid][node];
		} else
			fwdev->speed = binfo->link_spd;
		/*
		 * Test this speed with a read to the CSRROM.
		 * If it fails, slow down the speed and retry.
		 */
		while (fwdev->speed > FWSPD_S100 /* 0 */) {
			err = fw_explore_read_quads(fwdev, CSRROMOFF,
			    &speed_test, 1);
			if (err) {
				aprint_error_dev(fc->bdev, "fwdev->speed(%s)"
				    " decremented due to negotiation\n",
				    fw_linkspeed[fwdev->speed]);
				fwdev->speed--;
			} else
				break;
		}
		/*
		 * If the fwdev is not found in the
		 * fc->devices TAILQ, then we will add it.
		 */
		pfwdev = NULL;
		mutex_enter(&fc->fc_mtx);
		STAILQ_FOREACH(tfwdev, &fc->devices, link) {
			if (tfwdev->eui.hi > fwdev->eui.hi ||
			    (tfwdev->eui.hi == fwdev->eui.hi &&
						tfwdev->eui.lo > fwdev->eui.lo))
				break;
			pfwdev = tfwdev;
		}
		if (pfwdev == NULL)
			STAILQ_INSERT_HEAD(&fc->devices, fwdev, link);
		else
			STAILQ_INSERT_AFTER(&fc->devices, pfwdev, fwdev, link);
		mutex_exit(&fc->fc_mtx);

		aprint_normal_dev(fc->bdev, "New %s device ID:%08x%08x\n",
		    fw_linkspeed[fwdev->speed], fwdev->eui.hi, fwdev->eui.lo);
	} else {
		fwdev->dst = node;
		fwdev->status = FWDEVINIT;
		/* unchanged ? */
		if (memcmp(csr, fwdev->csrrom, sizeof(uint32_t) * 5) == 0) {
			if (firewire_debug)
				printf("node%d: crom unchanged\n", node);
			return 0;
		}
	}

	memset(fwdev->csrrom, 0, CROMSIZE);

	/* copy first quad and bus info block */
	memcpy(fwdev->csrrom, csr, sizeof(uint32_t) * 5);
	fwdev->rommax = CSRROMOFF + sizeof(uint32_t) * 4;

	err = fw_explore_csrblock(fwdev, 0x14, 1); /* root directory */

	if (err) {
		if (firewire_debug)
			printf("explore csrblock failed err(%d)\n", err);
		fwdev->status = FWDEVINVAL;
		fwdev->csrrom[0] = 0;
	}
	return err;
}

/*
 * Find the self_id packet for a node, ignoring sequels.
 */
static union fw_self_id *
fw_find_self_id(struct firewire_comm *fc, int node)
{
	uint32_t i;
	union fw_self_id *s;

	for (i = 0; i < fc->topology_map->self_id_count; i++) {
		s = &fc->topology_map->self_id[i];
		if (s->p0.sequel)
			continue;
		if (s->p0.phy_id == node)
			return s;
	}
	return 0;
}

static void
fw_explore(struct firewire_comm *fc)
{
	struct fw_device *dfwdev;
	union fw_self_id *fwsid;
	int node, err, i, todo, todo2, trys;
	char nodes[63];

	todo = 0;
	dfwdev = malloc(sizeof(*dfwdev), M_TEMP, M_NOWAIT);
	if (dfwdev == NULL)
		return;
	/* setup dummy fwdev */
	dfwdev->fc = fc;
	dfwdev->speed = 0;
	dfwdev->maxrec = 8; /* 512 */
	dfwdev->status = FWDEVINIT;

	for (node = 0; node <= fc->max_node; node++) {
		/* We don't probe myself and linkdown nodes */
		if (node == fc->nodeid) {
			if (firewire_debug)
				printf("found myself node(%d) fc->nodeid(%d)"
				    " fc->max_node(%d)\n",
				    node, fc->nodeid, fc->max_node);
			continue;
		} else if (firewire_debug)
			printf("node(%d) fc->max_node(%d) found\n",
			    node, fc->max_node);
		fwsid = fw_find_self_id(fc, node);
		if (!fwsid || !fwsid->p0.link_active) {
			if (firewire_debug)
				printf("node%d: link down\n", node);
			continue;
		}
		nodes[todo++] = node;
	}

	for (trys = 0; todo > 0 && trys < 3; trys++) {
		todo2 = 0;
		for (i = 0; i < todo; i++) {
			dfwdev->dst = nodes[i];
			err = fw_explore_node(dfwdev);
			if (err)
				nodes[todo2++] = nodes[i];
			if (firewire_debug)
				printf("node %d, err = %d\n", nodes[i], err);
		}
		todo = todo2;
	}
	free(dfwdev, M_TEMP);
}

static void
fw_bus_probe_thread(void *arg)
{
	struct firewire_comm *fc = (struct firewire_comm *)arg;

	/*
	 * Tell config we've scanned the bus.
	 *
	 * XXX This is not right -- we haven't actually scanned it.  We
	 * probably ought to call this after the first bus exploration.
	 *
	 * bool once = false;
	 * ...
	 * 	fw_attach_dev(fc);
	 * 	if (!once) {
	 * 		config_pending_decr();
	 * 		once = true;
	 * 	}
	 */
	config_pending_decr(fc->bdev);

	mutex_enter(&fc->wait_lock);
	while (fc->status != FWBUSDETACH) {
		if (fc->status == FWBUSEXPLORE) {
			mutex_exit(&fc->wait_lock);
			fw_explore(fc);
			fc->status = FWBUSEXPDONE;
			if (firewire_debug)
				printf("bus_explore done\n");
			fw_attach_dev(fc);
			mutex_enter(&fc->wait_lock);
		}
		cv_wait_sig(&fc->fc_cv, &fc->wait_lock);
	}
	fc->status = FWBUSDETACHOK;
	cv_signal(&fc->fc_cv);
	mutex_exit(&fc->wait_lock);
	kthread_exit(0);

	/* NOTREACHED */
}

static const char *
fw_get_devclass(struct fw_device *fwdev)
{
	struct crom_context cc;
	struct csrreg *reg;

	crom_init_context(&cc, fwdev->csrrom);
	reg = crom_search_key(&cc, CSRKEY_VER);
	if (reg == NULL)
		return "null";

	switch (reg->val) {
	case CSR_PROTAVC:
		return "av/c";
	case CSR_PROTCAL:
		return "cal";
	case CSR_PROTEHS:
		return "ehs";
	case CSR_PROTHAVI:
		return "havi";
	case CSR_PROTCAM104:
		return "cam104";
	case CSR_PROTCAM120:
		return "cam120";
	case CSR_PROTCAM130:
		return "cam130";
	case CSR_PROTDPP:
		return "printer";
	case CSR_PROTIICP:
		return "iicp";
	case CSRVAL_T10SBP2:
		return "sbp";
	default:
		if (firewire_debug)
			printf("%s: reg->val 0x%x\n",
				__func__, reg->val);
		return "sbp";
	}
}

/*
 * To attach sub-devices layer onto IEEE1394 bus.
 */
static void
fw_attach_dev(struct firewire_comm *fc)
{
	struct firewire_softc *sc = device_private(fc->bdev);
	struct firewire_dev_list *devlist, *elm;
	struct fw_device *fwdev, *next;
	struct firewire_dev_comm *fdc;
	struct fw_attach_args fwa;
	int locs[IEEE1394IFCF_NLOCS];

	fwa.name = "null";
	fwa.fc = fc;

	mutex_enter(&fc->fc_mtx);
	for (fwdev = STAILQ_FIRST(&fc->devices); fwdev != NULL; fwdev = next) {
		next = STAILQ_NEXT(fwdev, link);
		mutex_exit(&fc->fc_mtx);
		switch (fwdev->status) {
		case FWDEVNEW:
			devlist = malloc(sizeof(struct firewire_dev_list),
			    M_DEVBUF, M_NOWAIT);
			if (devlist == NULL) {
				aprint_error_dev(fc->bdev,
				    "memory allocation failed\n");
				break;
			}

			locs[IEEE1394IFCF_EUIHI] = fwdev->eui.hi;
			locs[IEEE1394IFCF_EUILO] = fwdev->eui.lo;

			fwa.name = fw_get_devclass(fwdev);
			fwa.fwdev = fwdev;
			fwdev->dev = config_found_sm_loc(sc->dev, "ieee1394if",
			    locs, &fwa, firewire_print, config_stdsubmatch);
			if (fwdev->dev == NULL) {
				free(devlist, M_DEVBUF);
				break;
			}

			devlist->fwdev = fwdev;
			devlist->dev = fwdev->dev;

			mutex_enter(&fc->fc_mtx);
			if (SLIST_EMPTY(&sc->devlist))
				SLIST_INSERT_HEAD(&sc->devlist, devlist, link);
			else {
				for (elm = SLIST_FIRST(&sc->devlist);
				    SLIST_NEXT(elm, link) != NULL;
				    elm = SLIST_NEXT(elm, link));
				SLIST_INSERT_AFTER(elm, devlist, link);
			}
			mutex_exit(&fc->fc_mtx);

			/* FALLTHROUGH */

		case FWDEVINIT:
		case FWDEVATTACHED:
			fwdev->status = FWDEVATTACHED;
			break;

		case FWDEVINVAL:
			fwdev->rcnt++;
			if (firewire_debug)
				printf("fwdev->rcnt(%d), hold_count(%d)\n",
				    fwdev->rcnt, hold_count);
			break;

		default:
			/* XXX */
			break;
		}
		mutex_enter(&fc->fc_mtx);
	}
	mutex_exit(&fc->fc_mtx);

	SLIST_FOREACH(devlist, &sc->devlist, link) {
		fdc = device_private(devlist->dev);
		if (fdc->post_explore != NULL)
			fdc->post_explore(fdc);
	}

	for (fwdev = STAILQ_FIRST(&fc->devices); fwdev != NULL; fwdev = next) {
		next = STAILQ_NEXT(fwdev, link);
		if (fwdev->rcnt > 0 && fwdev->rcnt > hold_count) {
			/*
			 * Remove devices which have not been seen
			 * for a while.
			 */
			SLIST_FOREACH(devlist, &sc->devlist, link)
				if (devlist->fwdev == fwdev)
					break;

			if (devlist == NULL)
				continue;

			if (devlist->fwdev != fwdev)
				panic("already detached");

			SLIST_REMOVE(&sc->devlist, devlist, firewire_dev_list,
			    link);
			free(devlist, M_DEVBUF);

			if (config_detach(fwdev->dev, DETACH_FORCE) != 0)
				return;

			STAILQ_REMOVE(&fc->devices, fwdev, fw_device, link);
			free(fwdev, M_FW);
		}
	}

	return;
}

/*
 * To allocate unique transaction label.
 */
static int
fw_get_tlabel(struct firewire_comm *fc, struct fw_xfer *xfer)
{
	u_int dst, new_tlabel;
	struct fw_xfer *txfer;

	dst = xfer->send.hdr.mode.hdr.dst & 0x3f;
	mutex_enter(&fc->tlabel_lock);
	new_tlabel = (fc->last_tlabel[dst] + 1) & 0x3f;
	STAILQ_FOREACH(txfer, &fc->tlabels[new_tlabel], tlabel)
		if ((txfer->send.hdr.mode.hdr.dst & 0x3f) == dst)
			break;
	if (txfer == NULL) {
		fc->last_tlabel[dst] = new_tlabel;
		STAILQ_INSERT_TAIL(&fc->tlabels[new_tlabel], xfer, tlabel);
		mutex_exit(&fc->tlabel_lock);
		xfer->tl = new_tlabel;
		xfer->send.hdr.mode.hdr.tlrt = new_tlabel << 2;
		if (firewire_debug > 1)
			printf("fw_get_tlabel: dst=%d tl=%d\n",
			    dst, new_tlabel);
		return new_tlabel;
	}
	mutex_exit(&fc->tlabel_lock);

	if (firewire_debug > 1)
		printf("fw_get_tlabel: no free tlabel\n");
	return -1;
}

static void
fw_rcv_copy(struct fw_rcv_buf *rb)
{
	struct fw_pkt *pkt;
	u_char *p;
	const struct tcode_info *tinfo;
	u_int res, i, len, plen;

	rb->xfer->recv.spd = rb->spd;

	pkt = (struct fw_pkt *)rb->vec->iov_base;
	tinfo = &rb->fc->tcode[pkt->mode.hdr.tcode];

	/* Copy header */
	p = (u_char *)&rb->xfer->recv.hdr;
	memcpy(p, rb->vec->iov_base, tinfo->hdr_len);
	rb->vec->iov_base = (u_char *)rb->vec->iov_base + tinfo->hdr_len;
	rb->vec->iov_len -= tinfo->hdr_len;

	/* Copy payload */
	p = (u_char *)rb->xfer->recv.payload;
	res = rb->xfer->recv.pay_len;

	/* special handling for RRESQ */
	if (pkt->mode.hdr.tcode == FWTCODE_RRESQ &&
	    p != NULL && res >= sizeof(uint32_t)) {
		*(uint32_t *)p = pkt->mode.rresq.data;
		rb->xfer->recv.pay_len = sizeof(uint32_t);
		return;
	}

	if ((tinfo->flag & FWTI_BLOCK_ASY) == 0)
		return;

	plen = pkt->mode.rresb.len;

	for (i = 0; i < rb->nvec; i++, rb->vec++) {
		len = MIN(rb->vec->iov_len, plen);
		if (res < len) {
			aprint_error_dev(rb->fc->bdev,
			    "rcv buffer(%d) is %d bytes short.\n",
			    rb->xfer->recv.pay_len, len - res);
			len = res;
		}
		if (p) {
			memcpy(p, rb->vec->iov_base, len);
			p += len;
		}
		res -= len;
		plen -= len;
		if (res == 0 || plen == 0)
			break;
	}
	rb->xfer->recv.pay_len -= res;

}

/*
 * Post process for Bus Manager election process.
 */
static void
fw_try_bmr_callback(struct fw_xfer *xfer)
{
	struct firewire_comm *fc;
	int bmr;

	if (xfer == NULL)
		return;
	fc = xfer->fc;
	if (xfer->resp != 0)
		goto error;
	if (xfer->recv.payload == NULL)
		goto error;
	if (xfer->recv.hdr.mode.lres.rtcode != FWRCODE_COMPLETE)
		goto error;

	bmr = ntohl(xfer->recv.payload[0]);
	if (bmr == 0x3f)
		bmr = fc->nodeid;

	CSRARC(fc, BUS_MGR_ID) = fc->set_bmr(fc, bmr & 0x3f);
	fw_xfer_free_buf(xfer);
	fw_bmr(fc);
	return;

error:
	aprint_error_dev(fc->bdev, "bus manager election failed\n");
	fw_xfer_free_buf(xfer);
}


/*
 * To candidate Bus Manager election process.
 */
static void
fw_try_bmr(void *arg)
{
	struct fw_xfer *xfer;
	struct firewire_comm *fc = (struct firewire_comm *)arg;
	struct fw_pkt *fp;
	int err = 0;

	xfer = fw_xfer_alloc_buf(M_FW, 8, 4);
	if (xfer == NULL)
		return;
	xfer->send.spd = 0;
	fc->status = FWBUSMGRELECT;

	fp = &xfer->send.hdr;
	fp->mode.lreq.dest_hi = 0xffff;
	fp->mode.lreq.tlrt = 0;
	fp->mode.lreq.tcode = FWTCODE_LREQ;
	fp->mode.lreq.pri = 0;
	fp->mode.lreq.src = 0;
	fp->mode.lreq.len = 8;
	fp->mode.lreq.extcode = EXTCODE_CMP_SWAP;
	fp->mode.lreq.dst = FWLOCALBUS | fc->irm;
	fp->mode.lreq.dest_lo = 0xf0000000 | BUS_MGR_ID;
	xfer->send.payload[0] = htonl(0x3f);
	xfer->send.payload[1] = htonl(fc->nodeid);
	xfer->hand = fw_try_bmr_callback;

	err = fw_asyreq(fc, -1, xfer);
	if (err) {
		fw_xfer_free_buf(xfer);
		return;
	}
	return;
}

/*
 * Find the root node, if it is not
 * Cycle Master Capable, then we should
 * override this and become the Cycle
 * Master
 */
static int
fw_bmr(struct firewire_comm *fc)
{
	struct fw_device fwdev;
	union fw_self_id *self_id;
	int cmstr;
	uint32_t quad;

	/* Check to see if the current root node is cycle master capable */
	self_id = fw_find_self_id(fc, fc->max_node);
	if (fc->max_node > 0) {
		/* XXX check cmc bit of businfo block rather than contender */
		if (self_id->p0.link_active && self_id->p0.contender)
			cmstr = fc->max_node;
		else {
			aprint_normal_dev(fc->bdev,
				"root node is not cycle master capable\n");
			/* XXX shall we be the cycle master? */
			cmstr = fc->nodeid;
			/* XXX need bus reset */
		}
	} else
		cmstr = -1;

	aprint_normal_dev(fc->bdev, "bus manager %d%s\n",
	    CSRARC(fc, BUS_MGR_ID),
	    (CSRARC(fc, BUS_MGR_ID) != fc->nodeid) ? " (me)" : "");
	if (CSRARC(fc, BUS_MGR_ID) != fc->nodeid)
		/* We are not the bus manager */
		return 0;

	/* Optimize gapcount */
	if (fc->max_hop <= MAX_GAPHOP)
		fw_phy_config(fc, cmstr, gap_cnt[fc->max_hop]);
	/* If we are the cycle master, nothing to do */
	if (cmstr == fc->nodeid || cmstr == -1)
		return 0;
	/* Bus probe has not finished, make dummy fwdev for cmstr */
	memset(&fwdev, 0, sizeof(fwdev));
	fwdev.fc = fc;
	fwdev.dst = cmstr;
	fwdev.speed = 0;
	fwdev.maxrec = 8; /* 512 */
	fwdev.status = FWDEVINIT;
	/* Set cmstr bit on the cycle master */
	quad = htonl(1 << 8);
	fwmem_write_quad(&fwdev, NULL, 0/*spd*/, 0xffff, 0xf0000000 | STATE_SET,
	    &quad, fw_asy_callback_free);

	return 0;
}
