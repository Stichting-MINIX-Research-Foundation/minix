/*	$NetBSD: bio.c,v 1.13 2015/08/20 14:40:17 christos Exp $ */
/*	$OpenBSD: bio.c,v 1.9 2007/03/20 02:35:55 marco Exp $	*/

/*
 * Copyright (c) 2002 Niklas Hallqvist.  All rights reserved.
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

/* A device controller ioctl tunnelling device.  */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bio.c,v 1.13 2015/08/20 14:40:17 christos Exp $");

#include "opt_compat_netbsd.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kauth.h>

#include <dev/biovar.h>
#include <dev/sysmon/sysmonvar.h>

#include "ioconf.h"

struct bio_mapping {
	LIST_ENTRY(bio_mapping) bm_link;
	device_t bm_dev;
	int (*bm_ioctl)(device_t, u_long, void *);
};

static LIST_HEAD(, bio_mapping) bios = LIST_HEAD_INITIALIZER(bios);
static kmutex_t bio_lock;
static bool bio_lock_initialized = false;

static void	bio_initialize(void);
static int	bioclose(dev_t, int, int, struct lwp *);
static int	bioioctl(dev_t, u_long, void *, int, struct lwp *);
static int	bioopen(dev_t, int, int, struct lwp *);

static int	bio_delegate_ioctl(void *, u_long, void *);
static struct	bio_mapping *bio_lookup(char *);
static int	bio_validate(void *);

const struct cdevsw bio_cdevsw = {
        .d_open = bioopen,
	.d_close = bioclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = bioioctl,
        .d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};


static void
bio_initialize(void)
{
	if (bio_lock_initialized)
		return;

	mutex_init(&bio_lock, MUTEX_DEFAULT, IPL_VM);
	bio_lock_initialized = true;
}

void
bioattach(int nunits)
{
	if (!bio_lock_initialized)
		bio_initialize();
}

static int
bioopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	return 0;
}

static int
bioclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	return 0;
}

static int
bioioctl(dev_t dev, u_long cmd, void *addr, int flag, struct  lwp *l)
{
	struct bio_locate *locate;
	struct bio_common *common;
	char name[16];
	int error;

	switch(cmd) {
	case BIOCLOCATE:
	case BIOCINQ:
	case BIOCDISK:
	case BIOCDISK_NOVOL:
	case BIOCVOL:
#ifdef COMPAT_30
	case OBIOCDISK:
	case OBIOCVOL:
#endif
		error = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_READCONF, addr);
		if (error)
			return error;
		break;
	case BIOCBLINK:
	case BIOCSETSTATE:
	case BIOCVOLOPS:
		error = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_WRITECONF, addr);
		if (error)
			return error;
		break;
	case BIOCALARM: {
		struct bioc_alarm *alarm = (struct bioc_alarm *)addr;
		switch (alarm->ba_opcode) {
		case BIOC_SADISABLE:
		case BIOC_SAENABLE:
		case BIOC_SASILENCE:
		case BIOC_SATEST:
			error = kauth_authorize_device_passthru(l->l_cred, dev,
			    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_WRITECONF, addr);
			if (error)
				return error;
			break;
		case BIOC_GASTATUS:
			error = kauth_authorize_device_passthru(l->l_cred, dev,
			    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_READCONF, addr);
			if (error)
				return error;
			break;
		default:
			return EINVAL;
		}
		break;
	}
	default:
		return ENOTTY;
	}

	switch (cmd) {
	case BIOCLOCATE:
		locate = addr;
		error = copyinstr(locate->bl_name, name, sizeof(name), NULL);
		if (error != 0)
			return error;
		locate->bl_cookie = bio_lookup(name);
		if (locate->bl_cookie == NULL)
			return ENOENT;
		break;

	default:
		common = addr;
		mutex_enter(&bio_lock);
		if (!bio_validate(common->bc_cookie)) {
			mutex_exit(&bio_lock);
			return ENOENT;
		}
		mutex_exit(&bio_lock);
#ifdef COMPAT_30
		switch (cmd) {
		case OBIOCDISK: {
			struct bioc_disk *bd =
			    malloc(sizeof(*bd), M_DEVBUF, M_WAITOK|M_ZERO);

			(void)memcpy(bd, addr, sizeof(struct obioc_disk));
			error = bio_delegate_ioctl(common->bc_cookie,
			    BIOCDISK, bd);
			if (error) {
				free(bd, M_DEVBUF);
				return error;
			}

			(void)memcpy(addr, bd, sizeof(struct obioc_disk));
			free(bd, M_DEVBUF);
			return 0;
		}
		case OBIOCVOL: {
			struct bioc_vol *bv =
			    malloc(sizeof(*bv), M_DEVBUF, M_WAITOK|M_ZERO);

			(void)memcpy(bv, addr, sizeof(struct obioc_vol));
			error = bio_delegate_ioctl(common->bc_cookie,
			    BIOCVOL, bv);
			if (error) {
				free(bv, M_DEVBUF);
				return error;
			}

			(void)memcpy(addr, bv, sizeof(struct obioc_vol));
			free(bv, M_DEVBUF);
			return 0;
		}
		}
#endif
		error = bio_delegate_ioctl(common->bc_cookie, cmd, addr);
		return error;
	}
	return 0;
}

