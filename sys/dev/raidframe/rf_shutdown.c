/*	$NetBSD: rf_shutdown.c,v 1.20 2008/12/17 20:51:34 cegger Exp $	*/
/*
 * rf_shutdown.c
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
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
/*
 * Maintain lists of cleanup functions. Also, mechanisms for coordinating
 * thread startup and shutdown.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_shutdown.c,v 1.20 2008/12/17 20:51:34 cegger Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_archs.h"
#include "rf_shutdown.h"
#include "rf_debugMem.h"


#ifndef RF_DEBUG_SHUTDOWN
#define RF_DEBUG_SHUTDOWN 0
#endif

static void rf_FreeShutdownEnt(RF_ShutdownList_t *);

static void
rf_FreeShutdownEnt(RF_ShutdownList_t *ent)
{
	free(ent, M_RAIDFRAME);
}

#if RF_DEBUG_SHUTDOWN
void
_rf_ShutdownCreate(RF_ShutdownList_t **listp,  void (*cleanup)(void *arg),
		   void *arg, char *file, int line)
#else
void
_rf_ShutdownCreate(RF_ShutdownList_t **listp,  void (*cleanup)(void *arg),
		   void *arg)
#endif
{
	RF_ShutdownList_t *ent;

	/*
         * Have to directly allocate memory here, since we start up before
         * and shutdown after RAIDframe internal allocation system.
         */
	/* 	ent = (RF_ShutdownList_t *) malloc(sizeof(RF_ShutdownList_t),
		M_RAIDFRAME, M_WAITOK); */
	ent = (RF_ShutdownList_t *) malloc(sizeof(RF_ShutdownList_t),
					   M_RAIDFRAME, M_WAITOK);
	ent->cleanup = cleanup;
	ent->arg = arg;
#if RF_DEBUG_SHUTDOWN
	ent->file = file;
	ent->line = line;
#endif
	ent->next = *listp;
	*listp = ent;
}

void
rf_ShutdownList(RF_ShutdownList_t **list)
{
	RF_ShutdownList_t *r, *next;
#if RF_DEBUG_SHUTDOWN
	char   *file;
	int     line;
#endif

	for (r = *list; r; r = next) {
		next = r->next;
#if RF_DEBUG_SHUTDOWN
		file = r->file;
		line = r->line;

		if (rf_shutdownDebug) {
			printf("call shutdown, created %s:%d\n", file, line);
		}
#endif
		r->cleanup(r->arg);
#if RF_DEBUG_SHUTDOWN
		if (rf_shutdownDebug) {
			printf("completed shutdown, created %s:%d\n", file, line);
		}
#endif
		rf_FreeShutdownEnt(r);
	}
	*list = NULL;
}
