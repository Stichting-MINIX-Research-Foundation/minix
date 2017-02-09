/*        $NetBSD: dm_table.c,v 1.7 2011/08/27 17:10:05 ahoka Exp $      */

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

#include <sys/types.h>
#include <sys/param.h>

#include <sys/kmem.h>

#include "dm.h"

/*
 * There are two types of users of this interface:
 *
 * a) Readers such as
 *    dmstrategy, dmgetdisklabel, dmsize, dm_dev_status_ioctl,
 *    dm_table_deps_ioctl, dm_table_status_ioctl, dm_table_reload_ioctl
 *
 * b) Writers such as
 *    dm_dev_remove_ioctl, dm_dev_resume_ioctl, dm_table_clear_ioctl
 *
 * Writers can work with table_head only when there are no readers. I
 * use reference counting on io_cnt.
 *
 */

static int dm_table_busy(dm_table_head_t *, uint8_t);
static void dm_table_unbusy(dm_table_head_t *);

/*
 * Function to increment table user reference counter. Return id
 * of table_id table.
 * DM_TABLE_ACTIVE will return active table id.
 * DM_TABLE_INACTIVE will return inactive table id.
 */
static int
dm_table_busy(dm_table_head_t * head, uint8_t table_id)
{
	uint8_t id;

	id = 0;

	mutex_enter(&head->table_mtx);

	if (table_id == DM_TABLE_ACTIVE)
		id = head->cur_active_table;
	else
		id = 1 - head->cur_active_table;

	head->io_cnt++;

	mutex_exit(&head->table_mtx);
	return id;
}
/*
 * Function release table lock and eventually wakeup all waiters.
 */
static void
dm_table_unbusy(dm_table_head_t * head)
{
	KASSERT(head->io_cnt != 0);

	mutex_enter(&head->table_mtx);

	if (--head->io_cnt == 0)
		cv_broadcast(&head->table_cv);

	mutex_exit(&head->table_mtx);
}
/*
 * Return current active table to caller, increment io_cnt reference counter.
 */
dm_table_t *
dm_table_get_entry(dm_table_head_t * head, uint8_t table_id)
{
	uint8_t id;

	id = dm_table_busy(head, table_id);

	return &head->tables[id];
}
/*
 * Decrement io reference counter and wake up all callers, with table_head cv.
 */
void
dm_table_release(dm_table_head_t * head, uint8_t table_id)
{
	dm_table_unbusy(head);
}
/*
 * Switch table from inactive to active mode. Have to wait until io_cnt is 0.
 */
void
dm_table_switch_tables(dm_table_head_t * head)
{
	mutex_enter(&head->table_mtx);

	while (head->io_cnt != 0)
		cv_wait(&head->table_cv, &head->table_mtx);

	head->cur_active_table = 1 - head->cur_active_table;

	mutex_exit(&head->table_mtx);
}
/*
 * Destroy all table data. This function can run when there are no
 * readers on table lists.
 *
 * XXX Is it ok to call kmem_free and potentialy VOP_CLOSE with held mutex ?xs
 */
int
dm_table_destroy(dm_table_head_t * head, uint8_t table_id)
{
	dm_table_t *tbl;
	dm_table_entry_t *table_en;
	uint8_t id;

	mutex_enter(&head->table_mtx);

	aprint_debug("dm_Table_destroy called with %d--%d\n", table_id, head->io_cnt);

	while (head->io_cnt != 0)
		cv_wait(&head->table_cv, &head->table_mtx);

	if (table_id == DM_TABLE_ACTIVE)
		id = head->cur_active_table;
	else
		id = 1 - head->cur_active_table;

	tbl = &head->tables[id];

	while (!SLIST_EMPTY(tbl)) {	/* List Deletion. */
		table_en = SLIST_FIRST(tbl);
		/*
		 * Remove target specific config data. After successfull
		 * call table_en->target_config must be set to NULL.
		 */
		table_en->target->destroy(table_en);

		SLIST_REMOVE_HEAD(tbl, next);

		kmem_free(table_en, sizeof(*table_en));
	}

	mutex_exit(&head->table_mtx);

	return 0;
}
/*
 * Return length of active table in device.
 */
