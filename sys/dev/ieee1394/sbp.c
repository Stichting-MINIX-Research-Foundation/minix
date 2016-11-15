/*	$NetBSD: sbp.c,v 1.36 2014/02/25 18:30:09 pooka Exp $	*/
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
 * $FreeBSD: src/sys/dev/firewire/sbp.c,v 1.100 2009/02/18 18:41:34 sbruno Exp $
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sbp.c,v 1.36 2014/02/25 18:30:09 pooka Exp $");


#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <sys/bus.h>

#include <dev/scsipi/scsi_spc.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsipiconf.h>

#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwdma.h>
#include <dev/ieee1394/iec13213.h>
#include <dev/ieee1394/sbp.h>

#include "locators.h"


#define SBP_FWDEV_ALIVE(fwdev) (((fwdev)->status == FWDEVATTACHED) \
	&& crom_has_specver((fwdev)->csrrom, CSRVAL_ANSIT10, CSRVAL_T10SBP2))

#define SBP_NUM_TARGETS	8 /* MAX 64 */
#define SBP_NUM_LUNS	64
#define SBP_MAXPHYS	MIN(MAXPHYS, (512*1024) /* 512KB */)
#define SBP_DMA_SIZE	PAGE_SIZE
#define SBP_LOGIN_SIZE	sizeof(struct sbp_login_res)
#define SBP_QUEUE_LEN ((SBP_DMA_SIZE - SBP_LOGIN_SIZE) / sizeof(struct sbp_ocb))
#define SBP_NUM_OCB	(SBP_QUEUE_LEN * SBP_NUM_TARGETS)

/*
 * STATUS FIFO addressing
 *   bit
 * -----------------------
 *  0- 1( 2): 0 (alignment)
 *  2- 9( 8): lun
 * 10-31(14): unit
 * 32-47(16): SBP_BIND_HI
 * 48-64(16): bus_id, node_id
 */
#define SBP_BIND_HI 0x1
#define SBP_DEV2ADDR(u, l)		 \
	(((uint64_t)SBP_BIND_HI << 32)	|\
	 (((u) & 0x3fff) << 10)		|\
	 (((l) & 0xff) << 2))
#define SBP_ADDR2UNIT(a)	(((a) >> 10) & 0x3fff)
#define SBP_ADDR2LUN(a)		(((a) >> 2) & 0xff)
#define SBP_INITIATOR 7

static const char *orb_fun_name[] = {
	ORB_FUN_NAMES
};

static int debug = 0;
static int auto_login = 1;
static int max_speed = -1;
static int sbp_cold = 1;
static int ex_login = 1;
static int login_delay = 1000;	/* msec */
static int scan_delay = 500;	/* msec */
static int use_doorbell = 0;
static int sbp_tags = 0;

static int sysctl_sbp_verify(SYSCTLFN_PROTO, int lower, int upper);
static int sysctl_sbp_verify_max_speed(SYSCTLFN_PROTO);
static int sysctl_sbp_verify_tags(SYSCTLFN_PROTO);

/*
 * Setup sysctl(3) MIB, hw.sbp.*
 *
 * TBD condition CTLFLAG_PERMANENT on being a module or not
 */
SYSCTL_SETUP(sysctl_sbp, "sysctl sbp(4) subtree setup")
{
	int rc, sbp_node_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "sbp",
	    SYSCTL_DESCR("sbp controls"), NULL, 0, NULL,
	    0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;
	sbp_node_num = node->sysctl_num;

	/* sbp auto login flag */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "auto_login", SYSCTL_DESCR("SBP perform login automatically"),
	    NULL, 0, &auto_login,
	    0, CTL_HW, sbp_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* sbp max speed */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "max_speed", SYSCTL_DESCR("SBP transfer max speed"),
	    sysctl_sbp_verify_max_speed, 0, &max_speed,
	    0, CTL_HW, sbp_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* sbp exclusive login flag */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "exclusive_login", SYSCTL_DESCR("SBP enable exclusive login"),
	    NULL, 0, &ex_login,
	    0, CTL_HW, sbp_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* sbp login delay */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "login_delay", SYSCTL_DESCR("SBP login delay in msec"),
	    NULL, 0, &login_delay,
	    0, CTL_HW, sbp_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* sbp scan delay */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "scan_delay", SYSCTL_DESCR("SBP scan delay in msec"),
	    NULL, 0, &scan_delay,
	    0, CTL_HW, sbp_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* sbp use doorbell flag */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "use_doorbell", SYSCTL_DESCR("SBP use doorbell request"),
	    NULL, 0, &use_doorbell,
	    0, CTL_HW, sbp_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* sbp force tagged queuing */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "tags", SYSCTL_DESCR("SBP tagged queuing support"),
	    sysctl_sbp_verify_tags, 0, &sbp_tags,
	    0, CTL_HW, sbp_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* sbp driver debug flag */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "sbp_debug", SYSCTL_DESCR("SBP debug flag"),
	    NULL, 0, &debug,
	    0, CTL_HW, sbp_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return;

err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
sysctl_sbp_verify(SYSCTLFN_ARGS, int lower, int upper)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < lower || t > upper)
		return EINVAL;

	*(int*)rnode->sysctl_data = t;

	return 0;
}

static int
sysctl_sbp_verify_max_speed(SYSCTLFN_ARGS)
{

	return sysctl_sbp_verify(SYSCTLFN_CALL(rnode), 0, FWSPD_S400);
}

static int
sysctl_sbp_verify_tags(SYSCTLFN_ARGS)
{

	return sysctl_sbp_verify(SYSCTLFN_CALL(rnode), -1, 1);
}

#define NEED_RESPONSE 0

#define SBP_SEG_MAX rounddown(0xffff, PAGE_SIZE)
#ifdef __sparc64__ /* iommu */
#define SBP_IND_MAX howmany(SBP_MAXPHYS, SBP_SEG_MAX)
#else
#define SBP_IND_MAX howmany(SBP_MAXPHYS, PAGE_SIZE)
#endif
struct sbp_ocb {
	uint32_t	orb[8];
#define IND_PTR_OFFSET	(sizeof(uint32_t) * 8)
	struct ind_ptr	ind_ptr[SBP_IND_MAX];
	struct scsipi_xfer *xs;
	struct sbp_dev	*sdev;
	uint16_t	index;
	uint16_t	flags; /* XXX should be removed */
	bus_dmamap_t	dmamap;
	bus_addr_t	bus_addr;
	STAILQ_ENTRY(sbp_ocb)	ocb;
};

#define SBP_ORB_DMA_SYNC(dma, i, op)			\
	bus_dmamap_sync((dma).dma_tag, (dma).dma_map,	\
	    sizeof(struct sbp_ocb) * (i),		\
	    sizeof(ocb->orb) + sizeof(ocb->ind_ptr), (op));

#define OCB_ACT_MGM 0
#define OCB_ACT_CMD 1
#define OCB_MATCH(o,s)	((o)->bus_addr == ntohl((s)->orb_lo))

struct sbp_dev{
#define SBP_DEV_RESET		0	/* accept login */
#define SBP_DEV_LOGIN		1	/* to login */
#if 0
#define SBP_DEV_RECONN		2	/* to reconnect */
#endif
#define SBP_DEV_TOATTACH	3	/* to attach */
#define SBP_DEV_PROBE		4	/* scan lun */
#define SBP_DEV_ATTACHED	5	/* in operation */
#define SBP_DEV_DEAD		6	/* unavailable unit */
#define SBP_DEV_RETRY		7	/* unavailable unit */
	uint8_t status:4,
		 timeout:4;
	uint8_t type;
	uint16_t lun_id;
	uint16_t freeze;
#define	ORB_LINK_DEAD		(1 << 0)
#define	VALID_LUN		(1 << 1)
#define	ORB_POINTER_ACTIVE	(1 << 2)
#define	ORB_POINTER_NEED	(1 << 3)
#define	ORB_DOORBELL_ACTIVE	(1 << 4)
#define	ORB_DOORBELL_NEED	(1 << 5)
#define	ORB_SHORTAGE		(1 << 6)
	uint16_t flags;
	struct scsipi_periph *periph;
	struct sbp_target *target;
	struct fwdma_alloc dma;
	struct sbp_login_res *login;
	struct callout login_callout;
	struct sbp_ocb *ocb;
	STAILQ_HEAD(, sbp_ocb) ocbs;
	STAILQ_HEAD(, sbp_ocb) free_ocbs;
	struct sbp_ocb *last_ocb;
	char vendor[32];
	char product[32];
	char revision[10];
	char bustgtlun[32];
};

struct sbp_target {
	int target_id;
	int num_lun;
	struct sbp_dev	**luns;
	struct sbp_softc *sbp;
	struct fw_device *fwdev;
	uint32_t mgm_hi, mgm_lo;
	struct sbp_ocb *mgm_ocb_cur;
	STAILQ_HEAD(, sbp_ocb) mgm_ocb_queue;
	struct callout mgm_ocb_timeout;
	STAILQ_HEAD(, fw_xfer) xferlist;
	int n_xfer;
};

struct sbp_softc {
	struct firewire_dev_comm sc_fd;
	struct scsipi_adapter sc_adapter;
	struct scsipi_channel sc_channel;
	device_t sc_bus;
	struct lwp *sc_lwp;
	struct sbp_target sc_target;
	struct fw_bind sc_fwb;
	bus_dma_tag_t sc_dmat;
	struct timeval sc_last_busreset;
	int sc_flags;
	kmutex_t sc_mtx;
	kcondvar_t sc_cv;
};

MALLOC_DEFINE(M_SBP, "sbp", "SBP-II/IEEE1394");
MALLOC_DECLARE(M_SBP);


static int sbpmatch(device_t, cfdata_t, void *);
static void sbpattach(device_t, device_t, void *);
static int sbpdetach(device_t, int);

static void sbp_scsipi_request(struct scsipi_channel *, scsipi_adapter_req_t,
			       void *);
static void sbp_minphys(struct buf *);

static void sbp_show_sdev_info(struct sbp_dev *);
static void sbp_alloc_lun(struct sbp_target *);
static struct sbp_target *sbp_alloc_target(struct sbp_softc *,
					   struct fw_device *);
