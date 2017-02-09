/*	$NetBSD: ata_raid.c,v 1.36 2015/08/20 14:40:17 christos Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Support for autoconfiguration of RAID sets on ATA RAID controllers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_raid.c,v 1.36 2015/08/20 14:40:17 christos Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/proc.h>

#include <miscfs/specfs/specdev.h>

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>

#include <dev/ata/ata_raidreg.h>
#include <dev/ata/ata_raidvar.h>

#include "locators.h"
#include "ioconf.h"

#ifdef ATA_RAID_DEBUG
#define	DPRINTF(x)	printf x
#else
#define	DPRINTF(x)	/* nothing */
#endif

static int	ataraid_match(device_t, cfdata_t, void *);
static void	ataraid_attach(device_t, device_t, void *);
static int	ataraid_print(void *, const char *);

static int	ata_raid_finalize(device_t);

ataraid_array_info_list_t ataraid_array_info_list =
    TAILQ_HEAD_INITIALIZER(ataraid_array_info_list);
u_int ataraid_array_info_count;

CFATTACH_DECL_NEW(ataraid, 0,
    ataraid_match, ataraid_attach, NULL, NULL);

/*
 * ataraidattach:
 *
 *	Pseudo-device attach routine.
 */
void
ataraidattach(int count)
{

	/*
	 * Register a finalizer which will be used to actually configure
	 * the logical disks configured by ataraid.
	 */
	if (config_finalize_register(NULL, ata_raid_finalize) != 0)
		printf("WARNING: unable to register ATA RAID finalizer\n");
}

/*
 * ata_raid_type_name:
 *
 *	Return the type of ATA RAID.
 */
const char *
ata_raid_type_name(u_int type)
{
	static const char *ata_raid_type_names[] = {
		"Promise",
		"Adaptec",
		"VIA V-RAID",
		"nVidia",
		"JMicron",
		"Intel MatrixRAID"
	};

	if (type < __arraycount(ata_raid_type_names))
		return (ata_raid_type_names[type]);

	return (NULL);
}

/*
 * ata_raid_finalize:
 *
 *	Autoconfiguration finalizer for ATA RAID.
 */
static int
ata_raid_finalize(device_t self)
{
	static struct cfdata ataraid_cfdata = {
		.cf_name = "ataraid",
		.cf_atname = "ataraid",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
	};
	extern struct cfdriver ataraid_cd;
	static int done_once;
	int error;

	/*
	 * Since we only handle real hardware, we only need to be
	 * called once.
	 */
	if (done_once)
		return (0);
	done_once = 1;

	if (TAILQ_EMPTY(&ataraid_array_info_list))
		goto out;

	error = config_cfattach_attach(ataraid_cd.cd_name, &ataraid_ca);
	if (error) {
		printf("%s: unable to register cfattach, error = %d\n",
		    ataraid_cd.cd_name, error);
		(void) config_cfdriver_detach(&ataraid_cd);
		goto out;
	}

	if (config_attach_pseudo(&ataraid_cfdata) == NULL)
		printf("%s: unable to attach an instance\n",
		    ataraid_cd.cd_name);

 out:
	return (1);
}

/*
 * ataraid_match:
 *
 *	Autoconfiguration glue: match routine.
 */
static int
ataraid_match(device_t parent, cfdata_t cf, void *aux)
{

	/* pseudo-device; always present */
	return (1);
}

/*
 * ataraid_attach:
 *
 *	Autoconfiguration glue: attach routine.  We attach the children.
 */
static void
ataraid_attach(device_t parent, device_t self, void *aux)
{
	struct ataraid_array_info *aai;
	int locs[ATARAIDCF_NLOCS];

	/*
	 * We're a pseudo-device, so we get to announce our own
	 * presence.
	 */
	aprint_normal_dev(self, "found %u RAID volume%s\n",
	    ataraid_array_info_count,
	    ataraid_array_info_count == 1 ? "" : "s");

	TAILQ_FOREACH(aai, &ataraid_array_info_list, aai_list) {
		locs[ATARAIDCF_VENDTYPE] = aai->aai_type;
		locs[ATARAIDCF_UNIT] = aai->aai_arrayno;

		config_found_sm_loc(self, "ataraid", locs, aai,
				    ataraid_print, config_stdsubmatch);
	}
}

/*
 * ataraid_print:
 *
 *	Autoconfiguration glue: print routine.
 */
static int
ataraid_print(void *aux, const char *pnp)
{
	struct ataraid_array_info *aai = aux;

	if (pnp != NULL)
		aprint_normal("block device at %s", pnp);
	aprint_normal(" vendtype %d unit %d", aai->aai_type, aai->aai_arrayno);
	return (UNCONF);
}

/*
 * ata_raid_check_component:
 *
 *	Check the component for a RAID configuration structure.
 *	Called via autoconfiguration callback.
 */
void
ata_raid_check_component(device_t self)
{
	struct wd_softc *sc = device_private(self);

	if (ata_raid_read_config_adaptec(sc) == 0)
		return;
	if (ata_raid_read_config_promise(sc) == 0)
		return;
	if (ata_raid_read_config_via(sc) == 0)
		return;
	if (ata_raid_read_config_nvidia(sc) == 0)
		return;
	if (ata_raid_read_config_jmicron(sc) == 0)
		return;
	if (ata_raid_read_config_intel(sc) == 0)
		return;
}

struct ataraid_array_info *
ata_raid_get_array_info(u_int type, u_int arrayno)
{
	struct ataraid_array_info *aai, *laai;

	TAILQ_FOREACH(aai, &ataraid_array_info_list, aai_list) {
		if (aai->aai_type == type &&
		    aai->aai_arrayno == arrayno)
			goto out;
	}

	/* Need to allocate a new one. */
	aai = malloc(sizeof(*aai), M_DEVBUF, M_WAITOK | M_ZERO);
	aai->aai_type = type;
	aai->aai_arrayno = arrayno;
	aai->aai_curdisk = 0;

	ataraid_array_info_count++;

	/* Sort it into the list: type first, then array number. */
	TAILQ_FOREACH(laai, &ataraid_array_info_list, aai_list) {
		if (aai->aai_type < laai->aai_type) {
			TAILQ_INSERT_BEFORE(laai, aai, aai_list);
			goto out;
		}
		if (aai->aai_type == laai->aai_type &&
		    aai->aai_arrayno < laai->aai_arrayno) {
			TAILQ_INSERT_BEFORE(laai, aai, aai_list);
			goto out;
		}
	}
	TAILQ_INSERT_TAIL(&ataraid_array_info_list, aai, aai_list);

 out:
	return (aai);
}

int
ata_raid_config_block_rw(struct vnode *vp, daddr_t blkno, void *tbuf,
    size_t size, int bflags)
{
	struct buf *bp;
	int error;

	bp = getiobuf(vp, false);
	bp->b_blkno = blkno;
	bp->b_bcount = bp->b_resid = size;
	bp->b_flags = bflags;
	bp->b_proc = curproc;
	bp->b_data = tbuf;
	SET(bp->b_cflags, BC_BUSY);	/* mark buffer busy */

	VOP_STRATEGY(vp, bp);
	error = biowait(bp);

	putiobuf(bp);
	return (error);
}
