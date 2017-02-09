/* $NetBSD: rf_paritymap.c,v 1.8 2011/04/27 07:55:15 mrg Exp $ */

/*-
 * Copyright (c) 2009 Jed Davis.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_paritymap.c,v 1.8 2011/04/27 07:55:15 mrg Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <dev/raidframe/rf_paritymap.h>
#include <dev/raidframe/rf_stripelocks.h>
#include <dev/raidframe/rf_layout.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_parityscan.h>
#include <dev/raidframe/rf_kintf.h>

/* Important parameters: */
#define REGION_MINSIZE (25ULL << 20)
#define DFL_TICKMS      40000
#define DFL_COOLDOWN    8     /* 7-8 intervals of 40s = 5min +/- 20s */

/* Internal-use flag bits. */
#define TICKING 1
#define TICKED 2

/* Prototypes! */
static void rf_paritymap_write_locked(struct rf_paritymap *);
static void rf_paritymap_tick(void *);
static u_int rf_paritymap_nreg(RF_Raid_t *);

/* Extract the current status of the parity map. */
void
rf_paritymap_status(struct rf_paritymap *pm, struct rf_pmstat *ps)
{
	memset(ps, 0, sizeof(*ps));
	if (pm == NULL)
		ps->enabled = 0;
	else {
		ps->enabled = 1;
		ps->region_size = pm->region_size;
		mutex_enter(&pm->lock);
		memcpy(&ps->params, &pm->params, sizeof(ps->params));
		memcpy(ps->dirty, pm->disk_now, sizeof(ps->dirty));
		memcpy(&ps->ctrs, &pm->ctrs, sizeof(ps->ctrs));
		mutex_exit(&pm->lock);
	}
}

/* 
 * Test whether parity in a given sector is suspected of being inconsistent
 * on disk (assuming that any pending I/O to it is allowed to complete).
 * This may be of interest to future work on parity scrubbing.
 */
int
rf_paritymap_test(struct rf_paritymap *pm, daddr_t sector)
{
	unsigned region = sector / pm->region_size;
	int retval;

	mutex_enter(&pm->lock);
	retval = isset(pm->disk_boot->bits, region) ? 1 : 0;
	mutex_exit(&pm->lock);
	return retval;
}

/* To be called before a write to the RAID is submitted. */
void
rf_paritymap_begin(struct rf_paritymap *pm, daddr_t offset, daddr_t size)
{
	unsigned i, b, e;

	b = offset / pm->region_size;
	e = (offset + size - 1) / pm->region_size;

	for (i = b; i <= e; i++)
		rf_paritymap_begin_region(pm, i);
}

/* To be called after a write to the RAID completes. */
void
rf_paritymap_end(struct rf_paritymap *pm, daddr_t offset, daddr_t size)
{
	unsigned i, b, e;

	b = offset / pm->region_size;
	e = (offset + size - 1) / pm->region_size;

	for (i = b; i <= e; i++)
		rf_paritymap_end_region(pm, i);
}

void
rf_paritymap_begin_region(struct rf_paritymap *pm, unsigned region)
{
	int needs_write;

	KASSERT(region < RF_PARITYMAP_NREG);
	pm->ctrs.nwrite++;

	/* If it was being kept warm, deal with that. */
	mutex_enter(&pm->lock);
	if (pm->current->state[region] < 0)
		pm->current->state[region] = 0;

	/* This shouldn't happen unless RAIDOUTSTANDING is set too high. */
	KASSERT(pm->current->state[region] < 127);
	pm->current->state[region]++;

	needs_write = isclr(pm->disk_now->bits, region);

	if (needs_write) {
		KASSERT(pm->current->state[region] == 1);
		rf_paritymap_write_locked(pm);
	}

	mutex_exit(&pm->lock);
}

void
rf_paritymap_end_region(struct rf_paritymap *pm, unsigned region)
{
	KASSERT(region < RF_PARITYMAP_NREG);

	mutex_enter(&pm->lock);
	KASSERT(pm->current->state[region] > 0);
	--pm->current->state[region];

	if (pm->current->state[region] <= 0) {
		pm->current->state[region] = -pm->params.cooldown;
		KASSERT(pm->current->state[region] <= 0);
		mutex_enter(&pm->lk_flags);
		if (!(pm->flags & TICKING)) {
			pm->flags |= TICKING;
			mutex_exit(&pm->lk_flags);
			callout_schedule(&pm->ticker,
			    mstohz(pm->params.tickms));
		} else
			mutex_exit(&pm->lk_flags);
	}
	mutex_exit(&pm->lock);
}

