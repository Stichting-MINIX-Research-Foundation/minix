/*	$NetBSD: mlx.c,v 1.62 2014/07/25 08:10:37 dholland Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 */

/*-
 * Copyright (c) 1999 Michael Smith
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
 * from FreeBSD: mlx.c,v 1.14.2.3 2000/08/04 06:52:50 msmith Exp
 */

/*
 * Driver for the Mylex DAC960 family of RAID controllers.
 *
 * TODO:
 *
 * o Test and enable channel pause.
 * o SCSI pass-through.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mlx.c,v 1.62 2014/07/25 08:10:37 dholland Exp $");

#include "ld.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kthread.h>
#include <sys/disk.h>
#include <sys/kauth.h>

#include <machine/vmparam.h>
#include <sys/bus.h>

#include <dev/ldvar.h>

#include <dev/ic/mlxreg.h>
#include <dev/ic/mlxio.h>
#include <dev/ic/mlxvar.h>

#include "locators.h"

#define	MLX_TIMEOUT	60

#ifdef DIAGNOSTIC
#define	DPRINTF(x)	printf x
#else
#define	DPRINTF(x)
#endif

static void	mlx_adjqparam(struct mlx_softc *, int, int);
static int	mlx_ccb_submit(struct mlx_softc *, struct mlx_ccb *);
static int	mlx_check(struct mlx_softc *, int);
static void	mlx_configure(struct mlx_softc *, int);
static void	mlx_describe(struct mlx_softc *);
static void	*mlx_enquire(struct mlx_softc *, int, size_t,
			     void (*)(struct mlx_ccb *), int);
static int	mlx_fw_message(struct mlx_softc *, int, int, int);
static void	mlx_pause_action(struct mlx_softc *);
static void	mlx_pause_done(struct mlx_ccb *);
static void	mlx_periodic(struct mlx_softc *);
static void	mlx_periodic_enquiry(struct mlx_ccb *);
static void	mlx_periodic_eventlog_poll(struct mlx_softc *);
static void	mlx_periodic_eventlog_respond(struct mlx_ccb *);
static void	mlx_periodic_rebuild(struct mlx_ccb *);
static void	mlx_periodic_thread(void *);
static int	mlx_print(void *, const char *);
static int	mlx_rebuild(struct mlx_softc *, int, int);
static void	mlx_shutdown(void *);
static int	mlx_user_command(struct mlx_softc *, struct mlx_usercommand *);

dev_type_open(mlxopen);
dev_type_close(mlxclose);
dev_type_ioctl(mlxioctl);

const struct cdevsw mlx_cdevsw = {
	.d_open = mlxopen,
	.d_close = mlxclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = mlxioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

extern struct	cfdriver mlx_cd;
static struct	lwp *mlx_periodic_lwp;
static void	*mlx_sdh;

static struct {
	int	hwid;
	const char	*name;
} const mlx_cname[] = {
	{ 0x00, "960E/960M" },
	{ 0x01, "960P/PD" },
	{ 0x02,	"960PL" },
	{ 0x10, "960PG" },
	{ 0x11, "960PJ" },
	{ 0x12, "960PR" },
	{ 0x13,	"960PT" },
	{ 0x14, "960PTL0" },
	{ 0x15, "960PRL" },
	{ 0x16, "960PTL1" },
	{ 0x20, "1164PVX" },
};

static const char * const mlx_sense_msgs[] = {
	"because write recovery failed",
	"because of SCSI bus reset failure",
	"because of double check condition",
	"because it was removed",
	"because of gross error on SCSI chip",
	"because of bad tag returned from drive",
	"because of timeout on SCSI command",
	"because of reset SCSI command issued from system",
	"because busy or parity error count exceeded limit",
	"because of 'kill drive' command from system",
	"because of selection timeout",
	"due to SCSI phase sequence error",
	"due to unknown status"
};

static const char * const mlx_status_msgs[] = {
	"normal completion",				/* 0 */
	"irrecoverable data error",			/* 1 */
	"drive does not exist, or is offline",		/* 2 */
	"attempt to write beyond end of drive",		/* 3 */
	"bad data encountered",				/* 4 */
	"invalid log entry request",			/* 5 */
	"attempt to rebuild online drive",		/* 6 */
	"new disk failed during rebuild",		/* 7 */
	"invalid channel/target",			/* 8 */
	"rebuild/check already in progress",		/* 9 */
	"one or more disks are dead",			/* 10 */
	"invalid or non-redundant drive",		/* 11 */
	"channel is busy",				/* 12 */
	"channel is not stopped",			/* 13 */
	"rebuild successfully terminated",		/* 14 */
	"unsupported command",				/* 15 */
	"check condition received",			/* 16 */
	"device is busy",				/* 17 */
	"selection or command timeout",			/* 18 */
	"command terminated abnormally",		/* 19 */
	"controller wedged",				/* 20 */
	"software timeout",				/* 21 */
	"command busy (?)",				/* 22 */
};

static struct {
	u_char	command;
	u_char	msg;		/* Index into mlx_status_msgs[]. */
	u_short	status;
} const mlx_msgs[] = {
	{ MLX_CMD_READSG,	1,	0x0001 },
	{ MLX_CMD_READSG,	1,	0x0002 },
	{ MLX_CMD_READSG,	3,	0x0105 },
	{ MLX_CMD_READSG,	4,	0x010c },
	{ MLX_CMD_WRITESG,	1,	0x0001 },
	{ MLX_CMD_WRITESG,	1,	0x0002 },
	{ MLX_CMD_WRITESG,	3,	0x0105 },
	{ MLX_CMD_READSG_OLD,	1,	0x0001 },
	{ MLX_CMD_READSG_OLD,	1,	0x0002 },
	{ MLX_CMD_READSG_OLD,	3,	0x0105 },
	{ MLX_CMD_WRITESG_OLD,	1,	0x0001 },
	{ MLX_CMD_WRITESG_OLD,	1,	0x0002 },
	{ MLX_CMD_WRITESG_OLD,	3,	0x0105 },
	{ MLX_CMD_LOGOP,	5,	0x0105 },
	{ MLX_CMD_REBUILDASYNC,	6,	0x0002 },
	{ MLX_CMD_REBUILDASYNC,	7,	0x0004 },
	{ MLX_CMD_REBUILDASYNC,	8,	0x0105 },
	{ MLX_CMD_REBUILDASYNC,	9,	0x0106 },
	{ MLX_CMD_REBUILDASYNC,	14,	0x0107 },
	{ MLX_CMD_CHECKASYNC,	10,	0x0002 },
	{ MLX_CMD_CHECKASYNC,	11,	0x0105 },
	{ MLX_CMD_CHECKASYNC,	9,	0x0106 },
	{ MLX_CMD_STOPCHANNEL,	12,	0x0106 },
	{ MLX_CMD_STOPCHANNEL,	8,	0x0105 },
	{ MLX_CMD_STARTCHANNEL,	13,	0x0005 },
	{ MLX_CMD_STARTCHANNEL,	8,	0x0105 },
	{ MLX_CMD_DIRECT_CDB,	16,	0x0002 },
	{ MLX_CMD_DIRECT_CDB,	17,	0x0008 },
	{ MLX_CMD_DIRECT_CDB,	18,	0x000e },
	{ MLX_CMD_DIRECT_CDB,	19,	0x000f },
	{ MLX_CMD_DIRECT_CDB,	8,	0x0105 },

	{ 0,			20,	MLX_STATUS_WEDGED },
	{ 0,			21,	MLX_STATUS_LOST },
	{ 0,			22,	MLX_STATUS_BUSY },

	{ 0,			14,	0x0104 },
};

/*
 * Initialise the controller and our interface.
 */