static void sbp_probe_lun(struct sbp_dev *);
static void sbp_login_callout(void *);
static void sbp_login(struct sbp_dev *);
static void sbp_probe_target(void *);
static void sbp_post_busreset(void *);
static void sbp_post_explore(void *);
#if NEED_RESPONSE
static void sbp_loginres_callback(struct fw_xfer *);
#endif
static inline void sbp_xfer_free(struct fw_xfer *);
static void sbp_reset_start_callback(struct fw_xfer *);
static void sbp_reset_start(struct sbp_dev *);
static void sbp_mgm_callback(struct fw_xfer *);
static void sbp_scsipi_scan_target(void *);
static inline void sbp_scan_dev(struct sbp_dev *);
static void sbp_do_attach(struct fw_xfer *);
static void sbp_agent_reset_callback(struct fw_xfer *);
static void sbp_agent_reset(struct sbp_dev *);
static void sbp_busy_timeout_callback(struct fw_xfer *);
static void sbp_busy_timeout(struct sbp_dev *);
static void sbp_orb_pointer_callback(struct fw_xfer *);
static void sbp_orb_pointer(struct sbp_dev *, struct sbp_ocb *);
static void sbp_doorbell_callback(struct fw_xfer *);
static void sbp_doorbell(struct sbp_dev *);
static struct fw_xfer *sbp_write_cmd(struct sbp_dev *, int, int);
static void sbp_mgm_orb(struct sbp_dev *, int, struct sbp_ocb *);
static void sbp_print_scsi_cmd(struct sbp_ocb *);
static void sbp_scsi_status(struct sbp_status *, struct sbp_ocb *);
static void sbp_fix_inq_data(struct sbp_ocb *);
static void sbp_recv(struct fw_xfer *);
static int sbp_logout_all(struct sbp_softc *);
static void sbp_free_sdev(struct sbp_dev *);
static void sbp_free_target(struct sbp_target *);
static void sbp_scsipi_detach_sdev(struct sbp_dev *);
static void sbp_scsipi_detach_target(struct sbp_target *);
static void sbp_target_reset(struct sbp_dev *, int);
static void sbp_mgm_timeout(void *);
static void sbp_timeout(void *);
static void sbp_action1(struct sbp_softc *, struct scsipi_xfer *);
static void sbp_execute_ocb(struct sbp_ocb *, bus_dma_segment_t *, int);
static struct sbp_ocb *sbp_dequeue_ocb(struct sbp_dev *, struct sbp_status *);
static struct sbp_ocb *sbp_enqueue_ocb(struct sbp_dev *, struct sbp_ocb *);
static struct sbp_ocb *sbp_get_ocb(struct sbp_dev *);
static void sbp_free_ocb(struct sbp_dev *, struct sbp_ocb *);
static void sbp_abort_ocb(struct sbp_ocb *, int);
static void sbp_abort_all_ocbs(struct sbp_dev *, int);


static const char *orb_status0[] = {
	/* 0 */ "No additional information to report",
	/* 1 */ "Request type not supported",
	/* 2 */ "Speed not supported",
	/* 3 */ "Page size not supported",
	/* 4 */ "Access denied",
	/* 5 */ "Logical unit not supported",
	/* 6 */ "Maximum payload too small",
	/* 7 */ "Reserved for future standardization",
	/* 8 */ "Resources unavailable",
	/* 9 */ "Function rejected",
	/* A */ "Login ID not recognized",
	/* B */ "Dummy ORB completed",
	/* C */ "Request aborted",
	/* FF */ "Unspecified error"
#define MAX_ORB_STATUS0 0xd
};

static const char *orb_status1_object[] = {
	/* 0 */ "Operation request block (ORB)",
	/* 1 */ "Data buffer",
	/* 2 */ "Page table",
	/* 3 */ "Unable to specify"
};

static const char *orb_status1_serial_bus_error[] = {
	/* 0 */ "Missing acknowledge",
	/* 1 */ "Reserved; not to be used",
	/* 2 */ "Time-out error",
	/* 3 */ "Reserved; not to be used",
	/* 4 */ "Busy retry limit exceeded(X)",
	/* 5 */ "Busy retry limit exceeded(A)",
	/* 6 */ "Busy retry limit exceeded(B)",
	/* 7 */ "Reserved for future standardization",
	/* 8 */ "Reserved for future standardization",
	/* 9 */ "Reserved for future standardization",
	/* A */ "Reserved for future standardization",
	/* B */ "Tardy retry limit exceeded",
	/* C */ "Conflict error",
	/* D */ "Data error",
	/* E */ "Type error",
	/* F */ "Address error"
};


CFATTACH_DECL_NEW(sbp, sizeof(struct sbp_softc),
    sbpmatch, sbpattach, sbpdetach, NULL);


int
sbpmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct fw_attach_args *fwa = aux;

	if (strcmp(fwa->name, "sbp") == 0)
		return 1;
	return 0;
}

static void
sbpattach(device_t parent, device_t self, void *aux)
{
	struct sbp_softc *sc = device_private(self);
	struct fw_attach_args *fwa = (struct fw_attach_args *)aux;
	struct firewire_comm *fc;
	struct scsipi_adapter *sc_adapter = &sc->sc_adapter;
	struct scsipi_channel *sc_channel = &sc->sc_channel;
	struct sbp_target *target = &sc->sc_target;
	int dv_unit;

	aprint_naive("\n");
	aprint_normal(": SBP-2/SCSI over IEEE1394\n");

	sc->sc_fd.dev = self;

	if (cold)
		sbp_cold++;
	sc->sc_fd.fc = fc = fwa->fc;
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_VM);
	cv_init(&sc->sc_cv, "sbp");

	if (max_speed < 0)
		max_speed = fc->speed;

	sc->sc_dmat = fc->dmat;

	sc->sc_target.fwdev = NULL;
	sc->sc_target.luns = NULL;

	/* Initialize mutexes and lists before we can error out
	 * to prevent crashes on detach
	 */
	mutex_init(&sc->sc_fwb.fwb_mtx, MUTEX_DEFAULT, IPL_VM);
	STAILQ_INIT(&sc->sc_fwb.xferlist);

	if (sbp_alloc_target(sc, fwa->fwdev) == NULL)
		return;

	sc_adapter->adapt_dev = sc->sc_fd.dev;
	sc_adapter->adapt_nchannels = 1;
	sc_adapter->adapt_max_periph = 1;
	sc_adapter->adapt_request = sbp_scsipi_request;
	sc_adapter->adapt_minphys = sbp_minphys;
	sc_adapter->adapt_openings = 8;

	sc_channel->chan_adapter = sc_adapter;
	sc_channel->chan_bustype = &scsi_bustype;
	sc_channel->chan_defquirks = PQUIRK_ONLYBIG;
	sc_channel->chan_channel = 0;
	sc_channel->chan_flags = SCSIPI_CHAN_CANGROW | SCSIPI_CHAN_NOSETTLE;

	sc_channel->chan_ntargets = 1;
	sc_channel->chan_nluns = target->num_lun;	/* We set nluns 0 now */
	sc_channel->chan_id = 1;

	sc->sc_bus = config_found(sc->sc_fd.dev, sc_channel, scsiprint);
	if (sc->sc_bus == NULL) {
		aprint_error_dev(self, "attach failed\n");
		return;
	}

	/* We reserve 16 bit space (4 bytes X 64 unit X 256 luns) */
	dv_unit = device_unit(sc->sc_fd.dev);
	sc->sc_fwb.start = SBP_DEV2ADDR(dv_unit, 0);
	sc->sc_fwb.end = SBP_DEV2ADDR(dv_unit, -1);
	/* pre-allocate xfer */
	fw_xferlist_add(&sc->sc_fwb.xferlist, M_SBP,
	    /*send*/ 0, /*recv*/ SBP_RECV_LEN, SBP_NUM_OCB / 2,
	    fc, (void *)sc, sbp_recv);
	fw_bindadd(fc, &sc->sc_fwb);

	sc->sc_fd.post_busreset = sbp_post_busreset;
	sc->sc_fd.post_explore = sbp_post_explore;

	if (fc->status != FWBUSNOTREADY) {
		sbp_post_busreset((void *)sc);
		sbp_post_explore((void *)sc);
	}
}

static int
sbpdetach(device_t self, int flags)
{
	struct sbp_softc *sc = device_private(self);
	struct firewire_comm *fc = sc->sc_fd.fc;

	sbp_scsipi_detach_target(&sc->sc_target);

	if (sc->sc_target.fwdev && SBP_FWDEV_ALIVE(sc->sc_target.fwdev)) {
		sbp_logout_all(sc);

		/* XXX wait for logout completion */
		mutex_enter(&sc->sc_mtx);
		cv_timedwait_sig(&sc->sc_cv, &sc->sc_mtx, hz/2);
		mutex_exit(&sc->sc_mtx);
	}

	sbp_free_target(&sc->sc_target);

	fw_bindremove(fc, &sc->sc_fwb);
	fw_xferlist_remove(&sc->sc_fwb.xferlist);
	mutex_destroy(&sc->sc_fwb.fwb_mtx);

	mutex_destroy(&sc->sc_mtx);
	cv_destroy(&sc->sc_cv);

	return 0;
}


static void
sbp_scsipi_request(struct scsipi_channel *channel, scsipi_adapter_req_t req,
		   void *arg)
{
	struct sbp_softc *sc = device_private(channel->chan_adapter->adapt_dev);
	struct scsipi_xfer *xs = arg;
	int i;

SBP_DEBUG(1)
	printf("Called sbp_scsipi_request\n");
END_DEBUG

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
SBP_DEBUG(1)
		printf("Got req_run_xfer\n");
		printf("xs control: 0x%08x, timeout: %d\n",
		    xs->xs_control, xs->timeout);
		printf("opcode: 0x%02x\n", (int)xs->cmd->opcode);
		for (i = 0; i < 15; i++)
			printf("0x%02x ",(int)xs->cmd->bytes[i]);
		printf("\n");
END_DEBUG
		if (xs->xs_control & XS_CTL_RESET) {
SBP_DEBUG(1)
				printf("XS_CTL_RESET not support\n");
END_DEBUG
			break;
		}
#define SBPSCSI_SBP2_MAX_CDB 12
		if (xs->cmdlen > SBPSCSI_SBP2_MAX_CDB) {
SBP_DEBUG(0)
			printf(
			    "sbp doesn't support cdb's larger than %d bytes\n",
			    SBPSCSI_SBP2_MAX_CDB);
END_DEBUG
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			return;
		}
		sbp_action1(sc, xs);

		break;
	case ADAPTER_REQ_GROW_RESOURCES:
SBP_DEBUG(1)
		printf("Got req_grow_resources\n");
END_DEBUG
		break;
	case ADAPTER_REQ_SET_XFER_MODE:
SBP_DEBUG(1)
		printf("Got set xfer mode\n");
END_DEBUG
		break;
	default:
		panic("Unknown request: %d\n", (int)req);
	}
}

static void
sbp_minphys(struct buf *bp)
{

	minphys(bp);
}


/*
 * Display device characteristics on the console
 */
static void
sbp_show_sdev_info(struct sbp_dev *sdev)
{
	struct fw_device *fwdev = sdev->target->fwdev;
	struct sbp_softc *sc = sdev->target->sbp;

	aprint_normal_dev(sc->sc_fd.dev,
	    "ordered:%d type:%d EUI:%08x%08x node:%d speed:%d maxrec:%d\n",
	    (sdev->type & 0x40) >> 6,
	    (sdev->type & 0x1f),
	    fwdev->eui.hi,
	    fwdev->eui.lo,
	    fwdev->dst,
	    fwdev->speed,
	    fwdev->maxrec);
	aprint_normal_dev(sc->sc_fd.dev, "%s '%s' '%s' '%s'\n",
	    sdev->bustgtlun, sdev->vendor, sdev->product, sdev->revision);
}