/* 
 * Updates the parity map to account for any changes in current activity
 * and/or an ongoing parity scan, then writes it to disk with appropriate
 * synchronization. 
 */
void
rf_paritymap_write(struct rf_paritymap *pm)
{
	mutex_enter(&pm->lock);
	rf_paritymap_write_locked(pm);
	mutex_exit(&pm->lock);
}

/* As above, but to be used when pm->lock is already held. */
static void
rf_paritymap_write_locked(struct rf_paritymap *pm)
{
	char w, w0;
	int i, j, setting, clearing;

	setting = clearing = 0;
	for (i = 0; i < RF_PARITYMAP_NBYTE; i++) {
		w0 = pm->disk_now->bits[i];
		w = pm->disk_boot->bits[i];

		for (j = 0; j < NBBY; j++)
			if (pm->current->state[i * NBBY + j] != 0)
				w |= 1 << j;

		if (w & ~w0)
			setting = 1;
		if (w0 & ~w)
			clearing = 1;

		pm->disk_now->bits[i] = w;
	}
	pm->ctrs.ncachesync += setting + clearing;
	pm->ctrs.nclearing += clearing;

	/*
	 * If bits are being set in the parity map, then a sync is
	 * required afterwards, so that the regions are marked dirty
	 * on disk before any writes to them take place.  If bits are
	 * being cleared, then a sync is required before the write, so
	 * that any writes to those regions are processed before the
	 * region is marked clean.  (Synchronization is somewhat
	 * overkill; a write ordering barrier would suffice, but we
	 * currently have no way to express that directly.)
	 */
	if (clearing)
		rf_sync_component_caches(pm->raid);
	rf_paritymap_kern_write(pm->raid, pm->disk_now);
	if (setting)
		rf_sync_component_caches(pm->raid);
}

/* Mark all parity as being in need of rewrite. */
void
rf_paritymap_invalidate(struct rf_paritymap *pm)
{
	mutex_enter(&pm->lock);
	memset(pm->disk_boot, ~(unsigned char)0,
	    sizeof(struct rf_paritymap_ondisk));
	mutex_exit(&pm->lock);
}

/* Mark all parity as being correct. */
void
rf_paritymap_forceclean(struct rf_paritymap *pm)
{
	mutex_enter(&pm->lock);
	memset(pm->disk_boot, (unsigned char)0,
	    sizeof(struct rf_paritymap_ondisk));
	mutex_exit(&pm->lock);
}

/*
 * The cooldown callout routine just defers its work to a thread; it can't do
 * the parity map write itself as it would block, and although mutex-induced
 * blocking is permitted it seems wise to avoid tying up the softint.
 */
static void
rf_paritymap_tick(void *arg)
{
	struct rf_paritymap *pm = arg;

	mutex_enter(&pm->lk_flags);
	pm->flags |= TICKED;
	mutex_exit(&pm->lk_flags);

	rf_lock_mutex2(pm->raid->iodone_lock);
	rf_signal_cond2(pm->raid->iodone_cv); /* XXX */
	rf_unlock_mutex2(pm->raid->iodone_lock);
}

/*
 * This is where the parity cooling work (and rearming the callout if needed)
 * is done; the raidio thread calls it when woken up, as by the above.
 */
void
rf_paritymap_checkwork(struct rf_paritymap *pm)
{
	int i, zerop, progressp;

	mutex_enter(&pm->lk_flags);
	if (pm->flags & TICKED) {
		zerop = progressp = 0;

		pm->flags &= ~TICKED;
		mutex_exit(&pm->lk_flags);

		mutex_enter(&pm->lock);
		for (i = 0; i < RF_PARITYMAP_NREG; i++) {
			if (pm->current->state[i] < 0) {
				progressp = 1;
				pm->current->state[i]++;
				if (pm->current->state[i] == 0)
					zerop = 1;
			}
		}

		if (progressp)
			callout_schedule(&pm->ticker,
			    mstohz(pm->params.tickms));
		else {
			mutex_enter(&pm->lk_flags);
			pm->flags &= ~TICKING;
			mutex_exit(&pm->lk_flags);
		}

		if (zerop)
			rf_paritymap_write_locked(pm);
		mutex_exit(&pm->lock);
	} else
		mutex_exit(&pm->lk_flags);
}