int
bio_register(device_t dev, int (*ioctl)(device_t, u_long, void *))
{
	struct bio_mapping *bm;

	if (!bio_lock_initialized)
		bio_initialize();

	bm = malloc(sizeof(*bm), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (bm == NULL)
		return ENOMEM;
	bm->bm_dev = dev;
	bm->bm_ioctl = ioctl;
	mutex_enter(&bio_lock);
	LIST_INSERT_HEAD(&bios, bm, bm_link);
	mutex_exit(&bio_lock);
	return 0;
}

void
bio_unregister(device_t dev)
{
	struct bio_mapping *bm, *next;

	mutex_enter(&bio_lock);
	for (bm = LIST_FIRST(&bios); bm != NULL; bm = next) {
		next = LIST_NEXT(bm, bm_link);

		if (dev == bm->bm_dev) {
			LIST_REMOVE(bm, bm_link);
			free(bm, M_DEVBUF);
		}
	}
	mutex_exit(&bio_lock);
}

static struct bio_mapping *
bio_lookup(char *name)
{
	struct bio_mapping *bm;

	mutex_enter(&bio_lock);
	LIST_FOREACH(bm, &bios, bm_link) {
		if (strcmp(name, device_xname(bm->bm_dev)) == 0) {
			mutex_exit(&bio_lock);
			return bm;
		}
	}
	mutex_exit(&bio_lock);
	return NULL;
}

static int
bio_validate(void *cookie)
{
	struct bio_mapping *bm;

	LIST_FOREACH(bm, &bios, bm_link)
		if (bm == cookie)
			return 1;

	return 0;
}

static int
bio_delegate_ioctl(void *cookie, u_long cmd, void *addr)
{
	struct bio_mapping *bm = cookie;
	
	return bm->bm_ioctl(bm->bm_dev, cmd, addr);
}

void
bio_disk_to_envsys(envsys_data_t *edata, const struct bioc_disk *bd)
{
	switch (bd->bd_status) {
	case BIOC_SDONLINE:
		edata->value_cur = ENVSYS_DRIVE_ONLINE;
		edata->state = ENVSYS_SVALID;
		break;
	case BIOC_SDOFFLINE:
		edata->value_cur = ENVSYS_DRIVE_OFFLINE;
		edata->state = ENVSYS_SCRITICAL;
		break;
	default:
		edata->value_cur = ENVSYS_DRIVE_FAIL;
		edata->state = ENVSYS_SCRITICAL;
		break;
	}
}

void
bio_vol_to_envsys(envsys_data_t *edata, const struct bioc_vol *bv)
{
	switch (bv->bv_status) {
	case BIOC_SVOFFLINE:
		edata->value_cur = ENVSYS_DRIVE_OFFLINE;
		edata->state = ENVSYS_SCRITICAL;
		break;
	case BIOC_SVDEGRADED:
		edata->value_cur = ENVSYS_DRIVE_PFAIL;
		edata->state = ENVSYS_SCRITICAL;
		break;
	case BIOC_SVBUILDING:
		edata->value_cur = ENVSYS_DRIVE_BUILD;
		edata->state = ENVSYS_SVALID;
		break;
	case BIOC_SVMIGRATING:
		edata->value_cur = ENVSYS_DRIVE_MIGRATING;
		edata->state = ENVSYS_SVALID;
		break;
	case BIOC_SVCHECKING:
		edata->value_cur = ENVSYS_DRIVE_CHECK;
		edata->state = ENVSYS_SVALID;
		break;
	case BIOC_SVREBUILD:
		edata->value_cur = ENVSYS_DRIVE_REBUILD;
		edata->state = ENVSYS_SCRITICAL;
		break;
	case BIOC_SVSCRUB:
	case BIOC_SVONLINE:
		edata->value_cur = ENVSYS_DRIVE_ONLINE;
		edata->state = ENVSYS_SVALID;
		break;
	case BIOC_SVINVALID:
		/* FALLTHROUGH */
	default:
		edata->value_cur = ENVSYS_DRIVE_EMPTY; /* unknown state */
		edata->state = ENVSYS_SINVALID;
		break;
	}
}