static void
sbp_alloc_lun(struct sbp_target *target)
{
	struct crom_context cc;
	struct csrreg *reg;
	struct sbp_dev *sdev, **newluns;
	struct sbp_softc *sc;
	int maxlun, lun, i;

	sc = target->sbp;
	crom_init_context(&cc, target->fwdev->csrrom);
	/* XXX shoud parse appropriate unit directories only */
	maxlun = -1;
	while (cc.depth >= 0) {
		reg = crom_search_key(&cc, CROM_LUN);
		if (reg == NULL)
			break;
		lun = reg->val & 0xffff;
SBP_DEBUG(0)
		printf("target %d lun %d found\n", target->target_id, lun);
END_DEBUG
		if (maxlun < lun)
			maxlun = lun;
		crom_next(&cc);
	}
	if (maxlun < 0)
		aprint_normal_dev(sc->sc_fd.dev, "%d: no LUN found\n",
		    target->target_id);

	maxlun++;
	if (maxlun >= SBP_NUM_LUNS)
		maxlun = SBP_NUM_LUNS;

	/* Invalidiate stale devices */
	for (lun = 0; lun < target->num_lun; lun++) {
		sdev = target->luns[lun];
		if (sdev == NULL)
			continue;
		sdev->flags &= ~VALID_LUN;
		if (lun >= maxlun) {
			/* lost device */
			sbp_scsipi_detach_sdev(sdev);
			sbp_free_sdev(sdev);
			target->luns[lun] = NULL;
		}
	}

	/* Reallocate */
	if (maxlun != target->num_lun) {
		newluns = (struct sbp_dev **) realloc(target->luns,
		    sizeof(struct sbp_dev *) * maxlun,
		    M_SBP, M_NOWAIT | M_ZERO);

		if (newluns == NULL) {
			aprint_error_dev(sc->sc_fd.dev, "realloc failed\n");
			newluns = target->luns;
			maxlun = target->num_lun;
		}

		/*
		 * We must zero the extended region for the case
		 * realloc() doesn't allocate new buffer.
		 */
		if (maxlun > target->num_lun) {
			const int sbp_dev_p_sz = sizeof(struct sbp_dev *);

			memset(&newluns[target->num_lun], 0,
			    sbp_dev_p_sz * (maxlun - target->num_lun));
		}

		target->luns = newluns;
		target->num_lun = maxlun;
	}

	crom_init_context(&cc, target->fwdev->csrrom);
	while (cc.depth >= 0) {
		int new = 0;

		reg = crom_search_key(&cc, CROM_LUN);
		if (reg == NULL)
			break;
		lun = reg->val & 0xffff;
		if (lun >= SBP_NUM_LUNS) {
			aprint_error_dev(sc->sc_fd.dev, "too large lun %d\n",
			    lun);
			goto next;
		}

		sdev = target->luns[lun];
		if (sdev == NULL) {
			sdev = malloc(sizeof(struct sbp_dev),
			    M_SBP, M_NOWAIT | M_ZERO);
			if (sdev == NULL) {
				aprint_error_dev(sc->sc_fd.dev,
				    "malloc failed\n");
				goto next;
			}
			target->luns[lun] = sdev;
			sdev->lun_id = lun;
			sdev->target = target;
			STAILQ_INIT(&sdev->ocbs);
			callout_init(&sdev->login_callout, CALLOUT_MPSAFE);
			callout_setfunc(&sdev->login_callout,
			    sbp_login_callout, sdev);
			sdev->status = SBP_DEV_RESET;
			new = 1;
			snprintf(sdev->bustgtlun, 32, "%s:%d:%d",
			    device_xname(sc->sc_fd.dev),
			    sdev->target->target_id,
			    sdev->lun_id);
			if (!sc->sc_lwp)
				if (kthread_create(
				    PRI_NONE, KTHREAD_MPSAFE, NULL,
				    sbp_scsipi_scan_target, &sc->sc_target,
				    &sc->sc_lwp,
				    "sbp%d_attach", device_unit(sc->sc_fd.dev)))
					aprint_error_dev(sc->sc_fd.dev,
					    "unable to create thread");
		}
		sdev->flags |= VALID_LUN;
		sdev->type = (reg->val & 0xff0000) >> 16;

		if (new == 0)
			goto next;

		fwdma_alloc_setup(sc->sc_fd.dev, sc->sc_dmat, SBP_DMA_SIZE,
		    &sdev->dma, sizeof(uint32_t), BUS_DMA_NOWAIT);
		if (sdev->dma.v_addr == NULL) {
			free(sdev, M_SBP);
			target->luns[lun] = NULL;
			goto next;
		}
		sdev->ocb = (struct sbp_ocb *)sdev->dma.v_addr;
		sdev->login = (struct sbp_login_res *)&sdev->ocb[SBP_QUEUE_LEN];
		memset((char *)sdev->ocb, 0,
		    sizeof(struct sbp_ocb) * SBP_QUEUE_LEN);

		STAILQ_INIT(&sdev->free_ocbs);
		for (i = 0; i < SBP_QUEUE_LEN; i++) {
			struct sbp_ocb *ocb = &sdev->ocb[i];

			ocb->index = i;
			ocb->bus_addr =
			    sdev->dma.bus_addr + sizeof(struct sbp_ocb) * i;
			if (bus_dmamap_create(sc->sc_dmat, 0x100000,
			    SBP_IND_MAX, SBP_SEG_MAX, 0, 0, &ocb->dmamap)) {
				aprint_error_dev(sc->sc_fd.dev,
				    "cannot create dmamap %d\n", i);
				/* XXX */
				goto next;
			}
			sbp_free_ocb(sdev, ocb);	/* into free queue */
		}
next:
		crom_next(&cc);
	}

	for (lun = 0; lun < target->num_lun; lun++) {
		sdev = target->luns[lun];
		if (sdev != NULL && (sdev->flags & VALID_LUN) == 0) {
			sbp_scsipi_detach_sdev(sdev);
			sbp_free_sdev(sdev);
			target->luns[lun] = NULL;
		}
	}
}

static struct sbp_target *
sbp_alloc_target(struct sbp_softc *sc, struct fw_device *fwdev)
{
	struct sbp_target *target;
	struct crom_context cc;
	struct csrreg *reg;

SBP_DEBUG(1)
	printf("sbp_alloc_target\n");
END_DEBUG
	/* new target */
	target = &sc->sc_target;
	target->sbp = sc;
	target->fwdev = fwdev;
	target->target_id = 0;
	target->mgm_ocb_cur = NULL;
SBP_DEBUG(1)
	printf("target: mgm_port: %x\n", target->mgm_lo);
END_DEBUG
	STAILQ_INIT(&target->xferlist);
	target->n_xfer = 0;
	STAILQ_INIT(&target->mgm_ocb_queue);
	callout_init(&target->mgm_ocb_timeout, CALLOUT_MPSAFE);

	target->luns = NULL;
	target->num_lun = 0;

	/* XXX we may want to reload mgm port after each bus reset */
	/* XXX there might be multiple management agents */
	crom_init_context(&cc, target->fwdev->csrrom);
	reg = crom_search_key(&cc, CROM_MGM);
	if (reg == NULL || reg->val == 0) {
		aprint_error_dev(sc->sc_fd.dev, "NULL management address\n");
		target->fwdev = NULL;
		return NULL;
	}

	target->mgm_hi = 0xffff;
	target->mgm_lo = 0xf0000000 | (reg->val << 2);

	return target;
}

static void
sbp_probe_lun(struct sbp_dev *sdev)
{
	struct fw_device *fwdev;
	struct crom_context c, *cc = &c;
	struct csrreg *reg;

	memset(sdev->vendor, 0, sizeof(sdev->vendor));
	memset(sdev->product, 0, sizeof(sdev->product));

	fwdev = sdev->target->fwdev;
	crom_init_context(cc, fwdev->csrrom);
	/* get vendor string */
	crom_search_key(cc, CSRKEY_VENDOR);
	crom_next(cc);
	crom_parse_text(cc, sdev->vendor, sizeof(sdev->vendor));
	/* skip to the unit directory for SBP-2 */
	while ((reg = crom_search_key(cc, CSRKEY_VER)) != NULL) {
		if (reg->val == CSRVAL_T10SBP2)
			break;
		crom_next(cc);
	}
	/* get firmware revision */
	reg = crom_search_key(cc, CSRKEY_FIRM_VER);
	if (reg != NULL)
		snprintf(sdev->revision, sizeof(sdev->revision), "%06x",
		    reg->val);
	/* get product string */
	crom_search_key(cc, CSRKEY_MODEL);
	crom_next(cc);
	crom_parse_text(cc, sdev->product, sizeof(sdev->product));
}

static void
sbp_login_callout(void *arg)
{
	struct sbp_dev *sdev = (struct sbp_dev *)arg;

	sbp_mgm_orb(sdev, ORB_FUN_LGI, NULL);
}

static void
sbp_login(struct sbp_dev *sdev)
{
	struct sbp_softc *sc = sdev->target->sbp;
	struct timeval delta;
	struct timeval t;
	int ticks = 0;

	microtime(&delta);
	timersub(&delta, &sc->sc_last_busreset, &delta);
	t.tv_sec = login_delay / 1000;
	t.tv_usec = (login_delay % 1000) * 1000;
	timersub(&t, &delta, &t);
	if (t.tv_sec >= 0 && t.tv_usec > 0)
		ticks = (t.tv_sec * 1000 + t.tv_usec / 1000) * hz / 1000;
SBP_DEBUG(0)
	printf("%s: sec = %lld usec = %ld ticks = %d\n", __func__,
	    (long long)t.tv_sec, (long)t.tv_usec, ticks);
END_DEBUG
	callout_schedule(&sdev->login_callout, ticks);
}

static void
sbp_probe_target(void *arg)
{
	struct sbp_target *target = (struct sbp_target *)arg;
	struct sbp_dev *sdev;
	int i;

SBP_DEBUG(1)
	printf("%s %d\n", __func__, target->target_id);
END_DEBUG

	sbp_alloc_lun(target);

	/* XXX untimeout mgm_ocb and dequeue */
	for (i = 0; i < target->num_lun; i++) {
		sdev = target->luns[i];
		if (sdev == NULL || sdev->status == SBP_DEV_DEAD)
			continue;

		if (sdev->periph != NULL) {
			scsipi_periph_freeze(sdev->periph, 1);
			sdev->freeze++;
		}
		sbp_probe_lun(sdev);
		sbp_show_sdev_info(sdev);

		sbp_abort_all_ocbs(sdev, XS_RESET);
		switch (sdev->status) {
		case SBP_DEV_RESET:
			/* new or revived target */
			if (auto_login)
				sbp_login(sdev);
			break;
		case SBP_DEV_TOATTACH:
		case SBP_DEV_PROBE:
		case SBP_DEV_ATTACHED:
		case SBP_DEV_RETRY:
		default:
			sbp_mgm_orb(sdev, ORB_FUN_RCN, NULL);
			break;
		}
	}
}

static void
sbp_post_busreset(void *arg)
{
	struct sbp_softc *sc = (struct sbp_softc *)arg;
	struct sbp_target *target = &sc->sc_target;
	struct fw_device *fwdev = target->fwdev;
	int alive;

	alive = SBP_FWDEV_ALIVE(fwdev);
SBP_DEBUG(0)
	printf("sbp_post_busreset\n");
	if (!alive)
		printf("not alive\n");
END_DEBUG
	microtime(&sc->sc_last_busreset);

	if (!alive)
		return;

	scsipi_channel_freeze(&sc->sc_channel, 1);
}

static void
sbp_post_explore(void *arg)
{
	struct sbp_softc *sc = (struct sbp_softc *)arg;
	struct sbp_target *target = &sc->sc_target;
	struct fw_device *fwdev = target->fwdev;
	int alive;

	alive = SBP_FWDEV_ALIVE(fwdev);
SBP_DEBUG(0)
	printf("sbp_post_explore (sbp_cold=%d)\n", sbp_cold);
	if (!alive)
		printf("not alive\n");
END_DEBUG
	if (!alive)
		return;

	if (!firewire_phydma_enable)
		return;

	if (sbp_cold > 0)
		sbp_cold--;

SBP_DEBUG(0)
	printf("sbp_post_explore: EUI:%08x%08x ", fwdev->eui.hi, fwdev->eui.lo);
END_DEBUG
	sbp_probe_target((void *)target);
	if (target->num_lun == 0)
		sbp_free_target(target);

	scsipi_channel_thaw(&sc->sc_channel, 1);
}