/*
 * Set parity map parameters; used both to alter parameters on the fly and to
 * establish their initial values.  Note that setting a parameter to 0 means
 * to leave the previous setting unchanged, and that if this is done for the
 * initial setting of "regions", then a default value will be computed based
 * on the RAID component size.
 */
int
rf_paritymap_set_params(struct rf_paritymap *pm,
    const struct rf_pmparams *params, int todisk)
{
	int cooldown, tickms;
	u_int regions;
	RF_RowCol_t col;
	RF_ComponentLabel_t *clabel;
	RF_Raid_t *raidPtr;

	cooldown = params->cooldown != 0
	    ? params->cooldown : pm->params.cooldown;
	tickms = params->tickms != 0
	    ? params->tickms : pm->params.tickms;
	regions = params->regions != 0
	    ? params->regions : pm->params.regions;

	if (cooldown < 1 || cooldown > 128) {
		printf("raid%d: cooldown %d out of range\n", pm->raid->raidid,
		    cooldown);
		return (-1);
	}
	if (tickms < 10) {
		printf("raid%d: tick time %dms out of range\n",
		    pm->raid->raidid, tickms);
		return (-1);
	}
	if (regions == 0) {
		regions = rf_paritymap_nreg(pm->raid);
	} else if (regions > RF_PARITYMAP_NREG) {
		printf("raid%d: region count %u too large (more than %u)\n",
		    pm->raid->raidid, regions, RF_PARITYMAP_NREG);
		return (-1);
	}

	/* XXX any currently warm parity will be used with the new tickms! */
	pm->params.cooldown = cooldown;
	pm->params.tickms = tickms;
	/* Apply the initial region count, but do not change it after that. */
	if (pm->params.regions == 0)
		pm->params.regions = regions;

	/* So that the newly set parameters can be tested: */
	pm->ctrs.nwrite = pm->ctrs.ncachesync = pm->ctrs.nclearing = 0;

	if (todisk) {
		raidPtr = pm->raid;
		for (col = 0; col < raidPtr->numCol; col++) {
			if (RF_DEAD_DISK(raidPtr->Disks[col].status))
				continue;

			clabel = raidget_component_label(raidPtr, col);
			clabel->parity_map_ntick = cooldown;
			clabel->parity_map_tickms = tickms;
			clabel->parity_map_regions = regions;
			
			/* Don't touch the disk if it's been spared */
			if (clabel->status == rf_ds_spared)
				continue;
				
			raidflush_component_label(raidPtr, col);
		}

		/* handle the spares too... */
		for (col = 0; col < raidPtr->numSpare; col++) {
			if (raidPtr->Disks[raidPtr->numCol+col].status == rf_ds_used_spare) {
				clabel = raidget_component_label(raidPtr, raidPtr->numCol+col);
				clabel->parity_map_ntick = cooldown;
				clabel->parity_map_tickms = tickms;
				clabel->parity_map_regions = regions;
				raidflush_component_label(raidPtr, raidPtr->numCol+col);
			}				
		}
	}
	return 0;
}

/*
 * The number of regions may not be as many as can fit into the map, because
 * when regions are too small, the overhead of setting parity map bits
 * becomes significant in comparison to the actual I/O, while the
 * corresponding gains in parity verification time become negligible.  Thus,
 * a minimum region size (defined above) is imposed.
 *
 * Note that, if the number of regions is less than the maximum, then some of
 * the regions will be "fictional", corresponding to no actual disk; some
 * parts of the code may process them as normal, but they can not ever be
 * written to.
 */
static u_int
rf_paritymap_nreg(RF_Raid_t *raid)
{
	daddr_t bytes_per_disk, nreg;

	bytes_per_disk = raid->sectorsPerDisk << raid->logBytesPerSector;
	nreg = bytes_per_disk / REGION_MINSIZE;
	if (nreg > RF_PARITYMAP_NREG)
		nreg = RF_PARITYMAP_NREG;
	if (nreg < 1)
		nreg = 1;

	return (u_int)nreg;
}

/* 
 * Initialize a parity map given specific parameters.  This neither reads nor
 * writes the parity map config in the component labels; for that, see below.
 */
