/*        $NetBSD: dm_target_snapshot.c,v 1.17 2014/08/18 17:16:19 agc Exp $      */

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
 * 1. Suspend my_data to temporarily stop any I/O while the snapshot is being
 * activated.
 * dmsetup suspend my_data
 *
 * 2. Create the snapshot-origin device with no table.
 * dmsetup create my_data_org
 *
 * 3. Read the table from my_data and load it into my_data_org.
 * dmsetup table my_data | dmsetup load my_data_org
 *
 * 4. Resume this new table.
 * dmsetup resume my_data_org
 *
 * 5. Create the snapshot device with no table.
 * dmsetup create my_data_snap
 *
 * 6. Load the table into my_data_snap. This uses /dev/hdd1 as the COW device and
 * uses a 32kB chunk-size.
 * echo "0 `blockdev --getsize /dev/mapper/my_data` snapshot \
 *  /dev/mapper/my_data_org /dev/hdd1 p 64" | dmsetup load my_data_snap
 *
 * 7. Reload my_data as a snapshot-origin device that points to my_data_org.
 * echo "0 `blockdev --getsize /dev/mapper/my_data` snapshot-origin \
 *  /dev/mapper/my_data_org" | dmsetup load my_data
 *
 * 8. Resume the snapshot and origin devices.
 * dmsetup resume my_data_snap
 * dmsetup resume my_data
 *
 * Before snapshot creation
 *  dev_name; dev table
 * | my_data; 0 1024 linear /dev/sd1a 384|
 *
 * After snapshot creation
 *                               |my_data_org;0 1024 linear /dev/sd1a 384|
 *                              /
 * |my_data; 0 1024 snapshot-origin /dev/vg00/my_data_org|
 *                           /
 * |my_data_snap; 0 1024 snapshot /dev/vg00/my_data /dev/mapper/my_data_cow P 8
 *                           \
 *                            |my_data_cow; 0 256 linear /dev/sd1a 1408|
 */

/*
 * This file implements initial version of device-mapper snapshot target.
 */
#include <sys/types.h>
#include <sys/param.h>

#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/vnode.h>

#include "dm.h"

/* dm_target_snapshot.c */
int dm_target_snapshot_init(dm_dev_t *, void**, char *);
char * dm_target_snapshot_status(void *);
int dm_target_snapshot_strategy(dm_table_entry_t *, struct buf *);
int dm_target_snapshot_deps(dm_table_entry_t *, prop_array_t);
int dm_target_snapshot_destroy(dm_table_entry_t *);
int dm_target_snapshot_upcall(dm_table_entry_t *, struct buf *);

/* dm snapshot origin driver */
int dm_target_snapshot_orig_init(dm_dev_t *, void**, char *);
char * dm_target_snapshot_orig_status(void *);
int dm_target_snapshot_orig_strategy(dm_table_entry_t *, struct buf *);
int dm_target_snapshot_orig_sync(dm_table_entry_t *);
int dm_target_snapshot_orig_deps(dm_table_entry_t *, prop_array_t);
int dm_target_snapshot_orig_destroy(dm_table_entry_t *);
int dm_target_snapshot_orig_upcall(dm_table_entry_t *, struct buf *);

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

MODULE(MODULE_CLASS_MISC, dm_target_snapshot, "dm");