#if NEED_RESPONSE
static void
sbp_loginres_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev = (struct sbp_dev *)xfer->sc;
	struct sbp_softc *sc = sdev->target->sbp;

SBP_DEBUG(1)
	printf("sbp_loginres_callback\n");
END_DEBUG
	/* recycle */
	mutex_enter(&sc->sc_fwb.fwb_mtx);
	STAILQ_INSERT_TAIL(&sc->sc_fwb.xferlist, xfer, link);
	mutex_exit(&sc->sc_fwb.fwb_mtx);
	return;
}
#endif

static inline void
sbp_xfer_free(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev = (struct sbp_dev *)xfer->sc;
	struct sbp_softc *sc = sdev->target->sbp;

	fw_xfer_unload(xfer);
	mutex_enter(&sc->sc_mtx);
	STAILQ_INSERT_TAIL(&sdev->target->xferlist, xfer, link);
	mutex_exit(&sc->sc_mtx);
}

static void
sbp_reset_start_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *tsdev, *sdev = (struct sbp_dev *)xfer->sc;
	struct sbp_target *target = sdev->target;
	int i;

	if (xfer->resp != 0)
		aprint_error("%s: sbp_reset_start failed: resp=%d\n",
		    sdev->bustgtlun, xfer->resp);

	for (i = 0; i < target->num_lun; i++) {
		tsdev = target->luns[i];
		if (tsdev != NULL && tsdev->status == SBP_DEV_LOGIN)
			sbp_login(tsdev);
	}
}

static void
sbp_reset_start(struct sbp_dev *sdev)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

SBP_DEBUG(0)
	printf("%s: sbp_reset_start: %s\n",
	    device_xname(sdev->target->sbp->sc_fd.dev), sdev->bustgtlun);
END_DEBUG

	xfer = sbp_write_cmd(sdev, FWTCODE_WREQQ, 0);
	if (xfer == NULL)
		return;
	xfer->hand = sbp_reset_start_callback;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.dest_hi = 0xffff;
	fp->mode.wreqq.dest_lo = 0xf0000000 | RESET_START;
	fp->mode.wreqq.data = htonl(0xf);
	if (fw_asyreq(xfer->fc, -1, xfer) != 0)
		sbp_xfer_free(xfer);
}

static void
sbp_mgm_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;

	sdev = (struct sbp_dev *)xfer->sc;

SBP_DEBUG(1)
	printf("%s: sbp_mgm_callback: %s\n",
	    device_xname(sdev->target->sbp->sc_fd.dev), sdev->bustgtlun);
END_DEBUG
	sbp_xfer_free(xfer);
	return;
}

static void
sbp_scsipi_scan_target(void *arg)
{
	struct sbp_target *target = (struct sbp_target *)arg;
	struct sbp_softc *sc = target->sbp;
	struct sbp_dev *sdev;
	struct scsipi_channel *chan = &sc->sc_channel;
	struct scsibus_softc *sc_bus = device_private(sc->sc_bus);
	int lun, yet;

	do {
		mutex_enter(&sc->sc_mtx);
		cv_wait_sig(&sc->sc_cv, &sc->sc_mtx);
		mutex_exit(&sc->sc_mtx);
		yet = 0;

		for (lun = 0; lun < target->num_lun; lun++) {
			sdev = target->luns[lun];
			if (sdev == NULL)
				continue;
			if (sdev->status != SBP_DEV_PROBE) {
				yet++;
				continue;
			}

			if (sdev->periph == NULL) {
				if (chan->chan_nluns < target->num_lun)
					chan->chan_nluns = target->num_lun;

				scsi_probe_bus(sc_bus, target->target_id,
				    sdev->lun_id);
				sdev->periph = scsipi_lookup_periph(chan,
				    target->target_id, lun);
			}
			sdev->status = SBP_DEV_ATTACHED;
		}
	} while (yet > 0);

	sc->sc_lwp = NULL;
	kthread_exit(0);

	/* NOTREACHED */
}

static inline void
sbp_scan_dev(struct sbp_dev *sdev)
{
	struct sbp_softc *sc = sdev->target->sbp;

	sdev->status = SBP_DEV_PROBE;
	mutex_enter(&sc->sc_mtx);
	cv_signal(&sdev->target->sbp->sc_cv);
	mutex_exit(&sc->sc_mtx);
}


static void
sbp_do_attach(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;
	struct sbp_target *target;
	struct sbp_softc *sc;

	sdev = (struct sbp_dev *)xfer->sc;
	target = sdev->target;
	sc = target->sbp;

SBP_DEBUG(0)
	printf("%s:%s:%s\n", device_xname(sc->sc_fd.dev), __func__,
	    sdev->bustgtlun);
END_DEBUG
	sbp_xfer_free(xfer);

	sbp_scan_dev(sdev);
	return;
}

static void
sbp_agent_reset_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev = (struct sbp_dev *)xfer->sc;
	struct sbp_softc *sc = sdev->target->sbp;

SBP_DEBUG(1)
	printf("%s:%s:%s\n", device_xname(sc->sc_fd.dev), __func__,
	    sdev->bustgtlun);
END_DEBUG
	if (xfer->resp != 0)
		aprint_error_dev(sc->sc_fd.dev, "%s:%s: resp=%d\n", __func__,
		    sdev->bustgtlun, xfer->resp);

	sbp_xfer_free(xfer);
	if (sdev->periph != NULL) {
		scsipi_periph_thaw(sdev->periph, sdev->freeze);
		scsipi_channel_thaw(&sc->sc_channel, 0);
		sdev->freeze = 0;
	}
}

static void
sbp_agent_reset(struct sbp_dev *sdev)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

SBP_DEBUG(0)
	printf("%s:%s:%s\n", device_xname(sdev->target->sbp->sc_fd.dev),
	    __func__, sdev->bustgtlun);
END_DEBUG
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQQ, 0x04);
	if (xfer == NULL)
		return;
	if (sdev->status == SBP_DEV_ATTACHED || sdev->status == SBP_DEV_PROBE)
		xfer->hand = sbp_agent_reset_callback;
	else
		xfer->hand = sbp_do_attach;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.data = htonl(0xf);
	if (fw_asyreq(xfer->fc, -1, xfer) != 0)
		sbp_xfer_free(xfer);
	sbp_abort_all_ocbs(sdev, XS_RESET);
}

static void
sbp_busy_timeout_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev = (struct sbp_dev *)xfer->sc;

SBP_DEBUG(1)
	printf("%s:%s:%s\n", device_xname(sdev->target->sbp->sc_fd.dev),
	    __func__, sdev->bustgtlun);
END_DEBUG
	sbp_xfer_free(xfer);
	sbp_agent_reset(sdev);
}

static void
sbp_busy_timeout(struct sbp_dev *sdev)
{
	struct fw_pkt *fp;
	struct fw_xfer *xfer;

SBP_DEBUG(0)
	printf("%s:%s:%s\n", device_xname(sdev->target->sbp->sc_fd.dev),
	    __func__, sdev->bustgtlun);
END_DEBUG
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQQ, 0);
	if (xfer == NULL)
		return;
	xfer->hand = sbp_busy_timeout_callback;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.dest_hi = 0xffff;
	fp->mode.wreqq.dest_lo = 0xf0000000 | BUSY_TIMEOUT;
	fp->mode.wreqq.data = htonl((1 << (13+12)) | 0xf);
	if (fw_asyreq(xfer->fc, -1, xfer) != 0)
		sbp_xfer_free(xfer);
}

static void
sbp_orb_pointer_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev = (struct sbp_dev *)xfer->sc;
	struct sbp_softc *sc = sdev->target->sbp;

SBP_DEBUG(1)
	printf("%s:%s:%s\n", device_xname(sc->sc_fd.dev), __func__,
	    sdev->bustgtlun);
END_DEBUG
	if (xfer->resp != 0)
		aprint_error_dev(sc->sc_fd.dev, "%s:%s: xfer->resp = %d\n",
		    __func__, sdev->bustgtlun, xfer->resp);
	sbp_xfer_free(xfer);
	sdev->flags &= ~ORB_POINTER_ACTIVE;

	if ((sdev->flags & ORB_POINTER_NEED) != 0) {
		struct sbp_ocb *ocb;

		sdev->flags &= ~ORB_POINTER_NEED;
		ocb = STAILQ_FIRST(&sdev->ocbs);
		if (ocb != NULL)
			sbp_orb_pointer(sdev, ocb);
	}
	return;
}

static void
sbp_orb_pointer(struct sbp_dev *sdev, struct sbp_ocb *ocb)
{
	struct sbp_softc *sc = sdev->target->sbp;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

SBP_DEBUG(1)
	printf("%s:%s:%s: 0x%08x\n", device_xname(sc->sc_fd.dev), __func__,
	    sdev->bustgtlun, (uint32_t)ocb->bus_addr);
END_DEBUG

	if ((sdev->flags & ORB_POINTER_ACTIVE) != 0) {
SBP_DEBUG(0)
		printf("%s: orb pointer active\n", __func__);
END_DEBUG
		sdev->flags |= ORB_POINTER_NEED;
		return;
	}

	sdev->flags |= ORB_POINTER_ACTIVE;
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQB, 0x08);
	if (xfer == NULL)
		return;
	xfer->hand = sbp_orb_pointer_callback;

	fp = &xfer->send.hdr;
	fp->mode.wreqb.len = 8;
	fp->mode.wreqb.extcode = 0;
	xfer->send.payload[0] =
		htonl(((sc->sc_fd.fc->nodeid | FWLOCALBUS) << 16));
	xfer->send.payload[1] = htonl((uint32_t)ocb->bus_addr);

	if (fw_asyreq(xfer->fc, -1, xfer) != 0) {
		sbp_xfer_free(xfer);
		ocb->xs->error = XS_DRIVER_STUFFUP;
		scsipi_done(ocb->xs);
	}
}

static void
sbp_doorbell_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev = (struct sbp_dev *)xfer->sc;
	struct sbp_softc *sc = sdev->target->sbp;

SBP_DEBUG(1)
	printf("%s:%s:%s\n", device_xname(sc->sc_fd.dev), __func__,
	    sdev->bustgtlun);
END_DEBUG
	if (xfer->resp != 0) {
		aprint_error_dev(sc->sc_fd.dev, "%s: xfer->resp = %d\n",
		    __func__, xfer->resp);
	}
	sbp_xfer_free(xfer);
	sdev->flags &= ~ORB_DOORBELL_ACTIVE;
	if ((sdev->flags & ORB_DOORBELL_NEED) != 0) {
		sdev->flags &= ~ORB_DOORBELL_NEED;
		sbp_doorbell(sdev);
	}
	return;
}

static void
sbp_doorbell(struct sbp_dev *sdev)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

SBP_DEBUG(1)
	printf("%s:%s:%s\n", device_xname(sdev->target->sbp->sc_fd.dev),
	    __func__, sdev->bustgtlun);