int
rf_paritymap_init(struct rf_paritymap *pm, RF_Raid_t *raid,
    const struct rf_pmparams *params)
{
	daddr_t rstripes;
	struct rf_pmparams safe;

	pm->raid = raid;
	pm->params.regions = 0;
	if (0 != rf_paritymap_set_params(pm, params, 0)) {
		/*
		 * If the parameters are out-of-range, then bring the
		 * parity map up with something reasonable, so that
		 * the admin can at least go and fix it (or ignore it
		 * entirely).
		 */
		safe.cooldown = DFL_COOLDOWN;
		safe.tickms = DFL_TICKMS;
		safe.regions = 0;

		if (0 != rf_paritymap_set_params(pm, &safe, 0))
			return (-1);
	}

	rstripes = howmany(raid->Layout.numStripe, pm->params.regions);
	pm->region_size = rstripes * raid->Layout.dataSectorsPerStripe;

	callout_init(&pm->ticker, CALLOUT_MPSAFE);
	callout_setfunc(&pm->ticker, rf_paritymap_tick, pm);
	pm->flags = 0;

	pm->disk_boot = kmem_alloc(sizeof(struct rf_paritymap_ondisk),
	    KM_SLEEP);
	pm->disk_now = kmem_alloc(sizeof(struct rf_paritymap_ondisk),
	    KM_SLEEP);
	pm->current = kmem_zalloc(sizeof(struct rf_paritymap_current),
	    KM_SLEEP);

	rf_paritymap_kern_read(pm->raid, pm->disk_boot);
	memcpy(pm->disk_now, pm->disk_boot, sizeof(*pm->disk_now));

	mutex_init(&pm->lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&pm->lk_flags, MUTEX_DEFAULT, IPL_SOFTCLOCK);

	return 0;
}

/*
 * Destroys a parity map; unless "force" is set, also cleans parity for any
 * regions which were still in cooldown (but are not dirty on disk).
 */
void
rf_paritymap_destroy(struct rf_paritymap *pm, int force)
{
	int i;

	callout_halt(&pm->ticker, NULL); /* XXX stop? halt? */
	callout_destroy(&pm->ticker);

	if (!force) {
		for (i = 0; i < RF_PARITYMAP_NREG; i++) {
			/* XXX check for > 0 ? */
			if (pm->current->state[i] < 0)
				pm->current->state[i] = 0;
		}

		rf_paritymap_write_locked(pm);
	}

	mutex_destroy(&pm->lock);
	mutex_destroy(&pm->lk_flags);

	kmem_free(pm->disk_boot, sizeof(struct rf_paritymap_ondisk));
	kmem_free(pm->disk_now, sizeof(struct rf_paritymap_ondisk));
	kmem_free(pm->current, sizeof(struct rf_paritymap_current));
}

/*
 * Rewrite parity, taking parity map into account; this is the equivalent of
 * the old rf_RewriteParity, and is likewise to be called from a suitable
 * thread and shouldn't have multiple copies running in parallel and so on.
 *
 * Note that the fictional regions are "cleaned" in one shot, so that very
 * small RAIDs (useful for testing) will not experience potentially severe
 * regressions in rewrite time.
 */
int
rf_paritymap_rewrite(struct rf_paritymap *pm)
{
	int i, ret_val = 0;
	daddr_t reg_b, reg_e;

	/* Process only the actual regions. */
	for (i = 0; i < pm->params.regions; i++) {
		mutex_enter(&pm->lock);
		if (isset(pm->disk_boot->bits, i)) {
			mutex_exit(&pm->lock);

			reg_b = i * pm->region_size;
			reg_e = reg_b + pm->region_size;
			if (reg_e > pm->raid->totalSectors)
				reg_e = pm->raid->totalSectors;

			if (rf_RewriteParityRange(pm->raid, reg_b,
			    reg_e - reg_b)) {
				ret_val = 1;
				if (pm->raid->waitShutdown)
					return ret_val;
			} else {
				mutex_enter(&pm->lock);
				clrbit(pm->disk_boot->bits, i);
				rf_paritymap_write_locked(pm);
				mutex_exit(&pm->lock);
			}
		} else {
			mutex_exit(&pm->lock);
		}
	}

	/* Now, clear the fictional regions, if any. */
	rf_paritymap_forceclean(pm);
	rf_paritymap_write(pm);

	return ret_val;
}

/*
 * How to merge the on-disk parity maps when reading them in from the
 * various components; returns whether they differ.  In the case that
 * they do differ, sets *dst to the union of *dst and *src.
 *
 * In theory, it should be safe to take the intersection (or just pick
 * a single component arbitrarily), but the paranoid approach costs
 * little.
 *
 * Appropriate locking, if any, is the responsibility of the caller.
 */
