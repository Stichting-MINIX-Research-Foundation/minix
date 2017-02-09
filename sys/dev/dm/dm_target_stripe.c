/*$NetBSD: dm_target_stripe.c,v 1.21 2014/08/19 14:43:28 christos Exp $*/

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Hamsik.
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
 * This file implements initial version of device-mapper stripe target.
 */
#include <sys/types.h>
#include <sys/param.h>

#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/lwp.h>

#include "dm.h"

#ifdef DM_TARGET_MODULE
/*
 * Every target can be compiled directly to dm driver or as a
 * separate module this part of target is used for loading targets
 * to dm driver.
 * Target can be unloaded from kernel only if there are no users of
 * it e.g. there are no devices which uses that target.
 */
#include <sys/kernel.h>
#include <sys/module.h>

MODULE(MODULE_CLASS_MISC, dm_target_stripe, NULL);

static int
dm_target_stripe_modcmd(modcmd_t cmd, void *arg)
{
	dm_target_t *dmt;
	int r;
	dmt = NULL;

	switch (cmd) {
	case MODULE_CMD_INIT:
		if ((dmt = dm_target_lookup("stripe")) != NULL) {
			dm_target_unbusy(dmt);
			return EEXIST;
		}
		dmt = dm_target_alloc("stripe");

		dmt->version[0] = 1;
		dmt->version[1] = 0;
		dmt->version[2] = 0;
		strlcpy(dmt->name, "stripe", DM_MAX_TYPE_NAME);
		dmt->init = &dm_target_stripe_init;
		dmt->status = &dm_target_stripe_status;
		dmt->strategy = &dm_target_stripe_strategy;
		dmt->sync = &dm_target_stripe_sync;
		dmt->deps = &dm_target_stripe_deps;
		dmt->destroy = &dm_target_stripe_destroy;
		dmt->upcall = &dm_target_stripe_upcall;
		dmt->secsize = &dm_target_stripe_secsize;

		r = dm_target_insert(dmt);

		break;

	case MODULE_CMD_FINI:
		r = dm_target_rem("stripe");
		break;

	case MODULE_CMD_STAT:
		return ENOTTY;

	default:
		return ENOTTY;
	}

	return r;
}
#endif

static void
dm_target_stripe_fini(dm_target_stripe_config_t *tsc)
{
	dm_target_linear_config_t *tlc;

	if (tsc == NULL)
		return;

	while ((tlc = TAILQ_FIRST(&tsc->stripe_devs)) != NULL) {
		TAILQ_REMOVE(&tsc->stripe_devs, tlc, entries);
		dm_pdev_decr(tlc->pdev);
		kmem_free(tlc, sizeof(*tlc));
	}

	kmem_free(tsc, sizeof(*tsc));
}

/*
 * Init function called from dm_table_load_ioctl.
 * DM_STRIPE_DEV_OFFSET should always hold the index of the first device-offset
 * pair in the parameters.
 * Example line sent to dm from lvm tools when using striped target.
 * start length striped #stripes chunk_size device1 offset1 ... deviceN offsetN
 * 0 65536 striped 2 512 /dev/hda 0 /dev/hdb 0
 */
