/*	$NetBSD: rf_netbsd.h,v 1.30 2013/04/27 21:18:42 christos Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster; Jason R. Thorpe.
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

#ifndef _RF__RF_NETBSDSTUFF_H_
#define _RF__RF_NETBSDSTUFF_H_

#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/pool.h>
#include <sys/disk.h>

#include <dev/dkvar.h>
#include <dev/raidframe/raidframevar.h>

struct raidcinfo {
	struct vnode *ci_vp;	/* component device's vnode */
	dev_t   ci_dev;		/* component device's dev_t */
	RF_ComponentLabel_t ci_label; /* components RAIDframe label */
#if 0
	size_t  ci_size;	/* size */
	char   *ci_path;	/* path to component */
	size_t  ci_pathlen;	/* length of component path */
#endif
};


/* a little structure to serve as a container for all the various
   global pools used in RAIDframe */

struct RF_Pools_s {
	struct pool alloclist;   /* AllocList */
	struct pool asm_hdr;     /* Access Stripe Map Header */
	struct pool asmap;       /* Access Stripe Map */
	struct pool asmhle;      /* Access Stripe Map Header List Elements */
	struct pool callback;    /* Callback descriptors */
	struct pool dagh;        /* DAG headers */
	struct pool dagnode;     /* DAG nodes */
	struct pool daglist;     /* DAG lists */
	struct pool dagpcache;   /* DAG pointer/param cache */
	struct pool dqd;         /* Disk Queue Data */
	struct pool fss;         /* Failed Stripe Structures */
	struct pool funclist;    /* Function Lists */
	struct pool mcpair;      /* Mutex/Cond Pairs */
	struct pool pda;         /* Physical Disk Access structures */
	struct pool pss;         /* Parity Stripe Status */
	struct pool pss_issued;  /* Parity Stripe Status Issued */
	struct pool rad;         /* Raid Access Descriptors */
	struct pool reconbuffer; /* reconstruction buffer (header) pool */
	struct pool revent;      /* reconstruct events */
	struct pool stripelock;  /* StripeLock */
	struct pool vfple;       /* VoidFunctionPtr List Elements */
	struct pool vple;        /* VoidPointer List Elements */
};

extern struct RF_Pools_s rf_pools;
void rf_pool_init(struct pool *, size_t, const char *, size_t, size_t);
int rf_buf_queue_check(RF_Raid_t *);

/* XXX probably belongs in a different .h file. */
typedef struct RF_AutoConfig_s {
	char devname[56];       /* the name of this component */
	int flag;               /* a general-purpose flag */
	dev_t dev;              /* the device for this component */
	struct vnode *vp;       /* Mr. Vnode Pointer */
	RF_ComponentLabel_t *clabel;  /* the label */
	struct RF_AutoConfig_s *next; /* the next autoconfig structure
				         in this set. */
} RF_AutoConfig_t;

typedef struct RF_ConfigSet_s {
	struct RF_AutoConfig_s *ac; /* all of the autoconfig structures for
				       this config set. */
	int rootable;               /* Set to 1 if this set can be root */
	struct RF_ConfigSet_s *next;
} RF_ConfigSet_t;

#endif /* _RF__RF_NETBSDSTUFF_H_ */
