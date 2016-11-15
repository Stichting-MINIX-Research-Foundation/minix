/*	$NetBSD: rf_mcpair.h,v 1.10 2011/05/01 01:09:05 mrg Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
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

/* rf_mcpair.h
 * see comments in rf_mcpair.c
 */

#ifndef _RF__RF_MCPAIR_H_
#define _RF__RF_MCPAIR_H_

#include <dev/raidframe/raidframevar.h>
#include "rf_threadstuff.h"

struct RF_MCPair_s {
	rf_declare_mutex2(mutex);
	rf_declare_cond2(cond);
	int     flag;
};
#define RF_WAIT_MCPAIR(_mcp)	rf_wait_cond2((_mcp)->cond, (_mcp)->mutex)
#define RF_LOCK_MCPAIR(_mcp)	rf_lock_mutex2((_mcp)->mutex)
#define RF_UNLOCK_MCPAIR(_mcp)	rf_unlock_mutex2((_mcp)->mutex)

int     rf_ConfigureMCPair(RF_ShutdownList_t ** listp);
RF_MCPair_t *rf_AllocMCPair(void);
void    rf_FreeMCPair(RF_MCPair_t * t);
void    rf_MCPairWakeupFunc(RF_MCPair_t * t);

#endif				/* !_RF__RF_MCPAIR_H_ */