int
dm_target_stripe_init(dm_dev_t * dmv, void **target_config, char *params)
{
	dm_target_linear_config_t *tlc;
	dm_target_stripe_config_t *tsc;
	size_t len;
	char **ap, *argv[10];
	int strpc, strpi;

	if (params == NULL)
		return EINVAL;

	len = strlen(params) + 1;

	/*
	 * Parse a string, containing tokens delimited by white space,
	 * into an argument vector
	 */
	for (ap = argv; ap <= &argv[9] &&
	    (*ap = strsep(&params, " \t")) != NULL;) {
		if (**ap != '\0')
			ap++;
	}

	printf("Stripe target init function called!!\n");

	printf("Stripe target chunk size %s number of stripes %s\n",
	    argv[1], argv[0]);

	if ((tsc = kmem_alloc(sizeof(*tsc), KM_NOSLEEP)) == NULL)
		return ENOMEM;

	/* Initialize linked list for striping devices */
	TAILQ_INIT(&tsc->stripe_devs);

	/* Save length of param string */
	tsc->params_len = len;
	tsc->stripe_chunksize = atoi(argv[1]);
	tsc->stripe_num = (uint8_t) atoi(argv[0]);

	strpc = DM_STRIPE_DEV_OFFSET + (tsc->stripe_num * 2);
	for (strpi = DM_STRIPE_DEV_OFFSET; strpi < strpc; strpi += 2) {
		printf("Stripe target device name %s -- offset %s\n",
		       argv[strpi], argv[strpi+1]);

		tlc = kmem_alloc(sizeof(*tlc), KM_NOSLEEP);
		if ((tlc->pdev = dm_pdev_insert(argv[strpi])) == NULL) {
			kmem_free(tlc, sizeof(*tlc));
			dm_target_stripe_fini(tsc);
			return ENOENT;
		}
		tlc->offset = atoi(argv[strpi+1]);

		/* Insert striping device to linked list. */
		TAILQ_INSERT_TAIL(&tsc->stripe_devs, tlc, entries);
	}

	*target_config = tsc;

	dmv->dev_type = DM_STRIPE_DEV;

	return 0;
}
/* Status routine called to get params string. */
char *
dm_target_stripe_status(void *target_config)
{
	dm_target_linear_config_t *tlc;
	dm_target_stripe_config_t *tsc;
	char *params, *tmp;

	tsc = target_config;

	if ((params = kmem_alloc(DM_MAX_PARAMS_SIZE, KM_SLEEP)) == NULL)
		return NULL;

	if ((tmp = kmem_alloc(DM_MAX_PARAMS_SIZE, KM_SLEEP)) == NULL) {
		kmem_free(params, DM_MAX_PARAMS_SIZE);
		return NULL;
	}

	snprintf(params, DM_MAX_PARAMS_SIZE, "%d %" PRIu64,
	    tsc->stripe_num, tsc->stripe_chunksize);

	TAILQ_FOREACH(tlc, &tsc->stripe_devs, entries) {
		snprintf(tmp, DM_MAX_PARAMS_SIZE, " %s %" PRIu64,
		    tlc->pdev->name, tlc->offset);
		strcat(params, tmp);
	}

	kmem_free(tmp, DM_MAX_PARAMS_SIZE);

	return params;
}
/* Strategy routine called from dm_strategy. */
int
dm_target_stripe_strategy(dm_table_entry_t * table_en, struct buf * bp)
{
	dm_target_linear_config_t *tlc;
	dm_target_stripe_config_t *tsc;
	struct buf *nestbuf;
	uint64_t blkno, blkoff;
	uint64_t stripe, stripe_blknr;
	uint32_t stripe_off, stripe_rest, num_blks, issue_blks;
	int i, stripe_devnr;

	tsc = table_en->target_config;
	if (tsc == NULL)
		return 0;

/*	printf("Stripe target read function called %" PRIu64 "!!\n",
	tlc->offset);*/

	/* calculate extent of request */
	KASSERT(bp->b_resid % DEV_BSIZE == 0);

	blkno = bp->b_blkno;
	blkoff = 0;
	num_blks = bp->b_resid / DEV_BSIZE;
	for (;;) {
		/* blockno to strip piece nr */
		stripe = blkno / tsc->stripe_chunksize;
		stripe_off = blkno % tsc->stripe_chunksize;

		/* where we are inside the strip */
		stripe_devnr = stripe % tsc->stripe_num;
		stripe_blknr = stripe / tsc->stripe_num;

		/* how much is left before we hit a boundary */
		stripe_rest = tsc->stripe_chunksize - stripe_off;

		/* issue this piece on stripe `stripe' */
		issue_blks = MIN(stripe_rest, num_blks);
		nestbuf = getiobuf(NULL, true);

		nestiobuf_setup(bp, nestbuf, blkoff, issue_blks * DEV_BSIZE);
		nestbuf->b_blkno = stripe_blknr * tsc->stripe_chunksize + stripe_off;

		tlc = TAILQ_FIRST(&tsc->stripe_devs);
		for (i = 0; i < stripe_devnr && tlc != NULL; i++)
			tlc = TAILQ_NEXT(tlc, entries);

		/* by this point we should have an tlc */
		KASSERT(tlc != NULL);

		nestbuf->b_blkno += tlc->offset;

		VOP_STRATEGY(tlc->pdev->pdev_vnode, nestbuf);

		blkno += issue_blks;
		blkoff += issue_blks * DEV_BSIZE;
		num_blks -= issue_blks;

		if (num_blks <= 0)
			break;
	}

	return 0;
}
/* Sync underlying disk caches. */
int
dm_target_stripe_sync(dm_table_entry_t * table_en)
{
	int cmd, err;
	dm_target_stripe_config_t *tsc;
	dm_target_linear_config_t *tlc;

	tsc = table_en->target_config;

	err = 0;
	cmd = 1;

	TAILQ_FOREACH(tlc, &tsc->stripe_devs, entries) {
		if ((err = VOP_IOCTL(tlc->pdev->pdev_vnode, DIOCCACHESYNC,
			    &cmd, FREAD|FWRITE, kauth_cred_get())) != 0)
			return err;
	}

	return err;

}
/* Destroy target specific data. */
int
dm_target_stripe_destroy(dm_table_entry_t * table_en)
{
	dm_target_stripe_fini(table_en->target_config);

	/* Unbusy target so we can unload it */
	dm_target_unbusy(table_en->target);

	table_en->target_config = NULL;
	return 0;
}
/* Doesn't not need to do anything here. */
int
dm_target_stripe_deps(dm_table_entry_t * table_en, prop_array_t prop_array)
{
	dm_target_stripe_config_t *tsc;
	dm_target_linear_config_t *tlc;

	if (table_en->target_config == NULL)
		return ENOENT;

	tsc = table_en->target_config;

	TAILQ_FOREACH(tlc, &tsc->stripe_devs, entries) {
		prop_array_add_uint64(prop_array,
		    (uint64_t) tlc->pdev->pdev_vnode->v_rdev);
	}

	return 0;
}
/* Unsupported for this target. */
int
dm_target_stripe_upcall(dm_table_entry_t * table_en, struct buf * bp)
{
	return 0;
}
/*
 * Compute physical block size
 * For a stripe target we chose the maximum sector size of all
 * stripe devices. For the supported power-of-2 sizes this is equivalent
 * to the least common multiple.
 */
int
dm_target_stripe_secsize(dm_table_entry_t * table_en, unsigned *secsizep)
{
	dm_target_linear_config_t *tlc;
	dm_target_stripe_config_t *tsc;
	unsigned secsize;

	secsize = 0;

	tsc = table_en->target_config;
	if (tsc != NULL) {
		TAILQ_FOREACH(tlc, &tsc->stripe_devs, entries) {
			if (secsize < tlc->pdev->pdev_secsize)
				secsize = tlc->pdev->pdev_secsize;
		}
	}

	*secsizep = secsize;

	return 0;
}
