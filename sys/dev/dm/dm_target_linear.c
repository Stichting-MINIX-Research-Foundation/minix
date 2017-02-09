/*        $NetBSD: dm_target_linear.c,v 1.14 2014/06/14 07:39:00 hannken Exp $      */

/*
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 * This file implements initial version of device-mapper dklinear target.
 */

#include <sys/types.h>
#include <sys/param.h>

#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/lwp.h>

#include <machine/int_fmtio.h>

#include "dm.h"

/*
 * Allocate target specific config data, and link them to table.
 * This function is called only when, flags is not READONLY and
 * therefore we can add things to pdev list. This should not a
 * problem because this routine is called only from dm_table_load_ioctl.
 * @argv[0] is name,
 * @argv[1] is physical data offset.
 */
int
dm_target_linear_init(dm_dev_t * dmv, void **target_config, char *params)
{
	dm_target_linear_config_t *tlc;
	dm_pdev_t *dmp;

	char **ap, *argv[3];

	if (params == NULL)
		return EINVAL;

	/*
	 * Parse a string, containing tokens delimited by white space,
	 * into an argument vector
	 */
	for (ap = argv; ap < &argv[2] &&
	    (*ap = strsep(&params, " \t")) != NULL;) {
		if (**ap != '\0')
			ap++;
	}

	aprint_debug("Linear target init function called %s--%s!!\n",
	    argv[0], argv[1]);

	/* Insert dmp to global pdev list */
	if ((dmp = dm_pdev_insert(argv[0])) == NULL)
		return ENOENT;

	if ((tlc = kmem_alloc(sizeof(dm_target_linear_config_t), KM_SLEEP))
	    == NULL)
		return ENOMEM;

	tlc->pdev = dmp;
	tlc->offset = 0;	/* default settings */

	/* Check user input if it is not leave offset as 0. */
	tlc->offset = atoi(argv[1]);

	*target_config = tlc;

	dmv->dev_type = DM_LINEAR_DEV;

	return 0;
}
/*
 * Status routine is called to get params string, which is target
 * specific. When dm_table_status_ioctl is called with flag
 * DM_STATUS_TABLE_FLAG I have to sent params string back.
 */
char *
dm_target_linear_status(void *target_config)
{
	dm_target_linear_config_t *tlc;
	char *params;
	tlc = target_config;

	aprint_debug("Linear target status function called\n");

	if ((params = kmem_alloc(DM_MAX_PARAMS_SIZE, KM_NOSLEEP)) == NULL)
		return NULL;

	aprint_normal("%s %" PRIu64, tlc->pdev->name, tlc->offset);
	snprintf(params, DM_MAX_PARAMS_SIZE, "%s %" PRIu64,
	    tlc->pdev->name, tlc->offset);

	return params;
}
/*
 * Do IO operation, called from dmstrategy routine.
 */
int
dm_target_linear_strategy(dm_table_entry_t * table_en, struct buf * bp)
{
	dm_target_linear_config_t *tlc;

	tlc = table_en->target_config;

/*	printf("Linear target read function called %" PRIu64 "!!\n",
	tlc->offset);*/

	bp->b_blkno += tlc->offset;

	VOP_STRATEGY(tlc->pdev->pdev_vnode, bp);

	return 0;

}
/*
 * Sync underlying disk caches.
 */
int
dm_target_linear_sync(dm_table_entry_t * table_en)
{
	int cmd;
	dm_target_linear_config_t *tlc;

	tlc = table_en->target_config;

	cmd = 1;
	
	return VOP_IOCTL(tlc->pdev->pdev_vnode,  DIOCCACHESYNC, &cmd,
	    FREAD|FWRITE, kauth_cred_get());	
}
/*
 * Destroy target specific data. Decrement table pdevs.
 */
int
dm_target_linear_destroy(dm_table_entry_t * table_en)
{
	dm_target_linear_config_t *tlc;

	/*
	 * Destroy function is called for every target even if it
	 * doesn't have target_config.
	 */

	if (table_en->target_config == NULL)
		return 0;

	tlc = table_en->target_config;

	/* Decrement pdev ref counter if 0 remove it */
	dm_pdev_decr(tlc->pdev);

	/* Unbusy target so we can unload it */
	dm_target_unbusy(table_en->target);

	kmem_free(table_en->target_config, sizeof(dm_target_linear_config_t));

	table_en->target_config = NULL;

	return 0;
}
/* Add this target pdev dependiences to prop_array_t */
int
dm_target_linear_deps(dm_table_entry_t * table_en, prop_array_t prop_array)
{
	dm_target_linear_config_t *tlc;

	if (table_en->target_config == NULL)
		return ENOENT;

	tlc = table_en->target_config;

	prop_array_add_uint64(prop_array,
	    (uint64_t) tlc->pdev->pdev_vnode->v_rdev);

	return 0;
}
/*
 * Register upcall device.
 * Linear target doesn't need any upcall devices but other targets like
 * mirror, snapshot, multipath, stripe will use this functionality.
 */
int
dm_target_linear_upcall(dm_table_entry_t * table_en, struct buf * bp)
{
	return 0;
}
/*
 * Query physical block size of this target
 * For a linear target this is just the sector size of the underlying device
 */
int
dm_target_linear_secsize(dm_table_entry_t * table_en, unsigned *secsizep)
{
	dm_target_linear_config_t *tlc;
	unsigned secsize;

	secsize = 0;

	tlc = table_en->target_config;
	if (tlc != NULL)
		secsize = tlc->pdev->pdev_secsize;

	*secsizep = secsize;

	return 0;
}
/*
 * Transform char s to uint64_t offset number.
 */
uint64_t
atoi(const char *s)
{
	uint64_t n;
	n = 0;

	while (*s != '\0') {
		if (!isdigit(*s))
			break;

		n = (10 * n) + (*s - '0');
		s++;
	}

	return n;
}
