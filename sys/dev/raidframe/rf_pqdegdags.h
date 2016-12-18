/*	$NetBSD: rf_pqdegdags.h,v 1.3 1999/02/05 00:06:15 oster Exp $	*/
/*
 * rf_pqdegdags.h
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky
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
 * rf_pqdegdags.c
 * Degraded mode dags for double fault cases.
 */

#ifndef _RF__RF_PQDEGDAGS_H_
#define _RF__RF_PQDEGDAGS_H_

#include "rf_dag.h"

RF_CREATE_DAG_FUNC_DECL(rf_PQ_DoubleDegRead);
int     rf_PQDoubleRecoveryFunc(RF_DagNode_t * node);
int     rf_PQWriteDoubleRecoveryFunc(RF_DagNode_t * node);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_DDLargeWrite);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_DDSimpleSmallWrite);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_200_CreateWriteDAG);

#endif				/* !_RF__RF_PQDEGDAGS_H_ */
