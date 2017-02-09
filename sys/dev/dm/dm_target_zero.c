/*        $NetBSD: dm_target_zero.c,v 1.12 2011/12/11 22:53:26 agc Exp $      */

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
 * This file implements initial version of device-mapper zero target.
 */
#include <sys/types.h>
#include <sys/param.h>

#include <sys/buf.h>

#include "dm.h"

/* dm_target_zero.c */
int dm_target_zero_init(dm_dev_t *, void**,  char *);
char * dm_target_zero_status(void *);
int dm_target_zero_strategy(dm_table_entry_t *, struct buf *);
int dm_target_zero_sync(dm_table_entry_t *);
int dm_target_zero_destroy(dm_table_entry_t *);
int dm_target_zero_deps(dm_table_entry_t *, prop_array_t);
int dm_target_zero_upcall(dm_table_entry_t *, struct buf *);

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

MODULE(MODULE_CLASS_MISC, dm_target_zero, "dm");

static int
dm_target_zero_modcmd(modcmd_t cmd, void *arg)
{
	dm_target_t *dmt;
	int r;
	dmt = NULL;

	switch (cmd) {
	case MODULE_CMD_INIT:
		if ((dmt = dm_target_lookup("zero")) != NULL) {
			dm_target_unbusy(dmt);
			return EEXIST;
		}
		dmt = dm_target_alloc("zero");

		dmt->version[0] = 1;
		dmt->version[1] = 0;
		dmt->version[2] = 0;
		strlcpy(dmt->name, "zero", DM_MAX_TYPE_NAME);
		dmt->init = &dm_target_zero_init;
		dmt->status = &dm_target_zero_status;
		dmt->strategy = &dm_target_zero_strategy;
		dmt->sync = &dm_target_zero_sync;		 
		dmt->deps = &dm_target_zero_deps;
		dmt->destroy = &dm_target_zero_destroy;
		dmt->upcall = &dm_target_zero_upcall;

		r = dm_target_insert(dmt);
		break;

	case MODULE_CMD_FINI:
		r = dm_target_rem("zero");

		break;

	case MODULE_CMD_STAT:
		return ENOTTY;

	default:
		return ENOTTY;
	}

	return r;
}
#endif

/*
 * Zero target init function. This target doesn't need
 * target specific config area.
 */
int
dm_target_zero_init(dm_dev_t * dmv, void **target_config, char *argv)
{

	printf("Zero target init function called!!\n");

	dmv->dev_type = DM_ZERO_DEV;

	*target_config = NULL;

	return 0;
}
/* Status routine called to get params string. */
char *
dm_target_zero_status(void *target_config)
{
	return NULL;
}


/*
 * This routine does IO operations.
 */
int
dm_target_zero_strategy(dm_table_entry_t * table_en, struct buf * bp)
{

	/* printf("Zero target read function called %d!!\n", bp->b_bcount); */

	memset(bp->b_data, 0, bp->b_bcount);
	bp->b_resid = 0;	/* nestiobuf_done wants b_resid = 0 to be sure
				 * that there is no other io to done  */

	biodone(bp);

	return 0;
}
/* Sync underlying disk caches. */
int
dm_target_zero_sync(dm_table_entry_t * table_en)
{

	return 0;
}
/* Does not need to do anything here. */
int
dm_target_zero_destroy(dm_table_entry_t * table_en)
{
	table_en->target_config = NULL;

	/* Unbusy target so we can unload it */
	dm_target_unbusy(table_en->target);

	return 0;
}
/* Does not need to do anything here. */
int
dm_target_zero_deps(dm_table_entry_t * table_en, prop_array_t prop_array)
{
	return 0;
}
/* Unsuported for this target. */
int
dm_target_zero_upcall(dm_table_entry_t * table_en, struct buf * bp)
{
	return 0;
}