static inline uint64_t
dm_table_size_impl(dm_table_head_t * head, int table)
{
	dm_table_t *tbl;
	dm_table_entry_t *table_en;
	uint64_t length;
	uint8_t id;

	length = 0;

	id = dm_table_busy(head, table);

	/* Select active table */
	tbl = &head->tables[id];

	/*
	 * Find out what tables I want to select.
	 * if length => rawblkno then we should used that table.
	 */
	SLIST_FOREACH(table_en, tbl, next)
	    length += table_en->length;

	dm_table_unbusy(head);

	return length;
}

/*
 * Return length of active table in device.
 */
uint64_t
dm_table_size(dm_table_head_t * head)
{
	return dm_table_size_impl(head, DM_TABLE_ACTIVE);
}

/*
 * Return length of active table in device.
 */
uint64_t
dm_inactive_table_size(dm_table_head_t * head)
{
	return dm_table_size_impl(head, DM_TABLE_INACTIVE);
}

/*
 * Return combined disk geometry
 */
void
dm_table_disksize(dm_table_head_t * head, uint64_t *numsecp, unsigned *secsizep)
{
	dm_table_t *tbl;
	dm_table_entry_t *table_en;
	uint64_t length;
	unsigned secsize, tsecsize;
	uint8_t id;

	length = 0;

	id = dm_table_busy(head, DM_TABLE_ACTIVE);

	/* Select active table */
	tbl = &head->tables[id];

	/*
	 * Find out what tables I want to select.
	 * if length => rawblkno then we should used that table.
	 */
	secsize = 0;
	SLIST_FOREACH(table_en, tbl, next) {
	    length += table_en->length;
	    (void)table_en->target->secsize(table_en, &tsecsize);
	    if (secsize < tsecsize)
	    	secsize = tsecsize;
	}
	*numsecp = secsize > 0 ? dbtob(length) / secsize : 0;
	*secsizep = secsize;

	dm_table_unbusy(head);
}
/*
 * Return > 0 if table is at least one table entry (returns number of entries)
 * and return 0 if there is not. Target count returned from this function
 * doesn't need to be true when userspace user receive it (after return
 * there can be dm_dev_resume_ioctl), therfore this isonly informative.
 */
int
dm_table_get_target_count(dm_table_head_t * head, uint8_t table_id)
{
	dm_table_entry_t *table_en;
	dm_table_t *tbl;
	uint32_t target_count;
	uint8_t id;

	target_count = 0;

	id = dm_table_busy(head, table_id);

	tbl = &head->tables[id];

	SLIST_FOREACH(table_en, tbl, next)
	    target_count++;

	dm_table_unbusy(head);

	return target_count;
}


/*
 * Initialize table_head structures, I'm trying to keep this structure as
 * opaque as possible.
 */
void
dm_table_head_init(dm_table_head_t * head)
{
	head->cur_active_table = 0;
	head->io_cnt = 0;

	/* Initialize tables. */
	SLIST_INIT(&head->tables[0]);
	SLIST_INIT(&head->tables[1]);

	mutex_init(&head->table_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&head->table_cv, "dm_io");
}
/*
 * Destroy all variables in table_head
 */
void
dm_table_head_destroy(dm_table_head_t * head)
{
	KASSERT(!mutex_owned(&head->table_mtx));
	KASSERT(!cv_has_waiters(&head->table_cv));
	/* tables doens't exists when I call this routine, therefore it
	 * doesn't make sense to have io_cnt != 0 */
	KASSERT(head->io_cnt == 0);

	cv_destroy(&head->table_cv);
	mutex_destroy(&head->table_mtx);
}