END_DEBUG

	if ((sdev->flags & ORB_DOORBELL_ACTIVE) != 0) {
		sdev->flags |= ORB_DOORBELL_NEED;
		return;
	}
	sdev->flags |= ORB_DOORBELL_ACTIVE;
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQQ, 0x10);
	if (xfer == NULL)
		return;
	xfer->hand = sbp_doorbell_callback;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.data = htonl(0xf);
	if (fw_asyreq(xfer->fc, -1, xfer) != 0)
		sbp_xfer_free(xfer);
}

static struct fw_xfer *
sbp_write_cmd(struct sbp_dev *sdev, int tcode, int offset)
{
	struct sbp_softc *sc;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct sbp_target *target;
	int new = 0;

	target = sdev->target;
	sc = target->sbp;
	mutex_enter(&sc->sc_mtx);
	xfer = STAILQ_FIRST(&target->xferlist);
	if (xfer == NULL) {
		if (target->n_xfer > 5 /* XXX */) {
			aprint_error_dev(sc->sc_fd.dev,
			    "no more xfer for this target\n");
			mutex_exit(&sc->sc_mtx);
			return NULL;
		}
		xfer = fw_xfer_alloc_buf(M_SBP, 8, 0);
		if (xfer == NULL) {
			aprint_error_dev(sc->sc_fd.dev,
			    "fw_xfer_alloc_buf failed\n");
			mutex_exit(&sc->sc_mtx);
			return NULL;
		}
		target->n_xfer++;
SBP_DEBUG(0)
			printf("sbp: alloc %d xfer\n", target->n_xfer);
END_DEBUG
		new = 1;
	} else
		STAILQ_REMOVE_HEAD(&target->xferlist, link);
	mutex_exit(&sc->sc_mtx);

	microtime(&xfer->tv);

	if (new) {
		xfer->recv.pay_len = 0;
		xfer->send.spd = min(target->fwdev->speed, max_speed);
		xfer->fc = target->sbp->sc_fd.fc;
	}

	if (tcode == FWTCODE_WREQB)
		xfer->send.pay_len = 8;
	else
		xfer->send.pay_len = 0;

	xfer->sc = (void *)sdev;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.dest_hi = sdev->login->cmd_hi;
	fp->mode.wreqq.dest_lo = sdev->login->cmd_lo + offset;
	fp->mode.wreqq.tlrt = 0;
	fp->mode.wreqq.tcode = tcode;
	fp->mode.wreqq.pri = 0;
	fp->mode.wreqq.dst = FWLOCALBUS | target->fwdev->dst;

	return xfer;
}

static void
sbp_mgm_orb(struct sbp_dev *sdev, int func, struct sbp_ocb *aocb)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct sbp_ocb *ocb;
	struct sbp_target *target;
	int nid, dv_unit;

	target = sdev->target;
	nid = target->sbp->sc_fd.fc->nodeid | FWLOCALBUS;
	dv_unit = device_unit(target->sbp->sc_fd.dev);

	mutex_enter(&target->sbp->sc_mtx);
	if (func == ORB_FUN_RUNQUEUE) {
		ocb = STAILQ_FIRST(&target->mgm_ocb_queue);
		if (target->mgm_ocb_cur != NULL || ocb == NULL) {
			mutex_exit(&target->sbp->sc_mtx);
			return;
		}
		STAILQ_REMOVE_HEAD(&target->mgm_ocb_queue, ocb);
		mutex_exit(&target->sbp->sc_mtx);
		goto start;
	}
	if ((ocb = sbp_get_ocb(sdev)) == NULL) {
		mutex_exit(&target->sbp->sc_mtx);
		/* XXX */
		return;
	}
	mutex_exit(&target->sbp->sc_mtx);
	ocb->flags = OCB_ACT_MGM;
	ocb->sdev = sdev;

	memset(ocb->orb, 0, sizeof(ocb->orb));
	ocb->orb[6] = htonl((nid << 16) | SBP_BIND_HI);
	ocb->orb[7] = htonl(SBP_DEV2ADDR(dv_unit, sdev->lun_id));

SBP_DEBUG(0)
	printf("%s:%s:%s: %s\n", device_xname(sdev->target->sbp->sc_fd.dev),
	    __func__, sdev->bustgtlun, orb_fun_name[(func>>16)&0xf]);
END_DEBUG
	switch (func) {
	case ORB_FUN_LGI:
	{
		const off_t sbp_login_off =
		    sizeof(struct sbp_ocb) * SBP_QUEUE_LEN;

		ocb->orb[0] = ocb->orb[1] = 0; /* password */
		ocb->orb[2] = htonl(nid << 16);
		ocb->orb[3] = htonl(sdev->dma.bus_addr + sbp_login_off);
		ocb->orb[4] = htonl(ORB_NOTIFY | sdev->lun_id);
		if (ex_login)
			ocb->orb[4] |= htonl(ORB_EXV);
		ocb->orb[5] = htonl(SBP_LOGIN_SIZE);
		bus_dmamap_sync(sdev->dma.dma_tag, sdev->dma.dma_map,
		    sbp_login_off, SBP_LOGIN_SIZE, BUS_DMASYNC_PREREAD);
		break;
	}

	case ORB_FUN_ATA:
		ocb->orb[0] = htonl((0 << 16) | 0);
		ocb->orb[1] = htonl(aocb->bus_addr & 0xffffffff);
		/* fall through */
	case ORB_FUN_RCN:
	case ORB_FUN_LGO:
	case ORB_FUN_LUR:
	case ORB_FUN_RST:
	case ORB_FUN_ATS:
		ocb->orb[4] = htonl(ORB_NOTIFY | func | sdev->login->id);
		break;
	}

	if (target->mgm_ocb_cur != NULL) {
		/* there is a standing ORB */
		mutex_enter(&target->sbp->sc_mtx);
		STAILQ_INSERT_TAIL(&sdev->target->mgm_ocb_queue, ocb, ocb);
		mutex_exit(&target->sbp->sc_mtx);
		return;
	}
start:
	target->mgm_ocb_cur = ocb;

	callout_reset(&target->mgm_ocb_timeout, 5 * hz, sbp_mgm_timeout, ocb);
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQB, 0);
	if (xfer == NULL)
		return;
	xfer->hand = sbp_mgm_callback;

	fp = &xfer->send.hdr;
	fp->mode.wreqb.dest_hi = sdev->target->mgm_hi;
	fp->mode.wreqb.dest_lo = sdev->target->mgm_lo;
	fp->mode.wreqb.len = 8;
	fp->mode.wreqb.extcode = 0;
	xfer->send.payload[0] = htonl(nid << 16);
	xfer->send.payload[1] = htonl(ocb->bus_addr & 0xffffffff);

	/* cache writeback & invalidate(required ORB_FUN_LGI func) */
	/* when abort_ocb, should sync POST ope ? */
	SBP_ORB_DMA_SYNC(sdev->dma, ocb->index, BUS_DMASYNC_PREWRITE);
	if (fw_asyreq(xfer->fc, -1, xfer) != 0)
		sbp_xfer_free(xfer);
}

static void
sbp_print_scsi_cmd(struct sbp_ocb *ocb)
{
	struct scsipi_xfer *xs = ocb->xs;

	printf("%s:%d:%d:"
		" cmd: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x,"
		" flags: 0x%02x, %db cmd/%db data\n",
		device_xname(ocb->sdev->target->sbp->sc_fd.dev),
		xs->xs_periph->periph_target,
		xs->xs_periph->periph_lun,
		xs->cmd->opcode,
		xs->cmd->bytes[0], xs->cmd->bytes[1],
		xs->cmd->bytes[2], xs->cmd->bytes[3],
		xs->cmd->bytes[4], xs->cmd->bytes[5],
		xs->cmd->bytes[6], xs->cmd->bytes[7],
		xs->cmd->bytes[8],
		xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT),
		xs->cmdlen, xs->datalen);
}

static void
sbp_scsi_status(struct sbp_status *sbp_status, struct sbp_ocb *ocb)
{
	struct sbp_cmd_status *sbp_cmd_status;
	struct scsi_sense_data *sense = &ocb->xs->sense.scsi_sense;

	sbp_cmd_status = (struct sbp_cmd_status *)sbp_status->data;

SBP_DEBUG(0)
	sbp_print_scsi_cmd(ocb);
	/* XXX need decode status */
	printf("%s:"
	    " SCSI status %x sfmt %x valid %x key %x code %x qlfr %x len %d\n",
	    ocb->sdev->bustgtlun,
	    sbp_cmd_status->status,
	    sbp_cmd_status->sfmt,
	    sbp_cmd_status->valid,
	    sbp_cmd_status->s_key,
	    sbp_cmd_status->s_code,
	    sbp_cmd_status->s_qlfr,
	    sbp_status->len);
END_DEBUG

	switch (sbp_cmd_status->status) {
	case SCSI_CHECK:
	case SCSI_BUSY:
	case SCSI_TERMINATED:
		if (sbp_cmd_status->sfmt == SBP_SFMT_CURR)
			sense->response_code = SSD_RCODE_CURRENT;
		else
			sense->response_code = SSD_RCODE_DEFERRED;
		if (sbp_cmd_status->valid)
			sense->response_code |= SSD_RCODE_VALID;
		sense->flags = sbp_cmd_status->s_key;
		if (sbp_cmd_status->mark)
			sense->flags |= SSD_FILEMARK;
		if (sbp_cmd_status->eom)
			sense->flags |= SSD_EOM;
		if (sbp_cmd_status->ill_len)
			sense->flags |= SSD_ILI;

		memcpy(sense->info, &sbp_cmd_status->info, 4);

		if (sbp_status->len <= 1)
			/* XXX not scsi status. shouldn't be happened */
			sense->extra_len = 0;
		else if (sbp_status->len <= 4)
			/* add_sense_code(_qual), info, cmd_spec_info */
			sense->extra_len = 6;
		else
			/* fru, sense_key_spec */
			sense->extra_len = 10;

		memcpy(sense->csi, &sbp_cmd_status->cdb, 4);

		sense->asc = sbp_cmd_status->s_code;
		sense->ascq = sbp_cmd_status->s_qlfr;
		sense->fru = sbp_cmd_status->fru;

		memcpy(sense->sks.sks_bytes, sbp_cmd_status->s_keydep, 3);
		ocb->xs->error = XS_SENSE;
		ocb->xs->xs_status = sbp_cmd_status->status;
/*
{
		uint8_t j, *tmp;
		tmp = sense;
		for (j = 0; j < 32; j += 8)
			aprint_normal(
			    "sense %02x%02x %02x%02x %02x%02x %02x%02x\n",
			    tmp[j], tmp[j+1], tmp[j+2], tmp[j+3],
			    tmp[j+4], tmp[j+5], tmp[j+6], tmp[j+7]);

}
*/
		break;
	default:
		aprint_error_dev(ocb->sdev->target->sbp->sc_fd.dev,
		    "%s:%s: unknown scsi status 0x%x\n",
		    __func__, ocb->sdev->bustgtlun, sbp_cmd_status->status);
	}
}

