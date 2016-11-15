/*	$NetBSD: rf_compat50.c,v 1.2 2009/05/02 21:11:26 oster Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <dev/raidframe/raidframeio.h>
#include <dev/raidframe/raidframevar.h>

#include "rf_raid.h"
#include "rf_compat50.h"
#include "rf_debugMem.h"

typedef struct RF_Config50_s {
	RF_RowCol_t		numRow, numCol, numSpare;
	int32_t			devs[RF_MAXROW][RF_MAXCOL];
	char			devnames[RF_MAXROW][RF_MAXCOL][50];
	int32_t			spare_devs[RF_MAXSPARE];
	char			spare_names[RF_MAXSPARE][50];
	RF_SectorNum_t		sectPerSU;
	RF_StripeNum_t		SUsPerPU;
	RF_StripeNum_t		SUsPerRU;
	RF_ParityConfig_t	parityConfig;
	RF_DiskQueueType_t	diskQueueType;
	char			maxOutstandingDiskReqs;
	char			debugVars[RF_MAXDBGV][50];
	unsigned int		layoutSpecificSize;
	void		       *layoutSpecific;
	int			force;
} RF_Config50_t;

typedef struct RF_RaidDisk50_s {
        char		 devname[56];
        RF_DiskStatus_t	 status;
        RF_RowCol_t	 spareRow;
        RF_RowCol_t	 spareCol;
        RF_SectorCount_t numBlocks;
        int     	 blockSize;
        RF_SectorCount_t partitionSize;
        int    		 auto_configured;
        int32_t		 dev;
} RF_RaidDisk50_t;

typedef struct RF_DeviceConfig50_s {
	u_int			rows;
	u_int			cols;
	u_int			maxqdepth;
	int			ndevs;
	RF_RaidDisk50_t		devs[RF_MAX_DISKS];
	int			nspares;
	RF_RaidDisk50_t		spares[RF_MAX_DISKS];
} RF_DeviceConfig50_t;

static void
rf_disk_to_disk50(RF_RaidDisk50_t *d50, const RF_RaidDisk_t *d)
{
        memcpy(d50->devname, d->devname, sizeof(d50->devname));
        d50->status = d->status;
        d50->spareRow = d->spareRow;
        d50->spareCol = d->spareCol;
        d50->numBlocks = d->numBlocks;
        d50->blockSize = d->blockSize;
        d50->partitionSize = d->partitionSize;
        d50->auto_configured = d->auto_configured;
        d50->dev = d->dev;
}

int
rf_config50(RF_Raid_t *raidPtr, int unit, void *data, RF_Config_t **k_cfgp)
{
	RF_Config50_t *u50_cfg, *k50_cfg;
	RF_Config_t *k_cfg;
	size_t i, j;
	int error;

	if (raidPtr->valid) {
		/* There is a valid RAID set running on this unit! */
		printf("raid%d: Device already configured!\n", unit);
		return EINVAL;
	}

	/* copy-in the configuration information */
	/* data points to a pointer to the configuration structure */

	u50_cfg = *((RF_Config50_t **) data);
	RF_Malloc(k50_cfg, sizeof(RF_Config50_t), (RF_Config50_t *));
	if (k50_cfg == NULL)
		return ENOMEM;

	error = copyin(u50_cfg, k50_cfg, sizeof(RF_Config50_t));
	if (error) {
		RF_Free(k50_cfg, sizeof(RF_Config50_t));
		return error;
	}
	RF_Malloc(k_cfg, sizeof(RF_Config_t), (RF_Config_t *));
	if (k_cfg == NULL) {
		RF_Free(k50_cfg, sizeof(RF_Config50_t));
		return ENOMEM;
	}

	k_cfg->numRow = k50_cfg->numRow;
	k_cfg->numCol = k50_cfg->numCol;
	k_cfg->numSpare = k50_cfg->numSpare;

	for (i = 0; i < RF_MAXROW; i++)
		for (j = 0; j < RF_MAXCOL; j++)
			k_cfg->devs[i][j] = k50_cfg->devs[i][j];

	memcpy(k_cfg->devnames, k50_cfg->devnames,
	    sizeof(k_cfg->devnames));

	for (i = 0; i < RF_MAXSPARE; i++)
		k_cfg->spare_devs[i] = k50_cfg->spare_devs[i];

	memcpy(k_cfg->spare_names, k50_cfg->spare_names,
	    sizeof(k_cfg->spare_names));

	k_cfg->sectPerSU = k50_cfg->sectPerSU;
	k_cfg->SUsPerPU = k50_cfg->SUsPerPU;
	k_cfg->SUsPerRU = k50_cfg->SUsPerRU;
	k_cfg->parityConfig = k50_cfg->parityConfig;

	memcpy(k_cfg->diskQueueType, k50_cfg->diskQueueType,
	    sizeof(k_cfg->diskQueueType));

	k_cfg->maxOutstandingDiskReqs = k50_cfg->maxOutstandingDiskReqs;

	memcpy(k_cfg->debugVars, k50_cfg->debugVars,
	    sizeof(k_cfg->debugVars));

	k_cfg->layoutSpecificSize = k50_cfg->layoutSpecificSize;
	k_cfg->layoutSpecific = k50_cfg->layoutSpecific;
	k_cfg->force = k50_cfg->force;

	RF_Free(k50_cfg, sizeof(RF_Config50_t));
	*k_cfgp = k_cfg;
	return 0;
}

int
rf_get_info50(RF_Raid_t *raidPtr, void *data)
{
	RF_DeviceConfig50_t **ucfgp = data, *d_cfg;
	size_t i, j;
	int error;

	if (!raidPtr->valid)
		return ENODEV;

	RF_Malloc(d_cfg, sizeof(RF_DeviceConfig50_t), (RF_DeviceConfig50_t *));

	if (d_cfg == NULL)
		return ENOMEM;

	d_cfg->rows = 1; /* there is only 1 row now */
	d_cfg->cols = raidPtr->numCol;
	d_cfg->ndevs = raidPtr->numCol;
	if (d_cfg->ndevs >= RF_MAX_DISKS)
		goto nomem;

	d_cfg->nspares = raidPtr->numSpare;
	if (d_cfg->nspares >= RF_MAX_DISKS)
		goto nomem;

	d_cfg->maxqdepth = raidPtr->maxQueueDepth;
	for (j = 0; j < d_cfg->cols; j++)
		rf_disk_to_disk50(&d_cfg->devs[j], &raidPtr->Disks[j]);

	for (j = d_cfg->cols, i = 0; i < d_cfg->nspares; i++, j++)
		rf_disk_to_disk50(&d_cfg->spares[i], &raidPtr->Disks[j]);

	error = copyout(d_cfg, *ucfgp, sizeof(RF_DeviceConfig50_t));
	RF_Free(d_cfg, sizeof(RF_DeviceConfig50_t));

	return error;
nomem:
	RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));
	return ENOMEM;
}