int
rf_paritymap_merge(struct rf_paritymap_ondisk *dst,
    struct rf_paritymap_ondisk *src)
{
	int i, discrep = 0;

	for (i = 0; i < RF_PARITYMAP_NBYTE; i++) {
		if (dst->bits[i] != src->bits[i])
			discrep = 1;
		dst->bits[i] |= src->bits[i];
	}

	return discrep;
}

/*
 * Detach a parity map from its RAID.  This is not meant to be applied except
 * when unconfiguring the RAID after all I/O has been resolved, as otherwise
 * an out-of-date parity map could be treated as current.
 */
void
rf_paritymap_detach(RF_Raid_t *raidPtr)
{
	if (raidPtr->parity_map == NULL)
		return;

	rf_lock_mutex2(raidPtr->iodone_lock);
	struct rf_paritymap *pm = raidPtr->parity_map;
	raidPtr->parity_map = NULL;
	rf_unlock_mutex2(raidPtr->iodone_lock);
	/* XXXjld is that enough locking?  Or too much? */
	rf_paritymap_destroy(pm, 0);
	kmem_free(pm, sizeof(*pm));
}

/*
 * Is this RAID set ineligible for parity-map use due to not actually
 * having any parity?  (If so, rf_paritymap_attach is a no-op, but
 * rf_paritymap_{get,set}_disable will still pointlessly act on the
 * component labels.)
 */
int
rf_paritymap_ineligible(RF_Raid_t *raidPtr)
{
	return raidPtr->Layout.map->faultsTolerated == 0;
}

/*
 * Attach a parity map to a RAID set if appropriate.  Includes
 * configure-time processing of parity-map fields of component label.
 */
void
rf_paritymap_attach(RF_Raid_t *raidPtr, int force)
{
	RF_RowCol_t col;
	int pm_use, pm_zap;
	int g_tickms, g_ntick, g_regions;
	int good;
	RF_ComponentLabel_t *clabel;
	u_int flags, regions;
	struct rf_pmparams params;

	if (rf_paritymap_ineligible(raidPtr)) {
		/* There isn't any parity. */
		return;
	}

	pm_use = 1;
	pm_zap = 0;
	g_tickms = DFL_TICKMS;
	g_ntick = DFL_COOLDOWN;
	g_regions = 0;

	/*
	 * Collect opinions on the set config.  If this is the initial
	 * config (raidctl -C), treat all labels as invalid, since
	 * there may be random data present.
	 */
	if (!force) {
		for (col = 0; col < raidPtr->numCol; col++) {
			if (RF_DEAD_DISK(raidPtr->Disks[col].status))
				continue;
			clabel = raidget_component_label(raidPtr, col);
			flags = clabel->parity_map_flags;
			/* Check for use by non-parity-map kernel. */
			if (clabel->parity_map_modcount
			    != clabel->mod_counter) {
				flags &= ~RF_PMLABEL_WASUSED;
			}

			if (flags & RF_PMLABEL_VALID) {
				g_tickms = clabel->parity_map_tickms;
				g_ntick = clabel->parity_map_ntick;
				regions = clabel->parity_map_regions;
				if (g_regions == 0)
					g_regions = regions;
				else if (g_regions != regions) {
					pm_zap = 1; /* important! */
				}

				if (flags & RF_PMLABEL_DISABLE) {
					pm_use = 0;
				}
				if (!(flags & RF_PMLABEL_WASUSED)) {
					pm_zap = 1;
				}
			} else {
				pm_zap = 1;
			}
		}
	} else {
		pm_zap = 1;
	}

	/* Finally, create and attach the parity map. */
	if (pm_use) {
		params.cooldown = g_ntick;
		params.tickms = g_tickms;
		params.regions = g_regions;

		raidPtr->parity_map = kmem_alloc(sizeof(struct rf_paritymap),
		    KM_SLEEP);
		if (0 != rf_paritymap_init(raidPtr->parity_map, raidPtr,
			&params)) {
			/* It failed; do without. */
			kmem_free(raidPtr->parity_map,
			    sizeof(struct rf_paritymap));
			raidPtr->parity_map = NULL;
			return;
		}

		if (g_regions == 0)
			/* Pick up the autoconfigured region count. */
			g_regions = raidPtr->parity_map->params.regions;

		if (pm_zap) {
			good = raidPtr->parity_good && !force;

			if (good)
				rf_paritymap_forceclean(raidPtr->parity_map);
			else
				rf_paritymap_invalidate(raidPtr->parity_map);
			/* This needs to be on disk before WASUSED is set. */
			rf_paritymap_write(raidPtr->parity_map);
		}
	}

	/* Alter labels in-core to reflect the current view of things. */
	for (col = 0; col < raidPtr->numCol; col++) {
		if (RF_DEAD_DISK(raidPtr->Disks[col].status))
			continue;
		clabel = raidget_component_label(raidPtr, col);

		if (pm_use)
			flags = RF_PMLABEL_VALID | RF_PMLABEL_WASUSED;
		else
			flags = RF_PMLABEL_VALID | RF_PMLABEL_DISABLE;

		clabel->parity_map_flags = flags;
		clabel->parity_map_tickms = g_tickms;
		clabel->parity_map_ntick = g_ntick;
		clabel->parity_map_regions = g_regions;
		raidflush_component_label(raidPtr, col);
	}
	/* Note that we're just in 'attach' here, and there won't
	   be any spare disks at this point. */
}