static void
sbp_fix_inq_data(struct sbp_ocb *ocb)
{
	struct scsipi_xfer *xs = ocb->xs;
	struct sbp_dev *sdev;
	struct scsipi_inquiry_data *inq =
	    (struct scsipi_inquiry_data *)xs->data;

	sdev = ocb->sdev;

#if 0
/*
 * NetBSD is assuming always 0 for EVPD-bit and 'Page Code'.
 */
#define SI_EVPD		0x01
	if (xs->cmd->bytes[0] & SI_EVPD)
		return;
#endif
SBP_DEBUG(1)
	printf("%s:%s:%s\n", device_xname(sdev->target->sbp->sc_fd.dev),
	    __func__, sdev->bustgtlun);
END_DEBUG
	switch (inq->device & SID_TYPE) {
	case T_DIRECT:
#if 0
		/*
		 * XXX Convert Direct Access device to RBC.
		 * I've never seen FireWire DA devices which support READ_6.
		 */
		if ((inq->device & SID_TYPE) == T_DIRECT)
			inq->device |= T_SIMPLE_DIRECT; /* T_DIRECT == 0 */
#endif
		/* FALLTHROUGH */

	case T_SIMPLE_DIRECT:
		/*
		 * Override vendor/product/revision information.
		 * Some devices sometimes return strange strings.
		 */
#if 1
		memcpy(inq->vendor, sdev->vendor, sizeof(inq->vendor));
		memcpy(inq->product, sdev->product, sizeof(inq->product));
		memcpy(inq->revision + 2, sdev->revision,
		    sizeof(inq->revision));
#endif
		break;
	}
	/*
	 * Force to enable/disable tagged queuing.
	 * XXX CAM also checks SCP_QUEUE_DQUE flag in the control mode page.
	 */
	if (sbp_tags > 0)
		inq->flags3 |= SID_CmdQue;
	else if (sbp_tags < 0)
		inq->flags3 &= ~SID_CmdQue;

}

static void
sbp_recv(struct fw_xfer *xfer)
{
	struct fw_pkt *rfp;
#if NEED_RESPONSE
	struct fw_pkt *sfp;
#endif
	struct sbp_softc *sc;
	struct sbp_dev *sdev;
	struct sbp_ocb *ocb;
	struct sbp_login_res *login_res = NULL;
	struct sbp_status *sbp_status;
	struct sbp_target *target;
	int	orb_fun, status_valid0, status_valid, l, reset_agent = 0;
	uint32_t addr;
/*
	uint32_t *ld;
	ld = xfer->recv.buf;
printf("sbp %x %d %d %08x %08x %08x %08x\n",
			xfer->resp, xfer->recv.len, xfer->recv.off, ntohl(ld[0]), ntohl(ld[1]), ntohl(ld[2]), ntohl(ld[3]));
printf("sbp %08x %08x %08x %08x\n", ntohl(ld[4]), ntohl(ld[5]), ntohl(ld[6]), ntohl(ld[7]));
printf("sbp %08x %08x %08x %08x\n", ntohl(ld[8]), ntohl(ld[9]), ntohl(ld[10]), ntohl(ld[11]));
*/

	sc = (struct sbp_softc *)xfer->sc;
	if (xfer->resp != 0) {
		aprint_error_dev(sc->sc_fd.dev,
		    "sbp_recv: xfer->resp = %d\n", xfer->resp);
		goto done0;
	}
	if (xfer->recv.payload == NULL) {
		aprint_error_dev(sc->sc_fd.dev,
		    "sbp_recv: xfer->recv.payload == NULL\n");
		goto done0;
	}
	rfp = &xfer->recv.hdr;
	if (rfp->mode.wreqb.tcode != FWTCODE_WREQB) {
		aprint_error_dev(sc->sc_fd.dev,
		    "sbp_recv: tcode = %d\n", rfp->mode.wreqb.tcode);
		goto done0;
	}
	sbp_status = (struct sbp_status *)xfer->recv.payload;
	addr = rfp->mode.wreqb.dest_lo;
SBP_DEBUG(2)
	printf("received address 0x%x\n", addr);
END_DEBUG
	target = &sc->sc_target;
	l = SBP_ADDR2LUN(addr);
	if (l >= target->num_lun || target->luns[l] == NULL) {
		aprint_error_dev(sc->sc_fd.dev,
			"sbp_recv1: invalid lun %d (target=%d)\n",
			l, target->target_id);
		goto done0;
	}
	sdev = target->luns[l];

	ocb = NULL;
	switch (sbp_status->src) {
	case SRC_NEXT_EXISTS:
	case SRC_NO_NEXT:
		/* check mgm_ocb_cur first */
		ocb = target->mgm_ocb_cur;
		if (ocb != NULL)
			if (OCB_MATCH(ocb, sbp_status)) {
				callout_stop(&target->mgm_ocb_timeout);
				target->mgm_ocb_cur = NULL;
				break;
			}
		ocb = sbp_dequeue_ocb(sdev, sbp_status);
		if (ocb == NULL)
			aprint_error_dev(sc->sc_fd.dev,
			    "%s:%s: No ocb(%x) on the queue\n", __func__,
			    sdev->bustgtlun, ntohl(sbp_status->orb_lo));
		break;
	case SRC_UNSOL:
		/* unsolicit */
		aprint_error_dev(sc->sc_fd.dev,
		    "%s:%s: unsolicit status received\n",
		    __func__, sdev->bustgtlun);
		break;
	default:
		aprint_error_dev(sc->sc_fd.dev,
		    "%s:%s: unknown sbp_status->src\n",
		    __func__, sdev->bustgtlun);
	}

	status_valid0 = (sbp_status->src < 2
			&& sbp_status->resp == SBP_REQ_CMP
			&& sbp_status->dead == 0);
	status_valid = (status_valid0 && sbp_status->status == 0);

	if (!status_valid0 || debug > 2) {
		int status;
SBP_DEBUG(0)
		printf("%s:%s:%s: ORB status src:%x resp:%x dead:%x"
		    " len:%x stat:%x orb:%x%08x\n",
		    device_xname(sc->sc_fd.dev), __func__, sdev->bustgtlun,
		    sbp_status->src, sbp_status->resp, sbp_status->dead,
		    sbp_status->len, sbp_status->status,
		    ntohs(sbp_status->orb_hi), ntohl(sbp_status->orb_lo));
END_DEBUG
		printf("%s:%s\n", device_xname(sc->sc_fd.dev), sdev->bustgtlun);
		status = sbp_status->status;
		switch (sbp_status->resp) {
		case SBP_REQ_CMP:
			if (status > MAX_ORB_STATUS0)
				printf("%s\n", orb_status0[MAX_ORB_STATUS0]);
			else
				printf("%s\n", orb_status0[status]);
			break;
		case SBP_TRANS_FAIL:
			printf("Obj: %s, Error: %s\n",
			    orb_status1_object[(status>>6) & 3],
			    orb_status1_serial_bus_error[status & 0xf]);
			break;
		case SBP_ILLE_REQ:
			printf("Illegal request\n");
			break;
		case SBP_VEND_DEP:
			printf("Vendor dependent\n");
			break;
		default:
			printf("unknown respose code %d\n", sbp_status->resp);
		}
	}

	/* we have to reset the fetch agent if it's dead */
	if (sbp_status->dead) {
		if (sdev->periph != NULL) {
			scsipi_periph_freeze(sdev->periph, 1);
			sdev->freeze++;
		}
		reset_agent = 1;
	}

	if (ocb == NULL)
		goto done;

	switch (ntohl(ocb->orb[4]) & ORB_FMT_MSK) {
	case ORB_FMT_NOP:
		break;
	case ORB_FMT_VED:
		break;
	case ORB_FMT_STD:
		switch (ocb->flags) {
		case OCB_ACT_MGM:
			orb_fun = ntohl(ocb->orb[4]) & ORB_FUN_MSK;
			reset_agent = 0;
			switch (orb_fun) {
			case ORB_FUN_LGI:
			{
				const struct fwdma_alloc *dma = &sdev->dma;
				const off_t sbp_login_off =
				    sizeof(struct sbp_ocb) * SBP_QUEUE_LEN;

				bus_dmamap_sync(dma->dma_tag, dma->dma_map,
				    sbp_login_off, SBP_LOGIN_SIZE,
				    BUS_DMASYNC_POSTREAD);
				login_res = sdev->login;
				login_res->len = ntohs(login_res->len);
				login_res->id = ntohs(login_res->id);
				login_res->cmd_hi = ntohs(login_res->cmd_hi);
				login_res->cmd_lo = ntohl(login_res->cmd_lo);
				if (status_valid) {
SBP_DEBUG(0)
					printf("%s:%s:%s: login:"
					    " len %d, ID %d, cmd %08x%08x,"
					    " recon_hold %d\n",
					    device_xname(sc->sc_fd.dev),
					    __func__, sdev->bustgtlun,
					    login_res->len, login_res->id,
					    login_res->cmd_hi,
					    login_res->cmd_lo,
					    ntohs(login_res->recon_hold));
END_DEBUG
					sbp_busy_timeout(sdev);
				} else {
					/* forgot logout? */
					aprint_error_dev(sc->sc_fd.dev,
					    "%s:%s: login failed\n",
					    __func__, sdev->bustgtlun);
					sdev->status = SBP_DEV_RESET;
				}
				break;
			}
			case ORB_FUN_RCN:
				login_res = sdev->login;
				if (status_valid) {
SBP_DEBUG(0)
					printf("%s:%s:%s: reconnect:"
					    " len %d, ID %d, cmd %08x%08x\n",
					    device_xname(sc->sc_fd.dev),
					    __func__, sdev->bustgtlun,
					    login_res->len, login_res->id,
					    login_res->cmd_hi,
					    login_res->cmd_lo);
END_DEBUG
					sbp_agent_reset(sdev);
				} else {
					/* reconnection hold time exceed? */
SBP_DEBUG(0)
					aprint_error_dev(sc->sc_fd.dev,
					    "%s:%s: reconnect failed\n",
					    __func__, sdev->bustgtlun);
END_DEBUG
					sbp_login(sdev);
				}
				break;
			case ORB_FUN_LGO:
				sdev->status = SBP_DEV_RESET;
				break;
			case ORB_FUN_RST:
				sbp_busy_timeout(sdev);
				break;
			case ORB_FUN_LUR:
			case ORB_FUN_ATA:
			case ORB_FUN_ATS:
				sbp_agent_reset(sdev);
				break;
			default:
				aprint_error_dev(sc->sc_fd.dev,
				    "%s:%s: unknown function %d\n",
				    __func__, sdev->bustgtlun, orb_fun);
				break;
			}
			sbp_mgm_orb(sdev, ORB_FUN_RUNQUEUE, NULL);
			break;
		case OCB_ACT_CMD:
			sdev->timeout = 0;
			if (ocb->xs != NULL) {
				struct scsipi_xfer *xs = ocb->xs;

				if (sbp_status->len > 1)
					sbp_scsi_status(sbp_status, ocb);
				else
					if (sbp_status->resp != SBP_REQ_CMP)
						xs->error = XS_DRIVER_STUFFUP;
					else {
						xs->error = XS_NOERROR;
						xs->resid = 0;
					}
				/* fix up inq data */
				if (xs->cmd->opcode == INQUIRY)
					sbp_fix_inq_data(ocb);
				scsipi_done(xs);
			}
			break;
		default:
			break;
		}
	}

	if (!use_doorbell)
		sbp_free_ocb(sdev, ocb);
done:
	if (reset_agent)
		sbp_agent_reset(sdev);

done0:
	xfer->recv.pay_len = SBP_RECV_LEN;
/* The received packet is usually small enough to be stored within
 * the buffer. In that case, the controller return ack_complete and
 * no respose is necessary.
 *
 * XXX fwohci.c and firewire.c should inform event_code such as
 * ack_complete or ack_pending to upper driver.
 */
#if NEED_RESPONSE
	xfer->send.off = 0;
	sfp = (struct fw_pkt *)xfer->send.buf;
	sfp->mode.wres.dst = rfp->mode.wreqb.src;
	xfer->dst = sfp->mode.wres.dst;
	xfer->spd = min(sdev->target->fwdev->speed, max_speed);
	xfer->hand = sbp_loginres_callback;

	sfp->mode.wres.tlrt = rfp->mode.wreqb.tlrt;
	sfp->mode.wres.tcode = FWTCODE_WRES;
	sfp->mode.wres.rtcode = 0;
	sfp->mode.wres.pri = 0;

	if (fw_asyreq(xfer->fc, -1, xfer) != 0) {
		aprint_error_dev(sc->sc_fd.dev, "mgm_orb failed\n");
		mutex_enter(&sc->sc_fwb.fwb_mtx);
		STAILQ_INSERT_TAIL(&sc->sc_fwb.xferlist, xfer, link);
		mutex_exit(&sc->sc_fwb.fwb_mtx);
	}
#else
	/* recycle */
	mutex_enter(&sc->sc_fwb.fwb_mtx);
	STAILQ_INSERT_TAIL(&sc->sc_fwb.xferlist, xfer, link);
	mutex_exit(&sc->sc_fwb.fwb_mtx);
#endif

	return;

}