static int
dm_target_snapshot_modcmd(modcmd_t cmd, void *arg)
{
	dm_target_t *dmt, *dmt1;
	int r;

	dmt = NULL;
	dmt1 = NULL;

	switch (cmd) {
	case MODULE_CMD_INIT:
		if (((dmt = dm_target_lookup("snapshot")) != NULL)) {
			dm_target_unbusy(dmt);
			return EEXIST;
		}
		if (((dmt = dm_target_lookup("snapshot-origin")) != NULL)) {
			dm_target_unbusy(dmt);
			return EEXIST;
		}
		dmt = dm_target_alloc("snapshot");
		dmt1 = dm_target_alloc("snapshot-origin");

		dmt->version[0] = 1;
		dmt->version[1] = 0;
		dmt->version[2] = 5;
		strlcpy(dmt->name, "snapshot", DM_MAX_TYPE_NAME);
		dmt->init = &dm_target_snapshot_init;
		dmt->status = &dm_target_snapshot_status;
		dmt->strategy = &dm_target_snapshot_strategy;
		dmt->deps = &dm_target_snapshot_deps;
		dmt->destroy = &dm_target_snapshot_destroy;
		dmt->upcall = &dm_target_snapshot_upcall;

		r = dm_target_insert(dmt);

		dmt1->version[0] = 1;
		dmt1->version[1] = 0;
		dmt1->version[2] = 5;
		strlcpy(dmt1->name, "snapshot-origin", DM_MAX_TYPE_NAME);
		dmt1->init = &dm_target_snapshot_orig_init;
		dmt1->status = &dm_target_snapshot_orig_status;
		dmt1->strategy = &dm_target_snapshot_orig_strategy;
		dmt1->sync = &dm_target_snapshot_orig_sync;
		dmt1->deps = &dm_target_snapshot_orig_deps;
		dmt1->destroy = &dm_target_snapshot_orig_destroy;
		dmt1->upcall = &dm_target_snapshot_orig_upcall;

		r = dm_target_insert(dmt1);
		break;

	case MODULE_CMD_FINI:
		/*
		 * Try to remove snapshot target if it works remove snap-origin
		 * it is not possible to remove snapshot and do not remove
		 * snap-origin because they are used together.
		 */
		if ((r = dm_target_rem("snapshot")) == 0)
			r = dm_target_rem("snapshot-origin");

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
 * Init function called from dm_table_load_ioctl.
 * argv:  /dev/mapper/my_data_org /dev/mapper/tsc_cow_dev p 64
 *        snapshot_origin device, cow device, persistent flag, chunk size
 */
int
dm_target_snapshot_init(dm_dev_t * dmv, void **target_config, char *params)
{
	dm_target_snapshot_config_t *tsc;
	dm_pdev_t *dmp_snap, *dmp_cow;
	char **ap, *argv[5];

	dmp_cow = NULL;

	if (params == NULL)
		return EINVAL;
	/*
	 * Parse a string, containing tokens delimited by white space,
	 * into an argument vector
	 */
	for (ap = argv; ap < &argv[4] &&
	    (*ap = strsep(&params, " \t")) != NULL;) {
		if (**ap != '\0')
			ap++;
	}

	printf("Snapshot target init function called!!\n");
	printf("Snapshotted device: %s, cow device %s,\n\t persistent flag: %s, "
	    "chunk size: %s\n", argv[0], argv[1], argv[2], argv[3]);

	/* Insert snap device to global pdev list */
	if ((dmp_snap = dm_pdev_insert(argv[0])) == NULL)
		return ENOENT;

	if ((tsc = kmem_alloc(sizeof(*tsc), KM_NOSLEEP)) == NULL)
		return 1;

	tsc->tsc_persistent_dev = 0;

	/* There is now cow device for nonpersistent snapshot devices */
	if (strcmp(argv[2], "p") == 0) {
		tsc->tsc_persistent_dev = 1;

		/* Insert cow device to global pdev list */
		if ((dmp_cow = dm_pdev_insert(argv[1])) == NULL) {
			kmem_free(tsc, sizeof(*tsc));
			return ENOENT;
		}
	}
	tsc->tsc_chunk_size = atoi(argv[3]);

	tsc->tsc_snap_dev = dmp_snap;
	tsc->tsc_cow_dev = dmp_cow;

	*target_config = tsc;

	dmv->dev_type = DM_SNAPSHOT_DEV;
	dmv->sec_size = dmp_snap->dmp_secsize;

	return 0;
}
/*
 * Status routine is called to get params string, which is target
 * specific. When dm_table_status_ioctl is called with flag
 * DM_STATUS_TABLE_FLAG I have to sent params string back.
 */
char *
dm_target_snapshot_status(void *target_config)
{
	dm_target_snapshot_config_t *tsc;

	uint32_t i;
	uint32_t count;
	size_t prm_len, cow_len;
	char *params, *cow_name;

	tsc = target_config;

	prm_len = 0;
	cow_len = 0;
	count = 0;
	cow_name = NULL;

	printf("Snapshot target status function called\n");

	/* count number of chars in offset */
	for (i = tsc->tsc_chunk_size; i != 0; i /= 10)
		count++;

	if (tsc->tsc_persistent_dev)
		cow_len = strlen(tsc->tsc_cow_dev->name);

	/* length of names + count of chars + spaces and null char */
	prm_len = strlen(tsc->tsc_snap_dev->name) + cow_len + count + 5;

	if ((params = kmem_alloc(prm_len, KM_NOSLEEP)) == NULL)
		return NULL;

	printf("%s %s %s %" PRIu64 "\n", tsc->tsc_snap_dev->name,
	    tsc->tsc_cow_dev->name, tsc->tsc_persistent_dev ? "p" : "n",
	    tsc->tsc_chunk_size);

	snprintf(params, prm_len, "%s %s %s %" PRIu64, tsc->tsc_snap_dev->name,
	    tsc->tsc_persistent_dev ? tsc->tsc_cow_dev->name : "",
	    tsc->tsc_persistent_dev ? "p" : "n",
	    tsc->tsc_chunk_size);

	return params;
}
/* Strategy routine called from dm_strategy. */
int
dm_target_snapshot_strategy(dm_table_entry_t * table_en, struct buf * bp)
{

	printf("Snapshot target read function called!!\n");

	bp->b_error = EIO;
	bp->b_resid = 0;

	biodone(bp);

	return 0;
}
/* Doesn't do anything here. */
int
dm_target_snapshot_destroy(dm_table_entry_t * table_en)
{
	dm_target_snapshot_config_t *tsc;

	/*
	 * Destroy function is called for every target even if it
	 * doesn't have target_config.
	 */

	if (table_en->target_config == NULL)
		return 0;

	printf("Snapshot target destroy function called\n");

	tsc = table_en->target_config;

	/* Decrement pdev ref counter if 0 remove it */
	dm_pdev_decr(tsc->tsc_snap_dev);

	if (tsc->tsc_persistent_dev)
		dm_pdev_decr(tsc->tsc_cow_dev);

	/* Unbusy target so we can unload it */
	dm_target_unbusy(table_en->target);

	kmem_free(table_en->target_config, sizeof(dm_target_snapshot_config_t));

	table_en->target_config = NULL;

	return 0;
}
/* Add this target dependiences to prop_array_t */
int
dm_target_snapshot_deps(dm_table_entry_t * table_en,
    prop_array_t prop_array)
{
	dm_target_snapshot_config_t *tsc;

	if (table_en->target_config == NULL)
		return 0;

	tsc = table_en->target_config;

	prop_array_add_uint64(prop_array,
	    (uint64_t) tsc->tsc_snap_dev->pdev_vnode->v_rdev);

	if (tsc->tsc_persistent_dev) {
		prop_array_add_uint64(prop_array,
		    (uint64_t) tsc->tsc_cow_dev->pdev_vnode->v_rdev);

	}
	return 0;
}
/* Upcall is used to inform other depended devices about IO. */
int
dm_target_snapshot_upcall(dm_table_entry_t * table_en, struct buf * bp)
{
	printf("dm_target_snapshot_upcall called\n");

	printf("upcall buf flags %s %s\n",
	    (bp->b_flags & B_WRITE) ? "B_WRITE" : "",
	    (bp->b_flags & B_READ) ? "B_READ" : "");

	return 0;
}
/*
 * dm target snapshot origin routines.
 *
 * Keep for compatibility with linux lvm2tools. They use two targets
 * to implement snapshots. Snapshot target will implement exception
 * store and snapshot origin will implement device which calls every
 * snapshot device when write is done on master device.
 */

/*
 * Init function called from dm_table_load_ioctl.
 *
 * argv: /dev/mapper/my_data_real
 */
int
dm_target_snapshot_orig_init(dm_dev_t * dmv, void **target_config, char *params)
{
	dm_target_snapshot_origin_config_t *tsoc;
	dm_pdev_t *dmp_real;

	if (params == NULL)
		return EINVAL;

	printf("Snapshot origin target init function called!!\n");
	printf("Parent device: %s\n", params);

	/* Insert snap device to global pdev list */
	if ((dmp_real = dm_pdev_insert(params)) == NULL)
		return ENOENT;

	if ((tsoc = kmem_alloc(sizeof(dm_target_snapshot_origin_config_t), KM_NOSLEEP))
	    == NULL)
		return 1;

	tsoc->tsoc_real_dev = dmp_real;

	dmv->dev_type = DM_SNAPSHOT_ORIG_DEV;

	*target_config = tsoc;

	return 0;
}
/*
 * Status routine is called to get params string, which is target
 * specific. When dm_table_status_ioctl is called with flag
 * DM_STATUS_TABLE_FLAG I have to sent params string back.
 */
char *
dm_target_snapshot_orig_status(void *target_config)
{
	dm_target_snapshot_origin_config_t *tsoc;

	size_t prm_len;
	char *params;

	tsoc = target_config;

	prm_len = 0;

	printf("Snapshot origin target status function called\n");

	/* length of names + count of chars + spaces and null char */
	prm_len = strlen(tsoc->tsoc_real_dev->name) + 1;

	printf("real_dev name %s\n", tsoc->tsoc_real_dev->name);

	if ((params = kmem_alloc(prm_len, KM_NOSLEEP)) == NULL)
		return NULL;

	printf("%s\n", tsoc->tsoc_real_dev->name);

	snprintf(params, prm_len, "%s", tsoc->tsoc_real_dev->name);

	return params;
}
/* Strategy routine called from dm_strategy. */
int
dm_target_snapshot_orig_strategy(dm_table_entry_t * table_en, struct buf * bp)
{

	printf("Snapshot_Orig target read function called!!\n");

	bp->b_error = EIO;
	bp->b_resid = 0;

	biodone(bp);

	return 0;
}
/*
 * Sync underlying disk caches.
 */
int
dm_target_snapshot_orig_sync(dm_table_entry_t * table_en)
{
	int cmd;
	dm_target_snapshot_origin_config_t *tsoc;

	tsoc = table_en->target_config;

	cmd = 1;

	return VOP_IOCTL(tsoc->tsoc_real_dev->pdev_vnode,  DIOCCACHESYNC, &cmd, FREAD|FWRITE, kauth_cred_get());
}
/* Decrement pdev and free allocated space. */
int
dm_target_snapshot_orig_destroy(dm_table_entry_t * table_en)
{
	dm_target_snapshot_origin_config_t *tsoc;

	/*
	 * Destroy function is called for every target even if it
	 * doesn't have target_config.
	 */

	if (table_en->target_config == NULL)
		return 0;

	tsoc = table_en->target_config;

	/* Decrement pdev ref counter if 0 remove it */
	dm_pdev_decr(tsoc->tsoc_real_dev);

	/* Unbusy target so we can unload it */
	dm_target_unbusy(table_en->target);

	kmem_free(table_en->target_config, sizeof(dm_target_snapshot_origin_config_t));

	table_en->target_config = NULL;

	return 0;
}
/*
 * Get target deps and add them to prop_array_t.
 */
int
dm_target_snapshot_orig_deps(dm_table_entry_t * table_en,
    prop_array_t prop_array)
{
	dm_target_snapshot_origin_config_t *tsoc;
	struct vattr va;

	int error;

	if (table_en->target_config == NULL)
		return 0;

	tsoc = table_en->target_config;

	vn_lock(tsoc->tsoc_real_dev->pdev_vnode, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(tsoc->tsoc_real_dev->pdev_vnode, &va,
	    curlwp->l_cred);
	VOP_UNLOCK(tsoc->tsoc_real_dev->pdev_vnode);
	if (error != 0)
		return error;

	prop_array_add_uint64(prop_array, (uint64_t) va.va_rdev);

	return 0;
}
/* Unsupported for this target. */
int
dm_target_snapshot_orig_upcall(dm_table_entry_t * table_en, struct buf * bp)
{
	return 0;
}