void
mlx_init(struct mlx_softc *mlx, const char *intrstr)
{
	struct mlx_ccb *mc;
	struct mlx_enquiry_old *meo;
	struct mlx_enquiry2 *me2;
	struct mlx_cinfo *ci;
	int rv, fwminor, hscode, hserr, hsparam1, hsparam2, hsmsg;
	int size, i, rseg;
	const char *wantfwstr;
	bus_dma_segment_t seg;

	SIMPLEQ_INIT(&mlx->mlx_ccb_queue);
	SLIST_INIT(&mlx->mlx_ccb_freelist);
	TAILQ_INIT(&mlx->mlx_ccb_worklist);

	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", device_xname(mlx->mlx_dv),
		    intrstr);

	/*
	 * Allocate the scatter/gather lists.
	 */
        size = MLX_SGL_SIZE * MLX_MAX_QUEUECNT;

	if ((rv = bus_dmamem_alloc(mlx->mlx_dmat, size, PAGE_SIZE, 0, &seg, 1,
	    &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(mlx->mlx_dv, "unable to allocate sglists, rv = %d\n", rv);
		return;
	}

	if ((rv = bus_dmamem_map(mlx->mlx_dmat, &seg, rseg, size,
	    (void **)&mlx->mlx_sgls,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(mlx->mlx_dv, "unable to map sglists, rv = %d\n", rv);
		return;
	}

	if ((rv = bus_dmamap_create(mlx->mlx_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &mlx->mlx_dmamap)) != 0) {
		aprint_error_dev(mlx->mlx_dv, "unable to create sglist DMA map, rv = %d\n", rv);
		return;
	}

	if ((rv = bus_dmamap_load(mlx->mlx_dmat, mlx->mlx_dmamap,
	    mlx->mlx_sgls, size, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(mlx->mlx_dv, "unable to load sglist DMA map, rv = %d\n", rv);
		return;
	}

	mlx->mlx_sgls_paddr = mlx->mlx_dmamap->dm_segs[0].ds_addr;
	memset(mlx->mlx_sgls, 0, size);

	/*
	 * Allocate and initialize the CCBs.
	 */
	mc = malloc(sizeof(*mc) * MLX_MAX_QUEUECNT, M_DEVBUF, M_NOWAIT);
	mlx->mlx_ccbs = mc;

	for (i = 0; i < MLX_MAX_QUEUECNT; i++, mc++) {
		mc->mc_ident = i;
		rv = bus_dmamap_create(mlx->mlx_dmat, MLX_MAX_XFER,
		    MLX_MAX_SEGS, MLX_MAX_XFER, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &mc->mc_xfer_map);
		if (rv != 0)
			break;
		mlx->mlx_nccbs++;
		mlx_ccb_free(mlx, mc);
	}
	if (mlx->mlx_nccbs != MLX_MAX_QUEUECNT)
		printf("%s: %d/%d CCBs usable\n", device_xname(mlx->mlx_dv),
		    mlx->mlx_nccbs, MLX_MAX_QUEUECNT);

	/* Disable interrupts before we start talking to the controller */
	(*mlx->mlx_intaction)(mlx, 0);

	/* If we've got a reset routine, then reset the controller now. */
	if (mlx->mlx_reset != NULL) {
		printf("%s: resetting controller...\n", device_xname(mlx->mlx_dv));
		if ((*mlx->mlx_reset)(mlx) != 0) {
			aprint_error_dev(mlx->mlx_dv, "reset failed\n");
			return;
		}
	}

	/*
	 * Wait for the controller to come ready, handshaking with the
	 * firmware if required.  This is typically only necessary on
	 * platforms where the controller BIOS does not run.
	 */
	hsmsg = 0;

	for (;;) {
		hscode = (*mlx->mlx_fw_handshake)(mlx, &hserr, &hsparam1,
		    &hsparam2);
		if (hscode == 0) {
			if (hsmsg != 0)
				printf("%s: initialization complete\n",
				    device_xname(mlx->mlx_dv));
			break;
		}

		/* Report first time around... */
		if (hsmsg == 0) {
			printf("%s: initializing (may take some time)...\n",
			    device_xname(mlx->mlx_dv));
			hsmsg = 1;
		}

		/* Did we get a real message? */
		if (hscode == 2) {
			hscode = mlx_fw_message(mlx, hserr, hsparam1, hsparam2);

			/* Fatal initialisation error? */
			if (hscode != 0)
				return;
		}
	}

	/*
	 * Do quirk/feature related things.
	 */
	ci = &mlx->mlx_ci;

	if (ci->ci_iftype > 1) {
		me2 = mlx_enquire(mlx, MLX_CMD_ENQUIRY2,
		    sizeof(struct mlx_enquiry2), NULL, 0);
		if (me2 == NULL) {
			aprint_error_dev(mlx->mlx_dv, "ENQUIRY2 failed\n");
			return;
		}

		ci->ci_firmware_id[0] = me2->me_firmware_id[0];
		ci->ci_firmware_id[1] = me2->me_firmware_id[1];
		ci->ci_firmware_id[2] = me2->me_firmware_id[2];
		ci->ci_firmware_id[3] = me2->me_firmware_id[3];
		ci->ci_hardware_id = me2->me_hardware_id[0];
		ci->ci_mem_size = le32toh(me2->me_mem_size);
		ci->ci_max_sg = le16toh(me2->me_max_sg);
		ci->ci_max_commands = le16toh(me2->me_max_commands);
		ci->ci_nchan = me2->me_actual_channels;

		free(me2, M_DEVBUF);
	}

	if (ci->ci_iftype <= 2) {
		/*
		 * These controllers may not report the firmware version in
		 * the ENQUIRY2 response, or may not even support it.
		 */
		meo = mlx_enquire(mlx, MLX_CMD_ENQUIRY_OLD,
		    sizeof(struct mlx_enquiry_old), NULL, 0);
		if (meo == NULL) {
			aprint_error_dev(mlx->mlx_dv, "ENQUIRY_OLD failed\n");
			return;
		}
		ci->ci_firmware_id[0] = meo->me_fwmajor;
		ci->ci_firmware_id[1] = meo->me_fwminor;
		ci->ci_firmware_id[2] = 0;
		ci->ci_firmware_id[3] = '0';

		if (ci->ci_iftype == 1) {
			ci->ci_hardware_id = 0;	/* XXX */
			ci->ci_mem_size = 0;	/* XXX */
			ci->ci_max_sg = 17;	/* XXX */
			ci->ci_max_commands = meo->me_max_commands;
		}

		free(meo, M_DEVBUF);
	}

	wantfwstr = NULL;
	fwminor = ci->ci_firmware_id[1];

	switch (ci->ci_firmware_id[0]) {
	case 2:
		if (ci->ci_iftype == 1) {
			if (fwminor < 14)
				wantfwstr = "2.14";
		} else if (fwminor < 42)
			wantfwstr = "2.42";
		break;

	case 3:
		if (fwminor < 51)
			wantfwstr = "3.51";
		break;

	case 4:
		if (fwminor < 6)
			wantfwstr = "4.06";
		break;

	case 5:
		if (fwminor < 7)
			wantfwstr = "5.07";
		break;
	}

	/* Print a little information about the controller. */
	mlx_describe(mlx);

	if (wantfwstr != NULL) {
		printf("%s: WARNING: this f/w revision is not recommended\n",
		    device_xname(mlx->mlx_dv));
		printf("%s: WARNING: use revision %s or later\n",
		    device_xname(mlx->mlx_dv), wantfwstr);
	}

	/* We don't (yet) know where the event log is up to. */
	mlx->mlx_currevent = -1;

	/* No user-requested background operation is in progress. */
	mlx->mlx_bg = 0;
	mlx->mlx_rebuildstat.rs_code = MLX_REBUILDSTAT_IDLE;

	/* Set maximum number of queued commands for `regular' operations. */
	mlx->mlx_max_queuecnt =
	    min(ci->ci_max_commands, MLX_MAX_QUEUECNT) -
	    MLX_NCCBS_CONTROL;
#ifdef DIAGNOSTIC
	if (mlx->mlx_max_queuecnt < MLX_NCCBS_CONTROL + MLX_MAX_DRIVES)
		printf("%s: WARNING: few CCBs available\n",
		    device_xname(mlx->mlx_dv));
	if (ci->ci_max_sg < MLX_MAX_SEGS) {
		aprint_error_dev(mlx->mlx_dv, "oops, not enough S/G segments\n");
		return;
	}
#endif

	/* Attach child devices and enable interrupts. */
	mlx_configure(mlx, 0);
	(*mlx->mlx_intaction)(mlx, 1);
	mlx->mlx_flags |= MLXF_INITOK;

	if (mlx_sdh == NULL) {
		/*
		 * Set our `shutdownhook' before we start any device
		 * activity.
		 */
		mlx_sdh = shutdownhook_establish(mlx_shutdown, NULL);

		/* Create a status monitoring thread. */
		rv = kthread_create(PRI_NONE, 0, NULL, mlx_periodic_thread,
		    NULL, &mlx_periodic_lwp, "mlxtask");
		if (rv != 0)
			printf("mlx_init: unable to create thread (%d)\n", rv);
	}
}

/*
 * Tell the world about the controller.
 */
static void
mlx_describe(struct mlx_softc *mlx)
{
	struct mlx_cinfo *ci;
	static char tbuf[80];
	const char *model;
	int i;

	model = NULL;
	ci = &mlx->mlx_ci;

	for (i = 0; i < sizeof(mlx_cname) / sizeof(mlx_cname[0]); i++)
		if (ci->ci_hardware_id == mlx_cname[i].hwid) {
			model = mlx_cname[i].name;
			break;
		}

	if (model == NULL) {
		snprintf(tbuf, sizeof(tbuf), " model 0x%x", ci->ci_hardware_id);
		model = tbuf;
	}

	printf("%s: DAC%s, %d channel%s, firmware %d.%02d-%c-%02d",
	    device_xname(mlx->mlx_dv), model, ci->ci_nchan,
	    ci->ci_nchan > 1 ? "s" : "",
	    ci->ci_firmware_id[0], ci->ci_firmware_id[1],
	    ci->ci_firmware_id[3], ci->ci_firmware_id[2]);
	if (ci->ci_mem_size != 0)
		printf(", %dMB RAM", ci->ci_mem_size >> 20);
	printf("\n");
}

/*
 * Locate disk resources and attach children to them.
 */
static void
mlx_configure(struct mlx_softc *mlx, int waitok)
{
	struct mlx_enquiry *me;
	struct mlx_enquiry_old *meo;
	struct mlx_enq_sys_drive *mes;
	struct mlx_sysdrive *ms;
	struct mlx_attach_args mlxa;
	int i, nunits;
	u_int size;
	int locs[MLXCF_NLOCS];

	mlx->mlx_flags |= MLXF_RESCANNING;

	if (mlx->mlx_ci.ci_iftype <= 2) {
		meo = mlx_enquire(mlx, MLX_CMD_ENQUIRY_OLD,
		    sizeof(struct mlx_enquiry_old), NULL, waitok);
		if (meo == NULL) {
			aprint_error_dev(mlx->mlx_dv, "ENQUIRY_OLD failed\n");
			goto out;
		}
		mlx->mlx_numsysdrives = meo->me_num_sys_drvs;
		free(meo, M_DEVBUF);
	} else {
		me = mlx_enquire(mlx, MLX_CMD_ENQUIRY,
		    sizeof(struct mlx_enquiry), NULL, waitok);
		if (me == NULL) {
			aprint_error_dev(mlx->mlx_dv, "ENQUIRY failed\n");
			goto out;
		}
		mlx->mlx_numsysdrives = me->me_num_sys_drvs;
		free(me, M_DEVBUF);
	}

	mes = mlx_enquire(mlx, MLX_CMD_ENQSYSDRIVE,
	    sizeof(*mes) * MLX_MAX_DRIVES, NULL, waitok);
	if (mes == NULL) {
		aprint_error_dev(mlx->mlx_dv, "error fetching drive status\n");
		goto out;
	}

	/* Allow 1 queued command per unit while re-configuring. */
	mlx_adjqparam(mlx, 1, 0);

	ms = &mlx->mlx_sysdrive[0];
	nunits = 0;
	for (i = 0; i < MLX_MAX_DRIVES; i++, ms++) {
		size = le32toh(mes[i].sd_size);
		ms->ms_state = mes[i].sd_state;

		/*
		 * If an existing device has changed in some way (e.g. no
		 * longer present) then detach it.
		 */
		if (ms->ms_dv != NULL && (size != ms->ms_size ||
		    (mes[i].sd_raidlevel & 0xf) != ms->ms_raidlevel))
			config_detach(ms->ms_dv, DETACH_FORCE);

		ms->ms_size = size;
		ms->ms_raidlevel = mes[i].sd_raidlevel & 0xf;
		ms->ms_state = mes[i].sd_state;
		ms->ms_dv = NULL;

		if (i >= mlx->mlx_numsysdrives)
			continue;
		if (size == 0xffffffffU || size == 0)
			continue;

		/*
		 * Attach a new device.
		 */
		mlxa.mlxa_unit = i;

		locs[MLXCF_UNIT] = i;

		ms->ms_dv = config_found_sm_loc(mlx->mlx_dv, "mlx", locs,
				&mlxa, mlx_print, config_stdsubmatch);
		nunits += (ms->ms_dv != NULL);
	}

	free(mes, M_DEVBUF);

	if (nunits != 0)
		mlx_adjqparam(mlx, mlx->mlx_max_queuecnt / nunits,
		    mlx->mlx_max_queuecnt % nunits);
 out:
 	mlx->mlx_flags &= ~MLXF_RESCANNING;
}

/*
 * Print autoconfiguration message for a sub-device.
 */
static int
mlx_print(void *aux, const char *pnp)
{
	struct mlx_attach_args *mlxa;

	mlxa = (struct mlx_attach_args *)aux;

	if (pnp != NULL)
		aprint_normal("block device at %s", pnp);
	aprint_normal(" unit %d", mlxa->mlxa_unit);
	return (UNCONF);
}

/*
 * Shut down all configured `mlx' devices.
 */
static void
mlx_shutdown(void *cookie)
{
	struct mlx_softc *mlx;
	int i;

	for (i = 0; i < mlx_cd.cd_ndevs; i++)
		if ((mlx = device_lookup_private(&mlx_cd, i)) != NULL)
			mlx_flush(mlx, 0);
}

/*
 * Adjust queue parameters for all child devices.
 */
static void
mlx_adjqparam(struct mlx_softc *mlx, int mpu, int slop)
{
#if NLD > 0
	extern struct cfdriver ld_cd;
	struct ld_softc *ld;
	int i;

	for (i = 0; i < ld_cd.cd_ndevs; i++) {
		if ((ld = device_lookup_private(&ld_cd, i)) == NULL)
			continue;
		if (device_parent(ld->sc_dv) != mlx->mlx_dv)
			continue;
		ldadjqparam(ld, mpu + (slop-- > 0));
	}
#endif
}

/*
 * Accept an open operation on the control device.
 */
int
mlxopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct mlx_softc *mlx;

	if ((mlx = device_lookup_private(&mlx_cd, minor(dev))) == NULL)
		return (ENXIO);
	if ((mlx->mlx_flags & MLXF_INITOK) == 0)
		return (ENXIO);
	if ((mlx->mlx_flags & MLXF_OPEN) != 0)
		return (EBUSY);

	mlx->mlx_flags |= MLXF_OPEN;
	return (0);
}

/*
 * Accept the last close on the control device.
 */
int
mlxclose(dev_t dev, int flag, int mode,
    struct lwp *l)
{
	struct mlx_softc *mlx;

	mlx = device_lookup_private(&mlx_cd, minor(dev));
	mlx->mlx_flags &= ~MLXF_OPEN;
	return (0);
}

/*
 * Handle control operations.
 */
int
mlxioctl(dev_t dev, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct mlx_softc *mlx;
	struct mlx_rebuild_request *rb;
	struct mlx_rebuild_status *rs;
	struct mlx_pause *mp;
	struct mlx_sysdrive *ms;
	int i, rv, *arg, result;

	mlx = device_lookup_private(&mlx_cd, minor(dev));

	rb = (struct mlx_rebuild_request *)data;
	rs = (struct mlx_rebuild_status *)data;
	arg = (int *)data;
	rv = 0;

	switch (cmd) {
	case MLX_RESCAN_DRIVES:
		/*
		 * Scan the controller to see whether new drives have
		 * appeared, or old ones disappeared.
		 */
		mlx_configure(mlx, 1);
		return (0);

	case MLX_PAUSE_CHANNEL:
		/*
		 * Pause one or more SCSI channels for a period of time, to
		 * assist in the process of hot-swapping devices.
		 *
		 * Note that at least the 3.51 firmware on the DAC960PL
		 * doesn't seem to do this right.
		 */
		if ((mlx->mlx_flags & MLXF_PAUSEWORKS) == 0)
			return (EOPNOTSUPP);

		mp = (struct mlx_pause *)data;

		if ((mp->mp_which == MLX_PAUSE_CANCEL) &&
		    (mlx->mlx_pause.mp_when != 0)) {
			/* Cancel a pending pause operation. */
			mlx->mlx_pause.mp_which = 0;
			break;
		}

		/* Fix for legal channels. */
		mp->mp_which &= ((1 << mlx->mlx_ci.ci_nchan) -1);

		/* Check time values. */
		if (mp->mp_when < 0 || mp->mp_when > 3600 ||
		    mp->mp_howlong < 1 || mp->mp_howlong > (0xf * 30)) {
			rv = EINVAL;
			break;
		}

		/* Check for a pause currently running. */
		if ((mlx->mlx_pause.mp_which != 0) &&
		    (mlx->mlx_pause.mp_when == 0)) {
			rv = EBUSY;
			break;
		}

		/* Looks ok, go with it. */
		mlx->mlx_pause.mp_which = mp->mp_which;
		mlx->mlx_pause.mp_when = time_second + mp->mp_when;
		mlx->mlx_pause.mp_howlong =
		    mlx->mlx_pause.mp_when + mp->mp_howlong;

		return (0);

	case MLX_COMMAND:
		rv = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL, data);
		if (rv)
			return (rv);

		/*
		 * Accept a command passthrough-style.
		 */
		return (mlx_user_command(mlx, (struct mlx_usercommand *)data));

	case MLX_REBUILDASYNC:
		/*
		 * Start a rebuild on a given SCSI disk
		 */
		if (mlx->mlx_bg != 0) {
			rb->rr_status = 0x0106;
			rv = EBUSY;
			break;
		}

		rb->rr_status = mlx_rebuild(mlx, rb->rr_channel, rb->rr_target);
		switch (rb->rr_status) {
		case 0:
			rv = 0;
			break;
		case 0x10000:
			rv = ENOMEM;	/* Couldn't set up the command. */
			break;
		case 0x0002:
			rv = EBUSY;
			break;
		case 0x0104:
			rv = EIO;
			break;
		case 0x0105:
			rv = ERANGE;
			break;
		case 0x0106:
			rv = EBUSY;
			break;
		default:
			rv = EINVAL;
			break;
		}

		if (rv == 0)
			mlx->mlx_bg = MLX_BG_REBUILD;

		return (0);

	case MLX_REBUILDSTAT:
		/*
		 * Get the status of the current rebuild or consistency check.
		 */
		*rs = mlx->mlx_rebuildstat;
		return (0);

	case MLX_GET_SYSDRIVE:
		/*
		 * Return the system drive number matching the `ld' device
		 * unit in (arg), if it happens to belong to us.
		 */
		for (i = 0; i < MLX_MAX_DRIVES; i++) {
			ms = &mlx->mlx_sysdrive[i];
			if (ms->ms_dv != NULL)
				if (device_xname(ms->ms_dv)[2] == '0' + *arg) {
					*arg = i;
					return (0);
				}
		}
		return (ENOENT);

	case MLX_GET_CINFO:
		/*
		 * Return controller info.
		 */
		memcpy(arg, &mlx->mlx_ci, sizeof(mlx->mlx_ci));
		return (0);
	}

	switch (cmd) {
	case MLXD_DETACH:
	case MLXD_STATUS:
	case MLXD_CHECKASYNC:
		if ((u_int)*arg >= MLX_MAX_DRIVES)
			return (EINVAL);
		ms = &mlx->mlx_sysdrive[*arg];
		if (*arg > MLX_MAX_DRIVES || ms->ms_dv == NULL)
			return (ENOENT);
		break;

	default:
		return (ENOTTY);
	}

	switch (cmd) {
	case MLXD_DETACH:
		/*
		 * Disconnect from the specified drive; it may be about to go
		 * away.
		 */
		return (config_detach(ms->ms_dv, 0));

	case MLXD_STATUS:
		/*
		 * Return the current status of this drive.
		 */
		*arg = ms->ms_state;
		return (0);

	case MLXD_CHECKASYNC:
		/*
		 * Start a background consistency check on this drive.
		 */
		if (mlx->mlx_bg != 0) {
			*arg = 0x0106;
			return (EBUSY);
		}

		switch (result = mlx_check(mlx, *arg)) {
		case 0:
			rv = 0;
			break;
		case 0x10000:
			rv = ENOMEM;	/* Couldn't set up the command. */
			break;
		case 0x0002:
			rv = EIO;
			break;
		case 0x0105:
			rv = ERANGE;
			break;
		case 0x0106:
			rv = EBUSY;
			break;
		default:
			rv = EINVAL;
			break;
		}

		if (rv == 0)
			mlx->mlx_bg = MLX_BG_CHECK;
		*arg = result;
		return (rv);
	}

	return (ENOTTY);	/* XXX shut up gcc */
}

static void
mlx_periodic_thread(void *cookie)
{
	struct mlx_softc *mlx;
	int i;

	for (;;) {
		for (i = 0; i < mlx_cd.cd_ndevs; i++)
			if ((mlx = device_lookup_private(&mlx_cd, i)) != NULL)
				if (mlx->mlx_ci.ci_iftype > 1)
					mlx_periodic(mlx);

		tsleep(mlx_periodic_thread, PWAIT, "mlxzzz", hz * 2);
	}
}

static void
mlx_periodic(struct mlx_softc *mlx)
{
	struct mlx_ccb *mc, *nmc;
	int etype, s;

	if ((mlx->mlx_pause.mp_which != 0) &&
	    (mlx->mlx_pause.mp_when > 0) &&
	    (time_second >= mlx->mlx_pause.mp_when)) {
	    	/*
	    	 * Start bus pause.
	    	 */
		mlx_pause_action(mlx);
		mlx->mlx_pause.mp_when = 0;
	} else if ((mlx->mlx_pause.mp_which != 0) &&
		   (mlx->mlx_pause.mp_when == 0)) {
		/*
		 * Stop pause if required.
		 */
		if (time_second >= mlx->mlx_pause.mp_howlong) {
			mlx_pause_action(mlx);
			mlx->mlx_pause.mp_which = 0;
		}
	} else if (time_second > (mlx->mlx_lastpoll + 10)) {
		/*
		 * Run normal periodic activities...
		 */
		mlx->mlx_lastpoll = time_second;

		/*
		 * Check controller status.
		 */
		if ((mlx->mlx_flags & MLXF_PERIODIC_CTLR) == 0) {
			mlx->mlx_flags |= MLXF_PERIODIC_CTLR;

			if (mlx->mlx_ci.ci_iftype <= 2)
				etype = MLX_CMD_ENQUIRY_OLD;
			else
				etype =  MLX_CMD_ENQUIRY;

			mlx_enquire(mlx, etype, max(sizeof(struct mlx_enquiry),
			    sizeof(struct mlx_enquiry_old)),
			    mlx_periodic_enquiry, 1);
		}

		/*
		 * Check system drive status.
		 */
		if ((mlx->mlx_flags & MLXF_PERIODIC_DRIVE) == 0) {
			mlx->mlx_flags |= MLXF_PERIODIC_DRIVE;
			mlx_enquire(mlx, MLX_CMD_ENQSYSDRIVE,
			    sizeof(struct mlx_enq_sys_drive) * MLX_MAX_DRIVES,
			    mlx_periodic_enquiry, 1);
		}
	}

	/*
	 * Get drive rebuild/check status.
	 */
	if ((mlx->mlx_flags & MLXF_PERIODIC_REBUILD) == 0) {
		mlx->mlx_flags |= MLXF_PERIODIC_REBUILD;
		mlx_enquire(mlx, MLX_CMD_REBUILDSTAT,
		    sizeof(struct mlx_rebuild_stat), mlx_periodic_rebuild, 1);
	}

	/*
	 * Time-out busy CCBs.
	 */
	s = splbio();
	for (mc = TAILQ_FIRST(&mlx->mlx_ccb_worklist); mc != NULL; mc = nmc) {
		nmc = TAILQ_NEXT(mc, mc_chain.tailq);
		if (mc->mc_expiry > time_second) {
			/*
			 * The remaining CCBs will expire after this one, so
			 * there's no point in going further.
			 */
			break;
		}
		TAILQ_REMOVE(&mlx->mlx_ccb_worklist, mc, mc_chain.tailq);
		mc->mc_status = MLX_STATUS_LOST;
		if (mc->mc_mx.mx_handler != NULL)
			(*mc->mc_mx.mx_handler)(mc);
		else if ((mc->mc_flags & MC_WAITING) != 0)
			wakeup(mc);
	}
	splx(s);
}

/*
 * Handle the result of an ENQUIRY command instigated by periodic status
 * polling.
 */
static void
mlx_periodic_enquiry(struct mlx_ccb *mc)
{
	struct mlx_softc *mlx;
	struct mlx_enquiry *me;
	struct mlx_enquiry_old *meo;
	struct mlx_enq_sys_drive *mes;
	struct mlx_sysdrive *dr;
	const char *statestr;
	int i, j;
	u_int lsn;

	mlx = device_private(mc->mc_mx.mx_dv);
	mlx_ccb_unmap(mlx, mc);

	/*
	 * Command completed OK?
	 */
	if (mc->mc_status != 0) {
		aprint_error_dev(mlx->mlx_dv, "periodic enquiry failed - %s\n",
		    mlx_ccb_diagnose(mc));
		goto out;
	}

	/*
	 * Respond to command.
	 */
	switch (mc->mc_mbox[0]) {
	case MLX_CMD_ENQUIRY_OLD:
		/*
		 * This is currently a bit fruitless, as we don't know how
		 * to extract the eventlog pointer yet.
		 */
		me = (struct mlx_enquiry *)mc->mc_mx.mx_context;
		meo = (struct mlx_enquiry_old *)mc->mc_mx.mx_context;

		/* Convert data in-place to new format */
		i = sizeof(me->me_dead) / sizeof(me->me_dead[0]);
		while (--i >= 0) {
			me->me_dead[i].dd_chan = meo->me_dead[i].dd_chan;
			me->me_dead[i].dd_targ = meo->me_dead[i].dd_targ;
		}

		me->me_misc_flags = 0;
		me->me_rebuild_count = meo->me_rebuild_count;
		me->me_dead_count = meo->me_dead_count;
		me->me_critical_sd_count = meo->me_critical_sd_count;
		me->me_event_log_seq_num = 0;
		me->me_offline_sd_count = meo->me_offline_sd_count;
		me->me_max_commands = meo->me_max_commands;
		me->me_rebuild_flag = meo->me_rebuild_flag;
		me->me_fwmajor = meo->me_fwmajor;
		me->me_fwminor = meo->me_fwminor;
		me->me_status_flags = meo->me_status_flags;
		me->me_flash_age = meo->me_flash_age;

		i = sizeof(me->me_drvsize) / sizeof(me->me_drvsize[0]);
		j = sizeof(meo->me_drvsize) / sizeof(meo->me_drvsize[0]);

		while (--i >= 0) {
			if (i >= j)
				me->me_drvsize[i] = 0;
			else
				me->me_drvsize[i] = meo->me_drvsize[i];
		}

		me->me_num_sys_drvs = meo->me_num_sys_drvs;

		/* FALLTHROUGH */

	case MLX_CMD_ENQUIRY:
		/*
		 * Generic controller status update.  We could do more with
		 * this than just checking the event log.
		 */
		me = (struct mlx_enquiry *)mc->mc_mx.mx_context;
		lsn = le16toh(me->me_event_log_seq_num);

		if (mlx->mlx_currevent == -1) {
			/* Initialise our view of the event log. */
			mlx->mlx_currevent = lsn;
			mlx->mlx_lastevent = lsn;
		} else if (lsn != mlx->mlx_lastevent &&
			   (mlx->mlx_flags & MLXF_EVENTLOG_BUSY) == 0) {
			/* Record where current events are up to */
			mlx->mlx_currevent = lsn;

			/* Mark the event log as busy. */
			mlx->mlx_flags |= MLXF_EVENTLOG_BUSY;

			/* Drain new eventlog entries. */
			mlx_periodic_eventlog_poll(mlx);
		}
		break;

	case MLX_CMD_ENQSYSDRIVE:
		/*
		 * Perform drive status comparison to see if something
		 * has failed.  Don't perform the comparison if we're
		 * reconfiguring, since the system drive table will be
		 * changing.
		 */
		if ((mlx->mlx_flags & MLXF_RESCANNING) != 0)
			break;

		mes = (struct mlx_enq_sys_drive *)mc->mc_mx.mx_context;
		dr = &mlx->mlx_sysdrive[0];

		for (i = 0; i < mlx->mlx_numsysdrives; i++, dr++) {
			/* Has state been changed by controller? */
			if (dr->ms_state != mes[i].sd_state) {
				switch (mes[i].sd_state) {
				case MLX_SYSD_OFFLINE:
					statestr = "offline";
					break;

				case MLX_SYSD_ONLINE:
					statestr = "online";
					break;

				case MLX_SYSD_CRITICAL:
					statestr = "critical";
					break;

				default:
					statestr = "unknown";
					break;
				}

				printf("%s: unit %d %s\n", device_xname(mlx->mlx_dv),
				    i, statestr);

				/* Save new state. */
				dr->ms_state = mes[i].sd_state;
			}
		}
		break;

#ifdef DIAGNOSTIC
	default:
		printf("%s: mlx_periodic_enquiry: eh?\n",
		    device_xname(mlx->mlx_dv));
		break;
#endif
	}

 out:
	if (mc->mc_mbox[0] == MLX_CMD_ENQSYSDRIVE)
		mlx->mlx_flags &= ~MLXF_PERIODIC_DRIVE;
	else
		mlx->mlx_flags &= ~MLXF_PERIODIC_CTLR;

	free(mc->mc_mx.mx_context, M_DEVBUF);
	mlx_ccb_free(mlx, mc);
}

/*
 * Instigate a poll for one event log message on (mlx).  We only poll for
 * one message at a time, to keep our command usage down.
 */
static void
mlx_periodic_eventlog_poll(struct mlx_softc *mlx)
{
	struct mlx_ccb *mc;
	void *result;
	int rv;

	result = NULL;

	if ((rv = mlx_ccb_alloc(mlx, &mc, 1)) != 0)
		goto out;

	if ((result = malloc(1024, M_DEVBUF, M_WAITOK)) == NULL) {
		rv = ENOMEM;
		goto out;
	}
	if ((rv = mlx_ccb_map(mlx, mc, result, 1024, MC_XFER_IN)) != 0)
		goto out;
	if (mc->mc_nsgent != 1) {
		mlx_ccb_unmap(mlx, mc);
		printf("mlx_periodic_eventlog_poll: too many segs\n");
		goto out;
	}

	/* Build the command to get one log entry. */
	mlx_make_type3(mc, MLX_CMD_LOGOP, MLX_LOGOP_GET, 1,
	    mlx->mlx_lastevent, 0, 0, mc->mc_xfer_phys, 0);

	mc->mc_mx.mx_handler = mlx_periodic_eventlog_respond;
	mc->mc_mx.mx_dv = mlx->mlx_dv;
	mc->mc_mx.mx_context = result;

	/* Start the command. */
	mlx_ccb_enqueue(mlx, mc);

 out:
	if (rv != 0) {
		if (mc != NULL)
			mlx_ccb_free(mlx, mc);
		if (result != NULL)
			free(result, M_DEVBUF);
	}
}

/*
 * Handle the result of polling for a log message, generate diagnostic
 * output.  If this wasn't the last message waiting for us, we'll go collect
 * another.
 */
static void
mlx_periodic_eventlog_respond(struct mlx_ccb *mc)
{
	struct mlx_softc *mlx;
	struct mlx_eventlog_entry *el;
	const char *reason;
	u_int8_t sensekey, chan, targ;

	mlx = device_private(mc->mc_mx.mx_dv);
	el = mc->mc_mx.mx_context;
	mlx_ccb_unmap(mlx, mc);

	mlx->mlx_lastevent++;

	if (mc->mc_status == 0) {
		switch (el->el_type) {
		case MLX_LOGMSG_SENSE:		/* sense data */
			sensekey = el->el_sense & 0x0f;
			chan = (el->el_target >> 4) & 0x0f;
			targ = el->el_target & 0x0f;

			/*
			 * This is the only sort of message we understand at
			 * the moment.  The tests here are probably
			 * incomplete.
			 */

			/*
			 * Mylex vendor-specific message indicating a drive
			 * was killed?
			 */
			if (sensekey == 9 && el->el_asc == 0x80) {
				if (el->el_asq < sizeof(mlx_sense_msgs) /
				    sizeof(mlx_sense_msgs[0]))
					reason = mlx_sense_msgs[el->el_asq];
				else
					reason = "for unknown reason";

				printf("%s: physical drive %d:%d killed %s\n",
				    device_xname(mlx->mlx_dv), chan, targ, reason);
			}

			/*
			 * SCSI drive was reset?
			 */
			if (sensekey == 6 && el->el_asc == 0x29)
				printf("%s: physical drive %d:%d reset\n",
				    device_xname(mlx->mlx_dv), chan, targ);

			/*
			 * SCSI drive error?
			 */
			if (!(sensekey == 0 ||
			    (sensekey == 2 &&
			    el->el_asc == 0x04 &&
			    (el->el_asq == 0x01 || el->el_asq == 0x02)))) {
				printf("%s: physical drive %d:%d error log: "
				    "sense = %d asc = %x asq = %x\n",
				    device_xname(mlx->mlx_dv), chan, targ, sensekey,
				    el->el_asc, el->el_asq);
				printf("%s:   info = %d:%d:%d:%d "
				    " csi = %d:%d:%d:%d\n",
				    device_xname(mlx->mlx_dv),
				    el->el_information[0],
				    el->el_information[1],
				    el->el_information[2],
				    el->el_information[3],
				    el->el_csi[0], el->el_csi[1],
				    el->el_csi[2], el->el_csi[3]);
			}

			break;

		default:
			aprint_error_dev(mlx->mlx_dv, "unknown log message type 0x%x\n",
			    el->el_type);
			break;
		}
	} else {
		aprint_error_dev(mlx->mlx_dv, "error reading message log - %s\n",
		    mlx_ccb_diagnose(mc));

		/*
		 * Give up on all the outstanding messages, as we may have
		 * come unsynched.
		 */
		mlx->mlx_lastevent = mlx->mlx_currevent;
	}

	free(mc->mc_mx.mx_context, M_DEVBUF);
	mlx_ccb_free(mlx, mc);

	/*
	 * Is there another message to obtain?
	 */
	if (mlx->mlx_lastevent != mlx->mlx_currevent)
		mlx_periodic_eventlog_poll(mlx);
	else
		mlx->mlx_flags &= ~MLXF_EVENTLOG_BUSY;
}

/*
 * Handle check/rebuild operations in progress.
 */
static void
mlx_periodic_rebuild(struct mlx_ccb *mc)
{
	struct mlx_softc *mlx;
	const char *opstr;
	struct mlx_rebuild_status *mr;

	mlx = device_private(mc->mc_mx.mx_dv);
	mr = mc->mc_mx.mx_context;
	mlx_ccb_unmap(mlx, mc);

	switch (mc->mc_status) {
	case 0:
		/*
		 * Operation running, update stats.
		 */
		mlx->mlx_rebuildstat = *mr;

		/* Spontaneous rebuild/check? */
		if (mlx->mlx_bg == 0) {
			mlx->mlx_bg = MLX_BG_SPONTANEOUS;
			printf("%s: background check/rebuild started\n",
			    device_xname(mlx->mlx_dv));
		}
		break;

	case 0x0105:
		/*
		 * Nothing running, finalise stats and report.
		 */
		switch (mlx->mlx_bg) {
		case MLX_BG_CHECK:
			/* XXX Print drive? */
			opstr = "consistency check";
			break;

		case MLX_BG_REBUILD:
			/* XXX Print channel:target? */
			opstr = "drive rebuild";
			break;

		case MLX_BG_SPONTANEOUS:
		default:
			/*
			 * If we have previously been non-idle, report the
			 * transition
			 */
			if (mlx->mlx_rebuildstat.rs_code !=
			    MLX_REBUILDSTAT_IDLE)
				opstr = "background check/rebuild";
			else
				opstr = NULL;
		}

		if (opstr != NULL)
			printf("%s: %s completed\n", device_xname(mlx->mlx_dv),
			    opstr);

		mlx->mlx_bg = 0;
		mlx->mlx_rebuildstat.rs_code = MLX_REBUILDSTAT_IDLE;
		break;
	}

	free(mc->mc_mx.mx_context, M_DEVBUF);
	mlx_ccb_free(mlx, mc);
	mlx->mlx_flags &= ~MLXF_PERIODIC_REBUILD;
}

/*
 * It's time to perform a channel pause action for (mlx), either start or
 * stop the pause.
 */
static void
mlx_pause_action(struct mlx_softc *mlx)
{
	struct mlx_ccb *mc;
	int failsafe, i, cmd;

	/* What are we doing here? */
	if (mlx->mlx_pause.mp_when == 0) {
		cmd = MLX_CMD_STARTCHANNEL;
		failsafe = 0;
	} else {
		cmd = MLX_CMD_STOPCHANNEL;

		/*
		 * Channels will always start again after the failsafe
		 * period, which is specified in multiples of 30 seconds.
		 * This constrains us to a maximum pause of 450 seconds.
		 */
		failsafe = ((mlx->mlx_pause.mp_howlong - time_second) + 5) / 30;

		if (failsafe > 0xf) {
			failsafe = 0xf;
			mlx->mlx_pause.mp_howlong =
			     time_second + (0xf * 30) - 5;
		}
	}

	/* Build commands for every channel requested. */
	for (i = 0; i < mlx->mlx_ci.ci_nchan; i++) {
		if ((1 << i) & mlx->mlx_pause.mp_which) {
			if (mlx_ccb_alloc(mlx, &mc, 1) != 0) {
				aprint_error_dev(mlx->mlx_dv, "%s failed for channel %d\n",
				    cmd == MLX_CMD_STOPCHANNEL ?
				    "pause" : "resume", i);
				continue;
			}

			/* Build the command. */
			mlx_make_type2(mc, cmd, (failsafe << 4) | i, 0, 0,
			    0, 0, 0, 0, 0);
			mc->mc_mx.mx_handler = mlx_pause_done;
			mc->mc_mx.mx_dv = mlx->mlx_dv;

			mlx_ccb_enqueue(mlx, mc);
		}
	}
}

static void
mlx_pause_done(struct mlx_ccb *mc)
{
	struct mlx_softc *mlx;
	int command, channel;

	mlx = device_private(mc->mc_mx.mx_dv);
	command = mc->mc_mbox[0];
	channel = mc->mc_mbox[2] & 0xf;

	if (mc->mc_status != 0)
		aprint_error_dev(mlx->mlx_dv, "%s command failed - %s\n",
		    command == MLX_CMD_STOPCHANNEL ? "pause" : "resume",
		    mlx_ccb_diagnose(mc));
	else if (command == MLX_CMD_STOPCHANNEL)
		printf("%s: channel %d pausing for %ld seconds\n",
		    device_xname(mlx->mlx_dv), channel,
		    (long)(mlx->mlx_pause.mp_howlong - time_second));
	else
		printf("%s: channel %d resuming\n", device_xname(mlx->mlx_dv),
		    channel);

	mlx_ccb_free(mlx, mc);
}

/*
 * Perform an Enquiry command using a type-3 command buffer and a return a
 * single linear result buffer.  If the completion function is specified, it
 * will be called with the completed command (and the result response will
 * not be valid until that point).  Otherwise, the command will either be
 * busy-waited for (interrupts must be blocked), or slept for.
 */
static void *
mlx_enquire(struct mlx_softc *mlx, int command, size_t bufsize,
	    void (*handler)(struct mlx_ccb *mc), int waitok)
{
	struct mlx_ccb *mc;
	void *result;
	int rv, mapped;

	result = NULL;
	mapped = 0;

	if ((rv = mlx_ccb_alloc(mlx, &mc, 1)) != 0)
		goto out;

	result = malloc(bufsize, M_DEVBUF, waitok ? M_WAITOK : M_NOWAIT);
	if (result == NULL) {
		printf("mlx_enquire: malloc() failed\n");
		goto out;
	}
	if ((rv = mlx_ccb_map(mlx, mc, result, bufsize, MC_XFER_IN)) != 0)
		goto out;
	mapped = 1;
	if (mc->mc_nsgent != 1) {
		printf("mlx_enquire: too many segs\n");
		goto out;
	}

	/* Build an enquiry command. */
	mlx_make_type2(mc, command, 0, 0, 0, 0, 0, 0, mc->mc_xfer_phys, 0);

	/* Do we want a completion callback? */
	if (handler != NULL) {
		mc->mc_mx.mx_context = result;
		mc->mc_mx.mx_dv = mlx->mlx_dv;
		mc->mc_mx.mx_handler = handler;
		mlx_ccb_enqueue(mlx, mc);
	} else {
		/* Run the command in either polled or wait mode. */
		if (waitok)
			rv = mlx_ccb_wait(mlx, mc);
		else
			rv = mlx_ccb_poll(mlx, mc, 5000);
	}

 out:
	/* We got a command, but nobody else will free it. */
	if (handler == NULL && mc != NULL) {
		if (mapped)
			mlx_ccb_unmap(mlx, mc);
		mlx_ccb_free(mlx, mc);
	}

	/* We got an error, and we allocated a result. */
	if (rv != 0 && result != NULL) {
		if (mc != NULL)
			mlx_ccb_free(mlx, mc);
		free(result, M_DEVBUF);
		result = NULL;
	}

	return (result);
}

/*
 * Perform a Flush command on the nominated controller.
 *
 * May be called with interrupts enabled or disabled; will not return until
 * the flush operation completes or fails.
 */
int
mlx_flush(struct mlx_softc *mlx, int async)
{
	struct mlx_ccb *mc;
	int rv;

	if ((rv = mlx_ccb_alloc(mlx, &mc, 1)) != 0)
		goto out;

	/* Build a flush command and fire it off. */
	mlx_make_type2(mc, MLX_CMD_FLUSH, 0, 0, 0, 0, 0, 0, 0, 0);

	if (async)
		rv = mlx_ccb_wait(mlx, mc);
	else
		rv = mlx_ccb_poll(mlx, mc, MLX_TIMEOUT * 1000);
	if (rv != 0)
		goto out;

	/* Command completed OK? */
	if (mc->mc_status != 0) {
		aprint_error_dev(mlx->mlx_dv, "FLUSH failed - %s\n",
		    mlx_ccb_diagnose(mc));
		rv = EIO;
	}
 out:
	if (mc != NULL)
		mlx_ccb_free(mlx, mc);

	return (rv);
}

/*
 * Start a background consistency check on (drive).
 */
static int
mlx_check(struct mlx_softc *mlx, int drive)
{
	struct mlx_ccb *mc;
	int rv;

	/* Get ourselves a command buffer. */
	rv = 0x10000;

	if (mlx_ccb_alloc(mlx, &mc, 1) != 0)
		goto out;

	/* Build a checkasync command, set the "fix it" flag. */
	mlx_make_type2(mc, MLX_CMD_CHECKASYNC, 0, 0, 0, 0, 0, drive | 0x80,
	    0, 0);

	/* Start the command and wait for it to be returned. */
	if (mlx_ccb_wait(mlx, mc) != 0)
		goto out;

	/* Command completed OK? */
	if (mc->mc_status != 0)
		aprint_error_dev(mlx->mlx_dv, "CHECK ASYNC failed - %s\n",
		    mlx_ccb_diagnose(mc));
	else
		printf("%s: consistency check started",
		    device_xname(mlx->mlx_sysdrive[drive].ms_dv));

	rv = mc->mc_status;
 out:
	if (mc != NULL)
		mlx_ccb_free(mlx, mc);

	return (rv);
}

/*
 * Start a background rebuild of the physical drive at (channel),(target).
 *
 * May be called with interrupts enabled or disabled; will return as soon as
 * the operation has started or been refused.
 */
static int
mlx_rebuild(struct mlx_softc *mlx, int channel, int target)
{
	struct mlx_ccb *mc;
	int error;

	error = 0x10000;
	if (mlx_ccb_alloc(mlx, &mc, 1) != 0)
		goto out;

	/* Build a rebuildasync command, set the "fix it" flag. */
	mlx_make_type2(mc, MLX_CMD_REBUILDASYNC, channel, target, 0, 0, 0, 0,
	    0, 0);

	/* Start the command and wait for it to be returned. */
	if (mlx_ccb_wait(mlx, mc) != 0)
		goto out;

	/* Command completed OK? */
	aprint_normal_dev(mlx->mlx_dv, "");
	if (mc->mc_status != 0)
		printf("REBUILD ASYNC failed - %s\n", mlx_ccb_diagnose(mc));
	else
		printf("rebuild started for %d:%d\n", channel, target);

	error = mc->mc_status;

 out:
	if (mc != NULL)
		mlx_ccb_free(mlx, mc);

	return (error);
}

/*
 * Take a command from user-space and try to run it.
 *
 * XXX Note that this can't perform very much in the way of error checking,
 * XXX and as such, applications _must_ be considered trustworthy.
 *
 * XXX Commands using S/G for data are not supported.
 */
static int
mlx_user_command(struct mlx_softc *mlx, struct mlx_usercommand *mu)
{
	struct mlx_ccb *mc;
	struct mlx_dcdb *dcdb;
	void *kbuf;
	int rv, mapped;

	if ((mu->mu_bufdir & ~MU_XFER_MASK) != 0)
		return (EINVAL);

	kbuf = NULL;
	dcdb = NULL;
	mapped = 0;

	/* Get ourselves a command and copy in from user space. */
	if ((rv = mlx_ccb_alloc(mlx, &mc, 1)) != 0) {
		DPRINTF(("mlx_user_command: mlx_ccb_alloc = %d\n", rv));
		goto out;
	}

	memcpy(mc->mc_mbox, mu->mu_command, sizeof(mc->mc_mbox));

	/*
	 * If we need a buffer for data transfer, allocate one and copy in
	 * its initial contents.
	 */
	if (mu->mu_datasize > 0) {
		if (mu->mu_datasize > MAXPHYS)
			return (EINVAL);

		kbuf = malloc(mu->mu_datasize, M_DEVBUF, M_WAITOK);
		if (kbuf == NULL) {
			DPRINTF(("mlx_user_command: malloc = NULL\n"));
			rv = ENOMEM;
			goto out;
		}

		if ((mu->mu_bufdir & MU_XFER_OUT) != 0) {
			rv = copyin(mu->mu_buf, kbuf, mu->mu_datasize);
			if (rv != 0) {
				DPRINTF(("mlx_user_command: copyin = %d\n",
				    rv));
				goto out;
			}
		}

		/* Map the buffer so the controller can see it. */
		rv = mlx_ccb_map(mlx, mc, kbuf, mu->mu_datasize, mu->mu_bufdir);
		if (rv != 0) {
			DPRINTF(("mlx_user_command: mlx_ccb_map = %d\n", rv));
			goto out;
		}
		if (mc->mc_nsgent > 1) {
			DPRINTF(("mlx_user_command: too many s/g entries\n"));
			rv = EFBIG;
			goto out;
		}
		mapped = 1;
		/*
		 * If this is a passthrough SCSI command, the DCDB is packed at
		 * the beginning of the data area.  Fix up the DCDB to point to
		 * the correct physical address and override any bufptr
		 * supplied by the caller since we know what it's meant to be.
		 */
		if (mc->mc_mbox[0] == MLX_CMD_DIRECT_CDB) {
			dcdb = (struct mlx_dcdb *)kbuf;
			dcdb->dcdb_physaddr = mc->mc_xfer_phys + sizeof(*dcdb);
			mu->mu_bufptr = 8;
		}
	}


	/*
	 * If there's a data buffer, fix up the command's buffer pointer.
	 */
	if (mu->mu_datasize > 0) {
		/* Range check the pointer to physical buffer address. */
		if (mu->mu_bufptr < 0 ||
		    mu->mu_bufptr > sizeof(mu->mu_command) - 4) {
			DPRINTF(("mlx_user_command: bufptr botch\n"));
			rv = EINVAL;
			goto out;
		}

		mc->mc_mbox[mu->mu_bufptr] = mc->mc_xfer_phys;
		mc->mc_mbox[mu->mu_bufptr+1] = mc->mc_xfer_phys >> 8;
		mc->mc_mbox[mu->mu_bufptr+2] = mc->mc_xfer_phys >> 16;
		mc->mc_mbox[mu->mu_bufptr+3] = mc->mc_xfer_phys >> 24;
	}

	/* Submit the command and wait. */
	if ((rv = mlx_ccb_wait(mlx, mc)) != 0) {
#ifdef DEBUG
		printf("mlx_user_command: mlx_ccb_wait = %d\n", rv);
#endif
	}

 out:
	if (mc != NULL) {
		/* Copy out status and data */
		mu->mu_status = mc->mc_status;
		if (mapped)
			mlx_ccb_unmap(mlx, mc);
		mlx_ccb_free(mlx, mc);
	}

	if (kbuf != NULL) {
		if (mu->mu_datasize > 0 && (mu->mu_bufdir & MU_XFER_IN) != 0) {
			rv = copyout(kbuf, mu->mu_buf, mu->mu_datasize);
#ifdef DIAGNOSTIC
			if (rv != 0)
				printf("mlx_user_command: copyout = %d\n", rv);
#endif
		}
	}
	if (kbuf != NULL)
		free(kbuf, M_DEVBUF);

	return (rv);
}

/*
 * Allocate and initialise a CCB.
 */
int
mlx_ccb_alloc(struct mlx_softc *mlx, struct mlx_ccb **mcp, int control)
{
	struct mlx_ccb *mc;
	int s;

	s = splbio();
	mc = SLIST_FIRST(&mlx->mlx_ccb_freelist);
	if (control) {
		if (mlx->mlx_nccbs_ctrl >= MLX_NCCBS_CONTROL) {
			splx(s);
			*mcp = NULL;
			return (EAGAIN);
		}
		mc->mc_flags |= MC_CONTROL;
		mlx->mlx_nccbs_ctrl++;
	}
	SLIST_REMOVE_HEAD(&mlx->mlx_ccb_freelist, mc_chain.slist);
	splx(s);

	*mcp = mc;
	return (0);
}

/*
 * Free a CCB.
 */
void
mlx_ccb_free(struct mlx_softc *mlx, struct mlx_ccb *mc)
{
	int s;

	s = splbio();
	if ((mc->mc_flags & MC_CONTROL) != 0)
		mlx->mlx_nccbs_ctrl--;
	mc->mc_flags = 0;
	SLIST_INSERT_HEAD(&mlx->mlx_ccb_freelist, mc, mc_chain.slist);
	splx(s);
}

/*
 * If a CCB is specified, enqueue it.  Pull CCBs off the software queue in
 * the order that they were enqueued and try to submit their mailboxes to
 * the controller for execution.
 */
void
mlx_ccb_enqueue(struct mlx_softc *mlx, struct mlx_ccb *mc)
{
	int s;

	s = splbio();

	if (mc != NULL)
		SIMPLEQ_INSERT_TAIL(&mlx->mlx_ccb_queue, mc, mc_chain.simpleq);

	while ((mc = SIMPLEQ_FIRST(&mlx->mlx_ccb_queue)) != NULL) {
		if (mlx_ccb_submit(mlx, mc) != 0)
			break;
		SIMPLEQ_REMOVE_HEAD(&mlx->mlx_ccb_queue, mc_chain.simpleq);
		TAILQ_INSERT_TAIL(&mlx->mlx_ccb_worklist, mc, mc_chain.tailq);
	}

	splx(s);
}

/*
 * Map the specified CCB's data buffer onto the bus, and fill the
 * scatter-gather list.
 */
int
mlx_ccb_map(struct mlx_softc *mlx, struct mlx_ccb *mc, void *data, int size,
	    int dir)
{
	struct mlx_sgentry *sge;
	int nsegs, i, rv, sgloff;
	bus_dmamap_t xfer;

	xfer = mc->mc_xfer_map;

	rv = bus_dmamap_load(mlx->mlx_dmat, xfer, data, size, NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
	    ((dir & MC_XFER_IN) ? BUS_DMA_READ : BUS_DMA_WRITE));
	if (rv != 0)
		return (rv);

	nsegs = xfer->dm_nsegs;
	mc->mc_xfer_size = size;
	mc->mc_flags |= dir;
	mc->mc_nsgent = nsegs;
	mc->mc_xfer_phys = xfer->dm_segs[0].ds_addr;

	sgloff = MLX_SGL_SIZE * mc->mc_ident;
	sge = (struct mlx_sgentry *)((char *)mlx->mlx_sgls + sgloff);

	for (i = 0; i < nsegs; i++, sge++) {
		sge->sge_addr = htole32(xfer->dm_segs[i].ds_addr);
		sge->sge_count = htole32(xfer->dm_segs[i].ds_len);
	}

	if ((dir & MC_XFER_OUT) != 0)
		i = BUS_DMASYNC_PREWRITE;
	else
		i = 0;
	if ((dir & MC_XFER_IN) != 0)
		i |= BUS_DMASYNC_PREREAD;

	bus_dmamap_sync(mlx->mlx_dmat, xfer, 0, mc->mc_xfer_size, i);
	bus_dmamap_sync(mlx->mlx_dmat, mlx->mlx_dmamap, sgloff,
	    MLX_SGL_SIZE, BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Unmap the specified CCB's data buffer.
 */
void
mlx_ccb_unmap(struct mlx_softc *mlx, struct mlx_ccb *mc)
{
	int i;

	bus_dmamap_sync(mlx->mlx_dmat, mlx->mlx_dmamap,
	    MLX_SGL_SIZE * mc->mc_ident, MLX_SGL_SIZE,
	    BUS_DMASYNC_POSTWRITE);

	if ((mc->mc_flags & MC_XFER_OUT) != 0)
		i = BUS_DMASYNC_POSTWRITE;
	else
		i = 0;
	if ((mc->mc_flags & MC_XFER_IN) != 0)
		i |= BUS_DMASYNC_POSTREAD;

	bus_dmamap_sync(mlx->mlx_dmat, mc->mc_xfer_map, 0, mc->mc_xfer_size, i);
	bus_dmamap_unload(mlx->mlx_dmat, mc->mc_xfer_map);
}

/*
 * Submit the CCB, and busy-wait for it to complete.  Return non-zero on
 * timeout or submission error.  Must be called with interrupts blocked.
 */
int
mlx_ccb_poll(struct mlx_softc *mlx, struct mlx_ccb *mc, int timo)
{
	int rv;

	mc->mc_mx.mx_handler = NULL;

	if ((rv = mlx_ccb_submit(mlx, mc)) != 0)
		return (rv);
	TAILQ_INSERT_TAIL(&mlx->mlx_ccb_worklist, mc, mc_chain.tailq);

	for (timo *= 10; timo != 0; timo--) {
		mlx_intr(mlx);
		if (mc->mc_status != MLX_STATUS_BUSY)
			break;
		DELAY(100);
	}

	if (timo != 0) {
		if (mc->mc_status != 0) {
			aprint_error_dev(mlx->mlx_dv, "command failed - %s\n",
			    mlx_ccb_diagnose(mc));
			rv = EIO;
		} else
			rv = 0;
	} else {
		printf("%s: command timed out\n", device_xname(mlx->mlx_dv));
		rv = EIO;
	}

	return (rv);
}

/*
 * Enqueue the CCB, and sleep until it completes.  Return non-zero on
 * timeout or error.
 */
int
mlx_ccb_wait(struct mlx_softc *mlx, struct mlx_ccb *mc)
{
	int s;

	mc->mc_flags |= MC_WAITING;
	mc->mc_mx.mx_handler = NULL;

	s = splbio();
	mlx_ccb_enqueue(mlx, mc);
	tsleep(mc, PRIBIO, "mlxwccb", 0);
	splx(s);

	if (mc->mc_status != 0) {
		aprint_error_dev(mlx->mlx_dv, "command failed - %s\n",
		    mlx_ccb_diagnose(mc));
		return (EIO);
	}

	return (0);
}

/*
 * Try to submit a CCB's mailbox to the controller for execution.  Return
 * non-zero on timeout or error.  Must be called with interrupts blocked.
 */
static int
mlx_ccb_submit(struct mlx_softc *mlx, struct mlx_ccb *mc)
{
	int i, s, r;

	/* Save the ident so we can handle this command when complete. */
	mc->mc_mbox[1] = (u_int8_t)(mc->mc_ident + 1);

	/* Mark the command as currently being processed. */
	mc->mc_status = MLX_STATUS_BUSY;
	mc->mc_expiry = time_second + MLX_TIMEOUT;

	/* Spin waiting for the mailbox. */
	for (i = 100; i != 0; i--) {
		s = splbio();
		r = (*mlx->mlx_submit)(mlx, mc);
		splx(s);
		if (r != 0)
			break;
		DELAY(100);
	}
	if (i != 0)
		return (0);

	DPRINTF(("mlx_ccb_submit: rejected; queueing\n"));
	mc->mc_status = MLX_STATUS_WEDGED;
	return (EIO);
}

/*
 * Return a string that describes why a command has failed.
 */
const char *
mlx_ccb_diagnose(struct mlx_ccb *mc)
{
	static char tbuf[80];
	int i;

	for (i = 0; i < sizeof(mlx_msgs) / sizeof(mlx_msgs[0]); i++)
		if ((mc->mc_mbox[0] == mlx_msgs[i].command ||
		    mlx_msgs[i].command == 0) &&
		    mc->mc_status == mlx_msgs[i].status) {
			snprintf(tbuf, sizeof(tbuf), "%s (0x%x)",
			    mlx_status_msgs[mlx_msgs[i].msg], mc->mc_status);
			return (tbuf);
		}

	snprintf(tbuf, sizeof(tbuf), "unknown response 0x%x for command 0x%x",
	    (int)mc->mc_status, (int)mc->mc_mbox[0]);

	return (tbuf);
}

/*
 * Poll the controller for completed commands.  Returns non-zero if one or
 * more commands were completed.  Must be called with interrupts blocked.
 */
int
mlx_intr(void *cookie)
{
	struct mlx_softc *mlx;
	struct mlx_ccb *mc;
	int result;
	u_int ident, status;

	mlx = cookie;
	result = 0;

	while ((*mlx->mlx_findcomplete)(mlx, &ident, &status) != 0) {
		result = 1;
		ident--;

		if (ident >= MLX_MAX_QUEUECNT) {
			aprint_error_dev(mlx->mlx_dv, "bad completion returned\n");
			continue;
		}

		mc = mlx->mlx_ccbs + ident;

		if (mc->mc_status != MLX_STATUS_BUSY) {
			aprint_error_dev(mlx->mlx_dv, "bad completion returned\n");
			continue;
		}

		TAILQ_REMOVE(&mlx->mlx_ccb_worklist, mc, mc_chain.tailq);

		/* Record status and notify the initiator, if requested. */
		mc->mc_status = status;
		if (mc->mc_mx.mx_handler != NULL)
			(*mc->mc_mx.mx_handler)(mc);
		else if ((mc->mc_flags & MC_WAITING) != 0)
			wakeup(mc);
	}

	/* If we've completed any commands, try posting some more. */
	if (result)
		mlx_ccb_enqueue(mlx, NULL);

	return (result);
}

/*
 * Emit a string describing the firmware handshake status code, and return a
 * flag indicating whether the code represents a fatal error.
 *
 * Error code interpretations are from the Linux driver, and don't directly
 * match the messages printed by Mylex's BIOS.  This may change if
 * documentation on the codes is forthcoming.
 */
static int
mlx_fw_message(struct mlx_softc *mlx, int error, int param1, int param2)
{
	const char *fmt;

	switch (error) {
	case 0x00:
		fmt = "physical drive %d:%d not responding";
		break;

	case 0x08:
		/*
		 * We could be neater about this and give some indication
		 * when we receive more of them.
		 */
		if ((mlx->mlx_flags & MLXF_SPINUP_REPORTED) == 0) {
			printf("%s: spinning up drives...\n",
			    device_xname(mlx->mlx_dv));
			mlx->mlx_flags |= MLXF_SPINUP_REPORTED;
		}
		return (0);

	case 0x30:
		fmt = "configuration checksum error";
		break;

	case 0x60:
		fmt = "mirror race recovery failed";
		break;

	case 0x70:
		fmt = "mirror race recovery in progress";
		break;

	case 0x90:
		fmt = "physical drive %d:%d COD mismatch";
		break;

	case 0xa0:
		fmt = "logical drive installation aborted";
		break;

	case 0xb0:
		fmt = "mirror race on a critical system drive";
		break;

	case 0xd0:
		fmt = "new controller configuration found";
		break;

	case 0xf0:
		aprint_error_dev(mlx->mlx_dv, "FATAL MEMORY PARITY ERROR\n");
		return (1);

	default:
		aprint_error_dev(mlx->mlx_dv, "unknown firmware init error %02x:%02x:%02x\n",
		    error, param1, param2);
		return (0);
	}

	aprint_normal_dev(mlx->mlx_dv, "");
	aprint_normal(fmt, param2, param1);
	aprint_normal("\n");

	return (0);
}