static int
sbp_logout_all(struct sbp_softc *sbp)
{
	struct sbp_target *target;
	struct sbp_dev *sdev;
	int i;

SBP_DEBUG(0)
	printf("sbp_logout_all\n");
END_DEBUG
	target = &sbp->sc_target;
	if (target->luns != NULL) {
		for (i = 0; i < target->num_lun; i++) {
			sdev = target->luns[i];
			if (sdev == NULL)
				continue;
			callout_stop(&sdev->login_callout);
			if (sdev->status >= SBP_DEV_TOATTACH &&
			    sdev->status <= SBP_DEV_ATTACHED)
				sbp_mgm_orb(sdev, ORB_FUN_LGO, NULL);
		}
	}

	return 0;
}

static void
sbp_free_sdev(struct sbp_dev *sdev)
{
	struct sbp_softc *sc = sdev->target->sbp;
	int i;

	if (sdev == NULL)
		return;
	for (i = 0; i < SBP_QUEUE_LEN; i++)
		bus_dmamap_destroy(sc->sc_dmat, sdev->ocb[i].dmamap);
	fwdma_free(sdev->dma.dma_tag, sdev->dma.dma_map, sdev->dma.v_addr);
	free(sdev, M_SBP);
	sdev = NULL;
}

static void
sbp_free_target(struct sbp_target *target)
{
	struct fw_xfer *xfer, *next;
	int i;

	if (target->luns == NULL)
		return;
	callout_stop(&target->mgm_ocb_timeout);
	for (i = 0; i < target->num_lun; i++)
		sbp_free_sdev(target->luns[i]);

	for (xfer = STAILQ_FIRST(&target->xferlist);
	    xfer != NULL; xfer = next) {
		next = STAILQ_NEXT(xfer, link);
		fw_xfer_free_buf(xfer);
	}
	STAILQ_INIT(&target->xferlist);
	free(target->luns, M_SBP);
	target->num_lun = 0;
	target->luns = NULL;
	target->fwdev = NULL;
}

static void
sbp_scsipi_detach_sdev(struct sbp_dev *sdev)
{
	struct sbp_target *target;
	struct sbp_softc *sbp;

	if (sdev == NULL)
		return;

	target = sdev->target;
	if (target == NULL)
		return;

	sbp = target->sbp;

	if (sdev->status == SBP_DEV_DEAD)
		return;
	if (sdev->status == SBP_DEV_RESET)
		return;
	if (sdev->periph != NULL) {
		scsipi_periph_thaw(sdev->periph, sdev->freeze);
		scsipi_channel_thaw(&sbp->sc_channel, 0);	/* XXXX */
		sdev->freeze = 0;
		if (scsipi_target_detach(&sbp->sc_channel,
		    target->target_id, sdev->lun_id, DETACH_FORCE) != 0) {
			aprint_error_dev(sbp->sc_fd.dev, "detach failed\n");
		}
		sdev->periph = NULL;
	}
	sbp_abort_all_ocbs(sdev, XS_DRIVER_STUFFUP);
}

static void
sbp_scsipi_detach_target(struct sbp_target *target)
{
	struct sbp_softc *sbp = target->sbp;
	int i;

	if (target->luns != NULL) {
SBP_DEBUG(0)
		printf("sbp_detach_target %d\n", target->target_id);
END_DEBUG
		for (i = 0; i < target->num_lun; i++)
			sbp_scsipi_detach_sdev(target->luns[i]);
		if (config_detach(sbp->sc_bus, DETACH_FORCE) != 0)
			aprint_error_dev(sbp->sc_fd.dev, "%d detach failed\n",
			    target->target_id);
		sbp->sc_bus = NULL;
	}
}

static void
sbp_target_reset(struct sbp_dev *sdev, int method)
{
	struct sbp_target *target = sdev->target;
	struct sbp_dev *tsdev;
	int i;

	for (i = 0; i < target->num_lun; i++) {
		tsdev = target->luns[i];
		if (tsdev == NULL)
			continue;
		if (tsdev->status == SBP_DEV_DEAD)
			continue;
		if (tsdev->status == SBP_DEV_RESET)
			continue;
		if (sdev->periph != NULL) {
			scsipi_periph_freeze(tsdev->periph, 1);
			tsdev->freeze++;
		}
		sbp_abort_all_ocbs(tsdev, XS_TIMEOUT);
		if (method == 2)
			tsdev->status = SBP_DEV_LOGIN;
	}
	switch (method) {
	case 1:
		aprint_error("target reset\n");
		sbp_mgm_orb(sdev, ORB_FUN_RST, NULL);
		break;
	case 2:
		aprint_error("reset start\n");
		sbp_reset_start(sdev);
		break;
	}
}

static void
sbp_mgm_timeout(void *arg)
{
	struct sbp_ocb *ocb = (struct sbp_ocb *)arg;
	struct sbp_dev *sdev = ocb->sdev;
	struct sbp_target *target = sdev->target;

	aprint_error_dev(sdev->target->sbp->sc_fd.dev,
	    "%s:%s: request timeout(mgm orb:0x%08x) ... ",
	    __func__, sdev->bustgtlun, (uint32_t)ocb->bus_addr);
	target->mgm_ocb_cur = NULL;
	sbp_free_ocb(sdev, ocb);
#if 0
	/* XXX */
	aprint_error("run next request\n");
	sbp_mgm_orb(sdev, ORB_FUN_RUNQUEUE, NULL);
#endif
	aprint_error_dev(sdev->target->sbp->sc_fd.dev,
	    "%s:%s: reset start\n", __func__, sdev->bustgtlun);
	sbp_reset_start(sdev);
}

static void
sbp_timeout(void *arg)
{
	struct sbp_ocb *ocb = (struct sbp_ocb *)arg;
	struct sbp_dev *sdev = ocb->sdev;

	aprint_error_dev(sdev->target->sbp->sc_fd.dev,
	    "%s:%s: request timeout(cmd orb:0x%08x) ... ",
	    __func__, sdev->bustgtlun, (uint32_t)ocb->bus_addr);

	sdev->timeout++;
	switch (sdev->timeout) {
	case 1:
		aprint_error("agent reset\n");
		if (sdev->periph != NULL) {
			scsipi_periph_freeze(sdev->periph, 1);
			sdev->freeze++;
		}
		sbp_abort_all_ocbs(sdev, XS_TIMEOUT);
		sbp_agent_reset(sdev);
		break;
	case 2:
	case 3:
		sbp_target_reset(sdev, sdev->timeout - 1);
		break;
	default:
		aprint_error("\n");
#if 0
		/* XXX give up */
		sbp_scsipi_detach_target(target);
		if (target->luns != NULL)
			free(target->luns, M_SBP);
		target->num_lun = 0;
		target->luns = NULL;
		target->fwdev = NULL;
#endif
	}
}

static void
sbp_action1(struct sbp_softc *sc, struct scsipi_xfer *xs)
{
	struct sbp_target *target = &sc->sc_target;
	struct sbp_dev *sdev = NULL;
	struct sbp_ocb *ocb;
	int speed, flag, error;
	void *cdb;

	/* target:lun -> sdev mapping */
	if (target->fwdev != NULL &&
	    xs->xs_periph->periph_lun < target->num_lun) {
		sdev = target->luns[xs->xs_periph->periph_lun];
		if (sdev != NULL && sdev->status != SBP_DEV_ATTACHED &&
		    sdev->status != SBP_DEV_PROBE)
			sdev = NULL;
	}

	if (sdev == NULL) {
SBP_DEBUG(1)
		printf("%s:%d:%d: Invalid target (target needed)\n",
			sc ? device_xname(sc->sc_fd.dev) : "???",
			xs->xs_periph->periph_target,
			xs->xs_periph->periph_lun);
END_DEBUG

		xs->error = XS_DRIVER_STUFFUP;
		scsipi_done(xs);
		return;
	}

SBP_DEBUG(2)
	printf("%s:%d:%d:"
		" cmd: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x,"
		" flags: 0x%02x, %db cmd/%db data\n",
		device_xname(sc->sc_fd.dev),
		xs->xs_periph->periph_target,
		xs->xs_periph->periph_lun,
		xs->cmd->opcode,
		xs->cmd->bytes[0], xs->cmd->bytes[1],
		xs->cmd->bytes[2], xs->cmd->bytes[3],
		xs->cmd->bytes[4], xs->cmd->bytes[5],
		xs->cmd->bytes[6], xs->cmd->bytes[7],
		xs->cmd->bytes[8],
		xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT),
		xs->cmdlen, xs->datalen);
END_DEBUG
	mutex_enter(&sc->sc_mtx);
	ocb = sbp_get_ocb(sdev);
	mutex_exit(&sc->sc_mtx);
	if (ocb == NULL) {
		xs->error = XS_REQUEUE;
		if (sdev->freeze == 0) {
			scsipi_periph_freeze(sdev->periph, 1);
			sdev->freeze++;
		}
		scsipi_done(xs);
		return;
	}

	ocb->flags = OCB_ACT_CMD;
	ocb->sdev = sdev;
	ocb->xs = xs;
	ocb->orb[0] = htonl(1 << 31);
	ocb->orb[1] = 0;
	ocb->orb[2] = htonl(((sc->sc_fd.fc->nodeid | FWLOCALBUS) << 16));
	ocb->orb[3] = htonl(ocb->bus_addr + IND_PTR_OFFSET);
	speed = min(target->fwdev->speed, max_speed);
	ocb->orb[4] =
	    htonl(ORB_NOTIFY | ORB_CMD_SPD(speed) | ORB_CMD_MAXP(speed + 7));
	if ((xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) ==
	    XS_CTL_DATA_IN) {
		ocb->orb[4] |= htonl(ORB_CMD_IN);
		flag = BUS_DMA_READ;
	} else
		flag = BUS_DMA_WRITE;

	cdb = xs->cmd;
	memcpy((void *)&ocb->orb[5], cdb, xs->cmdlen);
