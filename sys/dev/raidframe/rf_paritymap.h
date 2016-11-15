/* $NetBSD: rf_paritymap.h,v 1.2 2010/03/14 21:11:41 jld Exp $ */

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

#include <sys/mutex.h>
#include <sys/param.h>
#include <sys/rwlock.h>
#include <sys/types.h>

#include <dev/raidframe/raidframevar.h>

/* RF_PARITYMAP_N* in raidframevar.h */

#define RF_PMLABEL_VALID 1
#define RF_PMLABEL_WASUSED 2
#define RF_PMLABEL_DISABLE 4

/*
 * On-disk format: a single bit for each region; if the bit is clear,
 * then the parity is clean.
 */
struct rf_paritymap_ondisk
{
        /* XXX Do these really need to be volatile? */
	volatile char bits[RF_PARITYMAP_NBYTE];
};

/* In-core per-region state: a byte for each, encoded as follows. */
struct rf_paritymap_current
{
	volatile int8_t state[RF_PARITYMAP_NREG];
	/*
	 * Values:
	 * if x == 0, the region may be written out as clean
	 * if x > 0, then x outstanding IOs to that region
	 * if x < 0, then there was recently IO; periodically increment x
	 */
};

/* The entire state. */
struct rf_paritymap
{
	struct rf_paritymap_ondisk *disk_boot, *disk_now;
	struct rf_paritymap_current *current;

	/*
	 * This lock will be held while component disks' caches are
	 * flushed, which could take many milliseconds, so it should
	 * not be taken where that kind of delay is unacceptable.
	 * Contention on this lock is not, however, expected to be a
	 * performance bottleneck.
	 */
	kmutex_t lock;
	/*
	 * The flags field, below, has its own lock so that
	 * inter-thread communication can occur without taking the
	 * overall lock.  Ordering is lock -> lk_flags.
	 */
	kmutex_t lk_flags;

	RF_Raid_t *raid;
	daddr_t region_size;
	callout_t ticker;
	struct rf_pmparams params;
	volatile int flags;
	struct rf_pmctrs ctrs;
};

void rf_paritymap_status(struct rf_paritymap *, struct rf_pmstat *);

int rf_paritymap_test(struct rf_paritymap *, daddr_t);
void rf_paritymap_begin_region(struct rf_paritymap *, unsigned);
void rf_paritymap_begin(struct rf_paritymap *, daddr_t, daddr_t);
void rf_paritymap_end_region(struct rf_paritymap *, unsigned);
void rf_paritymap_end(struct rf_paritymap *, daddr_t, daddr_t);

void rf_paritymap_checkwork(struct rf_paritymap *);
void rf_paritymap_invalidate(struct rf_paritymap *);
void rf_paritymap_forceclean(struct rf_paritymap *);
void rf_paritymap_write(struct rf_paritymap *);

int rf_paritymap_init(struct rf_paritymap *, RF_Raid_t *,
    const struct rf_pmparams *);
void rf_paritymap_destroy(struct rf_paritymap *, int);

int rf_paritymap_rewrite(struct rf_paritymap *);

int rf_paritymap_merge(struct rf_paritymap_ondisk *,
    struct rf_paritymap_ondisk *);

int rf_paritymap_ineligible(RF_Raid_t *);
void rf_paritymap_attach(RF_Raid_t *, int);
void rf_paritymap_detach(RF_Raid_t *); /* Not while the RAID is live! */

int rf_paritymap_get_disable(RF_Raid_t *);
void rf_paritymap_set_disable(RF_Raid_t *, int);

int rf_paritymap_set_params(struct rf_paritymap *,
    const struct rf_pmparams *, int);

void rf_paritymap_init_label(struct rf_paritymap *,
    RF_ComponentLabel_t *);
