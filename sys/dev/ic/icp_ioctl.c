/*	$NetBSD: icp_ioctl.c,v 1.21 2014/07/25 08:10:37 dholland Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of Wasabi Systems, Inc.
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

/*
 *       Copyright (c) 2000-01 Intel Corporation
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * icp_ioctl.c: Ioctl interface for the ICP-Vortex management tools.
 *
 * Based on ICP's FreeBSD "iir" driver ioctl interface, written by
 * Achim Leubner <achim.leubner@intel.com>.
 *
 * This is intended to be ABI-compatile with the ioctl interface for
 * other OSs.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: icp_ioctl.c,v 1.21 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/kauth.h>

#include <sys/bus.h>

#include <dev/ic/icpreg.h>
#include <dev/ic/icpvar.h>

/* These are simply the same as ICP's "iir" driver for FreeBSD. */
#define	ICP_DRIVER_VERSION		1
#define	ICP_DRIVER_SUBVERSION		3

static dev_type_open(icpopen);
static dev_type_ioctl(icpioctl);

const struct cdevsw icp_cdevsw = {
	.d_open = icpopen,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = icpioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

extern struct cfdriver icp_cd;

kmutex_t icp_ioctl_mutex;

static int
icpopen(dev_t dev, int flag, int mode, struct lwp *l)
{

	if (device_lookup(&icp_cd, minor(dev)) == NULL)
		return (ENXIO);

	return (0);
}

static int
icpioctl(dev_t dev, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	int error = 0;

	mutex_enter(&icp_ioctl_mutex);

	switch (cmd) {
	case GDT_IOCTL_GENERAL:
	    {
		struct icp_softc *icp;
		gdt_ucmd_t *ucmd = (void *) data;

		error = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL, data);
		if (error)
			break;

		icp = device_lookup_private(&icp_cd, ucmd->io_node);
		if (icp == NULL) {
			error = ENXIO;
			break;
		}

		error = icp_ucmd(icp, ucmd);
		break;
	    }

	case GDT_IOCTL_DRVERS:
		*(int *) data =
		    (ICP_DRIVER_VERSION << 8) | ICP_DRIVER_SUBVERSION;
		break;

	case GDT_IOCTL_CTRTYPE:
	    {
		struct icp_softc *icp;
		gdt_ctrt_t *ctrt = (void *) data;

		icp = device_lookup_private(&icp_cd, ctrt->io_node);
		if (icp == NULL) {
			error = ENXIO;
			break;
		}

		/* XXX magic numbers */
		ctrt->oem_id = 0x8000;
		ctrt->type = 0xfd;
		ctrt->info = (icp->icp_pci_bus << 8) | (icp->icp_pci_device << 3);
		ctrt->ext_type = 0x6000 | icp->icp_pci_subdevice_id;
		ctrt->device_id = icp->icp_pci_device_id;
		ctrt->sub_device_id = icp->icp_pci_subdevice_id;
		break;
	    }

	case GDT_IOCTL_OSVERS:
	    {
		gdt_osv_t *osv = (void *) data;

		osv->oscode = 12;

		/*
		 * __NetBSD_Version__ is encoded thusly:
		 *
		 *	MMmmrrpp00
		 *
		 * M = major version
		 * m = minor version
		 * r = release ["",A-Z[A-Z] but numeric]
		 * p = patchlevel
		 *
		 * Since the ABI is not supposed to change between
		 * patchlevels of the same major/minor version, we
		 * will encode major/minor/release into the returned
		 * data.
		 */

		osv->version = __NetBSD_Version__ / 100000000;
		osv->subversion = (__NetBSD_Version__ / 1000000) % 100;
		osv->revision = (__NetBSD_Version__ / 10000) % 100;

		strcpy(osv->name, ostype);
		break;
	    }

	case GDT_IOCTL_CTRCNT:
		*(int *) data = icp_count;
		break;

	case GDT_IOCTL_EVENT:
	    {
		struct icp_softc *icp;
		gdt_event_t *evt = (void *) data;
		gdt_evt_str *e = &evt->dvr;
		int s;

		icp = device_lookup_private(&icp_cd, minor(dev));

		switch (evt->erase) {
		case 0xff:
			switch (evt->dvr.event_source) {
			case GDT_ES_TEST:
				e->event_data.size =
				    sizeof(e->event_data.eu.test);
				break;

			case GDT_ES_DRIVER:
				e->event_data.size =
				    sizeof(e->event_data.eu.driver);
				break;

			case GDT_ES_SYNC:
				e->event_data.size =
				    sizeof(e->event_data.eu.sync);
				break;

			default:
				e->event_data.size =
				    sizeof(e->event_data.eu.async);
				break;
			}
			s = splbio();
			icp_store_event(icp, e->event_source, e->event_idx,
			    &e->event_data);
			splx(s);
			break;

		case 0xfe:
			s = splbio();
			icp_clear_events(icp);
			splx(s);
			break;

		case 0:
			evt->handle = icp_read_event(icp, evt->handle, e);
			break;

		default:
			icp_readapp_event(icp, (u_int8_t) evt->erase, e);
			break;
		}
		break;
	    }

	case GDT_IOCTL_STATIST:
		memcpy(&icp_stats, data, sizeof(gdt_statist_t));
		break;


	case GDT_IOCTL_RESCAN:
	    {
		struct icp_softc *icp;
		gdt_rescan_t *rsc = (void *) data;

		icp = device_lookup_private(&icp_cd, rsc->io_node);
		if (icp == NULL) {
			error = ENXIO;
			break;
		}

		error = icp_freeze(icp);
		if (error)
			break;
		if (rsc->flag == 0)
			icp_rescan_all(icp);
		else
			icp_rescan(icp, rsc->hdr_no);
		icp_unfreeze(icp);
		break;
	    }

	default:
		error = ENOTTY;
	}

	mutex_exit(&icp_ioctl_mutex);

	return (error);
}
