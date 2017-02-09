/*	$NetBSD: rf_mcpair.c,v 1.24 2011/05/01 01:09:05 mrg Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* rf_mcpair.c
 * an mcpair is a structure containing a mutex and a condition variable.
 * it's used to block the current thread until some event occurs.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_mcpair.c,v 1.24 2011/05/01 01:09:05 mrg Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_archs.h"
#include "rf_threadstuff.h"
#include "rf_mcpair.h"
#include "rf_debugMem.h"
#include "rf_general.h"
#include "rf_shutdown.h"
#include "rf_netbsd.h"

#include <sys/pool.h>
#include <sys/proc.h>

#define RF_MAX_FREE_MCPAIR 128
#define RF_MIN_FREE_MCPAIR  24

static void rf_ShutdownMCPair(void *);

static void
rf_ShutdownMCPair(void *ignored)
{
	pool_destroy(&rf_pools.mcpair);
}

int
rf_ConfigureMCPair(RF_ShutdownList_t **listp)
{

	rf_pool_init(&rf_pools.mcpair, sizeof(RF_MCPair_t),
		     "rf_mcpair_pl", RF_MIN_FREE_MCPAIR, RF_MAX_FREE_MCPAIR);
	rf_ShutdownCreate(listp, rf_ShutdownMCPair, NULL);

	return (0);
}

RF_MCPair_t *
rf_AllocMCPair(void)
{
	RF_MCPair_t *t;

	t = pool_get(&rf_pools.mcpair, PR_WAITOK);
	rf_init_mutex2(t->mutex, IPL_VM);
	rf_init_cond2(t->cond, "mcpair");
	t->flag = 0;

	return (t);
}

void
rf_FreeMCPair(RF_MCPair_t *t)
{
	rf_destroy_cond2(t->cond);
	rf_destroy_mutex2(t->mutex);
	pool_put(&rf_pools.mcpair, t);
}

/* the callback function used to wake you up when you use an mcpair to
   wait for something */
void
rf_MCPairWakeupFunc(RF_MCPair_t *mcpair)
{
	RF_LOCK_MCPAIR(mcpair);
	mcpair->flag = 1;
	rf_broadcast_cond2(mcpair->cond);
	RF_UNLOCK_MCPAIR(mcpair);
}