/*
 * For initializing the parity-map fields of a component label, both on
 * initial creation and on reconstruct/copyback/etc.  */
void
rf_paritymap_init_label(struct rf_paritymap *pm, RF_ComponentLabel_t *clabel)
{
	if (pm != NULL) {
		clabel->parity_map_flags =
		    RF_PMLABEL_VALID | RF_PMLABEL_WASUSED;
		clabel->parity_map_tickms = pm->params.tickms;
		clabel->parity_map_ntick = pm->params.cooldown;
		/*
		 * XXXjld: If the number of regions is changed on disk, and
		 * then a new component is labeled before the next configure,
		 * then it will get the old value and they will conflict on
		 * the next boot (and the default will be used instead).
		 */
		clabel->parity_map_regions = pm->params.regions;
	} else {
		/*
		 * XXXjld: if the map is disabled, and all the components are
		 * replaced without an intervening unconfigure/reconfigure,
		 * then it will become enabled on the next unconfig/reconfig.
		 */
	}
}


/* Will the parity map be disabled next time? */
int
rf_paritymap_get_disable(RF_Raid_t *raidPtr)
{
	RF_ComponentLabel_t *clabel;
	RF_RowCol_t col;
	int dis;

	dis = 0;
	for (col = 0; col < raidPtr->numCol; col++) {
		if (RF_DEAD_DISK(raidPtr->Disks[col].status))
			continue;
		clabel = raidget_component_label(raidPtr, col);
		if (clabel->parity_map_flags & RF_PMLABEL_DISABLE)
			dis = 1;
	}
        for (col = 0; col < raidPtr->numSpare; col++) {
		if (raidPtr->Disks[raidPtr->numCol+col].status != rf_ds_used_spare)
                        continue;
                clabel = raidget_component_label(raidPtr, raidPtr->numCol+col);
                if (clabel->parity_map_flags & RF_PMLABEL_DISABLE)
                        dis = 1;
        }

	return dis;
}

/* Set whether the parity map will be disabled next time. */
void
rf_paritymap_set_disable(RF_Raid_t *raidPtr, int dis)
{
	RF_ComponentLabel_t *clabel;
	RF_RowCol_t col;

	for (col = 0; col < raidPtr->numCol; col++) {
		if (RF_DEAD_DISK(raidPtr->Disks[col].status))
			continue;
		clabel = raidget_component_label(raidPtr, col);
		if (dis)
			clabel->parity_map_flags |= RF_PMLABEL_DISABLE;
		else
			clabel->parity_map_flags &= ~RF_PMLABEL_DISABLE;
		raidflush_component_label(raidPtr, col);
	}

	/* update any used spares as well */
	for (col = 0; col < raidPtr->numSpare; col++) {
		if (raidPtr->Disks[raidPtr->numCol+col].status != rf_ds_used_spare)
			continue;

		clabel = raidget_component_label(raidPtr, raidPtr->numCol+col);
		if (dis)
			clabel->parity_map_flags |= RF_PMLABEL_DISABLE;
		else
			clabel->parity_map_flags &= ~RF_PMLABEL_DISABLE;
		raidflush_component_label(raidPtr, raidPtr->numCol+col);
	}
}