/*
printf("ORB %08x %08x %08x %08x\n", ntohl(ocb->orb[0]), ntohl(ocb->orb[1]), ntohl(ocb->orb[2]), ntohl(ocb->orb[3]));
printf("ORB %08x %08x %08x %08x\n", ntohl(ocb->orb[4]), ntohl(ocb->orb[5]), ntohl(ocb->orb[6]), ntohl(ocb->orb[7]));
*/
	if (xs->datalen > 0) {
		error = bus_dmamap_load(sc->sc_dmat, ocb->dmamap,
		    xs->data, xs->datalen, NULL, BUS_DMA_NOWAIT | flag);
		if (error) {
			aprint_error_dev(sc->sc_fd.dev,
			    "DMA map load error %d\n", error);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
		} else
			sbp_execute_ocb(ocb, ocb->dmamap->dm_segs,
			    ocb->dmamap->dm_nsegs);
	} else
		sbp_execute_ocb(ocb, NULL, 0);

	return;
}

static void
sbp_execute_ocb(struct sbp_ocb *ocb, bus_dma_segment_t *segments, int seg)
{
	struct sbp_ocb *prev;
	bus_dma_segment_t *s;
	int i;

SBP_DEBUG(2)
	printf("sbp_execute_ocb: seg %d", seg);
	for (i = 0; i < seg; i++)
		printf(", %jx:%jd", (uintmax_t)segments[i].ds_addr,
		    (uintmax_t)segments[i].ds_len);
	printf("\n");
END_DEBUG

	if (seg == 1) {
		/* direct pointer */
		s = segments;
		if (s->ds_len > SBP_SEG_MAX)
			panic("ds_len > SBP_SEG_MAX, fix busdma code");
		ocb->orb[3] = htonl(s->ds_addr);
		ocb->orb[4] |= htonl(s->ds_len);
	} else if (seg > 1) {
		/* page table */
		for (i = 0; i < seg; i++) {
			s = &segments[i];
SBP_DEBUG(0)
			/* XXX LSI Logic "< 16 byte" bug might be hit */
			if (s->ds_len < 16)
				printf("sbp_execute_ocb: warning, "
				    "segment length(%jd) is less than 16."
				    "(seg=%d/%d)\n",
				    (uintmax_t)s->ds_len, i + 1, seg);
END_DEBUG
			if (s->ds_len > SBP_SEG_MAX)
				panic("ds_len > SBP_SEG_MAX, fix busdma code");
			ocb->ind_ptr[i].hi = htonl(s->ds_len << 16);
			ocb->ind_ptr[i].lo = htonl(s->ds_addr);
		}
		ocb->orb[4] |= htonl(ORB_CMD_PTBL | seg);
	}

	if (seg > 0) {
		struct sbp_softc *sc = ocb->sdev->target->sbp;
		const int flag = (ntohl(ocb->orb[4]) & ORB_CMD_IN) ?
		    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(sc->sc_dmat, ocb->dmamap,
		    0, ocb->dmamap->dm_mapsize, flag);
	}
	prev = sbp_enqueue_ocb(ocb->sdev, ocb);
	SBP_ORB_DMA_SYNC(ocb->sdev->dma, ocb->index, BUS_DMASYNC_PREWRITE);
	if (use_doorbell) {
		if (prev == NULL) {
			if (ocb->sdev->last_ocb != NULL)
				sbp_doorbell(ocb->sdev);
			else
				sbp_orb_pointer(ocb->sdev, ocb);
		}
	} else
		if (prev == NULL || (ocb->sdev->flags & ORB_LINK_DEAD) != 0) {
			ocb->sdev->flags &= ~ORB_LINK_DEAD;
			sbp_orb_pointer(ocb->sdev, ocb);
		}
}

static struct sbp_ocb *
sbp_dequeue_ocb(struct sbp_dev *sdev, struct sbp_status *sbp_status)
{
	struct sbp_softc *sc = sdev->target->sbp;
	struct sbp_ocb *ocb;
	struct sbp_ocb *next;
	int order = 0;

SBP_DEBUG(1)
	printf("%s:%s:%s: 0x%08x src %d\n", device_xname(sc->sc_fd.dev),
	    __func__, sdev->bustgtlun, ntohl(sbp_status->orb_lo),
	    sbp_status->src);
END_DEBUG
	mutex_enter(&sc->sc_mtx);
	for (ocb = STAILQ_FIRST(&sdev->ocbs); ocb != NULL; ocb = next) {
		next = STAILQ_NEXT(ocb, ocb);
		if (OCB_MATCH(ocb, sbp_status)) {
			/* found */
			SBP_ORB_DMA_SYNC(sdev->dma, ocb->index,
			    BUS_DMASYNC_POSTWRITE);
			STAILQ_REMOVE(&sdev->ocbs, ocb, sbp_ocb, ocb);
			if (ocb->xs != NULL)
				callout_stop(&ocb->xs->xs_callout);
			if (ntohl(ocb->orb[4]) & 0xffff) {
				const int flag =
				    (ntohl(ocb->orb[4]) & ORB_CMD_IN) ?
							BUS_DMASYNC_POSTREAD :
							BUS_DMASYNC_POSTWRITE;

				bus_dmamap_sync(sc->sc_dmat, ocb->dmamap,
				    0, ocb->dmamap->dm_mapsize, flag);
				bus_dmamap_unload(sc->sc_dmat, ocb->dmamap);

			}
			if (!use_doorbell) {
				if (sbp_status->src == SRC_NO_NEXT) {
					if (next != NULL)
						sbp_orb_pointer(sdev, next);
					else if (order > 0)
						/*
						 * Unordered execution
						 * We need to send pointer for
						 * next ORB
						 */
						sdev->flags |= ORB_LINK_DEAD;
				}
			}
			break;
		} else
			order++;
	}
	mutex_exit(&sc->sc_mtx);

	if (ocb && use_doorbell) {
		/*
		 * XXX this is not correct for unordered
		 * execution.
		 */
		if (sdev->last_ocb != NULL)
			sbp_free_ocb(sdev, sdev->last_ocb);
		sdev->last_ocb = ocb;
		if (next != NULL &&
		    sbp_status->src == SRC_NO_NEXT)
			sbp_doorbell(sdev);
	}

SBP_DEBUG(0)
	if (ocb && order > 0)
		printf("%s:%s:%s: unordered execution order:%d\n",
		    device_xname(sc->sc_fd.dev), __func__, sdev->bustgtlun,
		    order);
END_DEBUG
	return ocb;
}

static struct sbp_ocb *
sbp_enqueue_ocb(struct sbp_dev *sdev, struct sbp_ocb *ocb)
{
	struct sbp_softc *sc = sdev->target->sbp;
	struct sbp_ocb *tocb, *prev, *prev2;

SBP_DEBUG(1)
	printf("%s:%s:%s: 0x%08jx\n", device_xname(sc->sc_fd.dev),
	    __func__, sdev->bustgtlun, (uintmax_t)ocb->bus_addr);
END_DEBUG
	mutex_enter(&sc->sc_mtx);
	prev = NULL;
	STAILQ_FOREACH(tocb, &sdev->ocbs, ocb)
		prev = tocb;
	prev2 = prev;
	STAILQ_INSERT_TAIL(&sdev->ocbs, ocb, ocb);
	mutex_exit(&sc->sc_mtx);

	callout_reset(&ocb->xs->xs_callout, mstohz(ocb->xs->timeout),
	    sbp_timeout, ocb);

	if (use_doorbell && prev == NULL)
		prev2 = sdev->last_ocb;

	if (prev2 != NULL) {
SBP_DEBUG(2)
		printf("linking chain 0x%jx -> 0x%jx\n",
		    (uintmax_t)prev2->bus_addr, (uintmax_t)ocb->bus_addr);
END_DEBUG
		/*
		 * Suppress compiler optimization so that orb[1] must be
		 * written first.
		 * XXX We may need an explicit memory barrier for other
		 * architectures other than i386/amd64.
		 */
		*(volatile uint32_t *)&prev2->orb[1] = htonl(ocb->bus_addr);
		*(volatile uint32_t *)&prev2->orb[0] = 0;
	}

	return prev;
}

static struct sbp_ocb *
sbp_get_ocb(struct sbp_dev *sdev)
{
	struct sbp_softc *sc = sdev->target->sbp;
	struct sbp_ocb *ocb;

	KASSERT(mutex_owned(&sc->sc_mtx));

	ocb = STAILQ_FIRST(&sdev->free_ocbs);
	if (ocb == NULL) {
		sdev->flags |= ORB_SHORTAGE;
		aprint_error_dev(sc->sc_fd.dev,
		    "ocb shortage!!!\n");
		return NULL;
	}
	STAILQ_REMOVE_HEAD(&sdev->free_ocbs, ocb);
	ocb->xs = NULL;
	return ocb;
}

static void
sbp_free_ocb(struct sbp_dev *sdev, struct sbp_ocb *ocb)
{
	struct sbp_softc *sc = sdev->target->sbp;
	int count;

	ocb->flags = 0;
	ocb->xs = NULL;

	mutex_enter(&sc->sc_mtx);
	STAILQ_INSERT_TAIL(&sdev->free_ocbs, ocb, ocb);
	mutex_exit(&sc->sc_mtx);
	if (sdev->flags & ORB_SHORTAGE) {
		sdev->flags &= ~ORB_SHORTAGE;
		count = sdev->freeze;
		sdev->freeze = 0;
		if (sdev->periph)
			scsipi_periph_thaw(sdev->periph, count);
		scsipi_channel_thaw(&sc->sc_channel, 0);
	}
}

static void
sbp_abort_ocb(struct sbp_ocb *ocb, int status)
{
	struct sbp_softc *sc;
	struct sbp_dev *sdev;

	sdev = ocb->sdev;
	sc = sdev->target->sbp;
SBP_DEBUG(0)
	printf("%s:%s:%s: sbp_abort_ocb 0x%jx\n", device_xname(sc->sc_fd.dev),
	    __func__, sdev->bustgtlun, (uintmax_t)ocb->bus_addr);
END_DEBUG
SBP_DEBUG(1)
	if (ocb->xs != NULL)
		sbp_print_scsi_cmd(ocb);
END_DEBUG
	if (ntohl(ocb->orb[4]) & 0xffff) {
		const int flag = (ntohl(ocb->orb[4]) & ORB_CMD_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE;

		bus_dmamap_sync(sc->sc_dmat, ocb->dmamap,
		    0, ocb->dmamap->dm_mapsize, flag);
		bus_dmamap_unload(sc->sc_dmat, ocb->dmamap);
	}
	if (ocb->xs != NULL) {
		callout_stop(&ocb->xs->xs_callout);
		ocb->xs->error = status;
		scsipi_done(ocb->xs);
	}
	sbp_free_ocb(sdev, ocb);
}

static void
sbp_abort_all_ocbs(struct sbp_dev *sdev, int status)
{
	struct sbp_softc *sc = sdev->target->sbp;
	struct sbp_ocb *ocb, *next;
	STAILQ_HEAD(, sbp_ocb) temp;

	mutex_enter(&sc->sc_mtx);
	STAILQ_INIT(&temp);
	STAILQ_CONCAT(&temp, &sdev->ocbs);
	STAILQ_INIT(&sdev->ocbs);
	mutex_exit(&sc->sc_mtx);

	for (ocb = STAILQ_FIRST(&temp); ocb != NULL; ocb = next) {
		next = STAILQ_NEXT(ocb, ocb);
		sbp_abort_ocb(ocb, status);
	}
	if (sdev->last_ocb != NULL) {
		sbp_free_ocb(sdev, sdev->last_ocb);
		sdev->last_ocb = NULL;
	}
}
