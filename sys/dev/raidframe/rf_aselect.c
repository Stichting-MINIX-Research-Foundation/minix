/*	$NetBSD: rf_aselect.c,v 1.28 2013/09/15 12:11:16 martin Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II
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

/*****************************************************************************
 *
 * aselect.c -- algorithm selection code
 *
 *****************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_aselect.c,v 1.28 2013/09/15 12:11:16 martin Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_archs.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_general.h"
#include "rf_desc.h"
#include "rf_map.h"

static void InitHdrNode(RF_DagHeader_t **, RF_Raid_t *, RF_RaidAccessDesc_t *);
int     rf_SelectAlgorithm(RF_RaidAccessDesc_t *, RF_RaidAccessFlags_t);

/******************************************************************************
 *
 * Create and Initialiaze a dag header and termination node
 *
 *****************************************************************************/
static void
InitHdrNode(RF_DagHeader_t **hdr, RF_Raid_t *raidPtr, RF_RaidAccessDesc_t *desc)
{
	/* create and initialize dag hdr */
	*hdr = rf_AllocDAGHeader();
	rf_MakeAllocList((*hdr)->allocList);
	(*hdr)->status = rf_enable;
	(*hdr)->numSuccedents = 0;
	(*hdr)->nodes = NULL;
	(*hdr)->raidPtr = raidPtr;
	(*hdr)->next = NULL;
	(*hdr)->desc = desc;
}

/******************************************************************************
 *
 * Create a DAG to do a read or write operation.
 *
 * create a list of dagLists, one list per parity stripe.
 * return the lists in the desc->dagList (which is a list of lists).
 *
 * Normally, each list contains one dag for the entire stripe.  In some
 * tricky cases, we break this into multiple dags, either one per stripe
 * unit or one per block (sector).  When this occurs, these dags are returned
 * as a linked list (dagList) which is executed sequentially (to preserve
 * atomic parity updates in the stripe).
 *
 * dags which operate on independent parity goups (stripes) are returned in
 * independent dagLists (distinct elements in desc->dagArray) and may be
 * executed concurrently.
 *
 * Finally, if the SelectionFunc fails to create a dag for a block, we punt
 * and return 1.
 *
 * The above process is performed in two phases:
 *   1) create an array(s) of creation functions (eg stripeFuncs)
 *   2) create dags and concatenate/merge to form the final dag.
 *
 * Because dag's are basic blocks (single entry, single exit, unconditional
 * control flow, we can add the following optimizations (future work):
 *   first-pass optimizer to allow max concurrency (need all data dependencies)
 *   second-pass optimizer to eliminate common subexpressions (need true
 *                         data dependencies)
 *   third-pass optimizer to eliminate dead code (need true data dependencies)
 *****************************************************************************/

int
rf_SelectAlgorithm(RF_RaidAccessDesc_t *desc, RF_RaidAccessFlags_t flags)
{
	RF_AccessStripeMapHeader_t *asm_h = desc->asmap;
	RF_IoType_t type = desc->type;
	RF_Raid_t *raidPtr = desc->raidPtr;
	void   *bp = desc->bp;

	RF_AccessStripeMap_t *asmap = asm_h->stripeMap;
	RF_AccessStripeMap_t *asm_p;
	RF_DagHeader_t *dag_h = NULL, *tempdag_h, *lastdag_h;
	RF_DagList_t *dagList, *dagListend;
	int     i, j, k;
	RF_FuncList_t *stripeFuncsList, *stripeFuncs, *stripeFuncsEnd, *temp;
	RF_AccessStripeMap_t *asm_up, *asm_bp;
	RF_AccessStripeMapHeader_t *endASMList;
	RF_ASMHeaderListElem_t *asmhle, *tmpasmhle;
	RF_VoidFunctionPointerListElem_t *vfple, *tmpvfple;
	RF_FailedStripe_t *failed_stripes_list, *failed_stripes_list_end;
	RF_FailedStripe_t *tmpfailed_stripe, *failed_stripe = NULL;
	RF_ASMHeaderListElem_t *failed_stripes_asmh_u_end = NULL;
	RF_ASMHeaderListElem_t *failed_stripes_asmh_b_end = NULL;
	RF_VoidFunctionPointerListElem_t *failed_stripes_vfple_end = NULL;
	RF_VoidFunctionPointerListElem_t *failed_stripes_bvfple_end = NULL;
	RF_VoidFuncPtr uFunc;
	RF_VoidFuncPtr bFunc;
	int     numStripesBailed = 0, cantCreateDAGs = RF_FALSE;
	int     numStripeUnitsBailed = 0;
	int     stripeNum, numUnitDags = 0, stripeUnitNum, numBlockDags = 0;
	RF_StripeNum_t numStripeUnits;
	RF_SectorNum_t numBlocks;
	RF_RaidAddr_t address;
	int     length;
	RF_PhysDiskAddr_t *physPtr;
	void *buffer;

	lastdag_h = NULL;

	stripeFuncsList = NULL;
	stripeFuncsEnd = NULL;

	failed_stripes_list = NULL;
	failed_stripes_list_end = NULL;

	/* walk through the asm list once collecting information */
	/* attempt to find a single creation function for each stripe */
	desc->numStripes = 0;
	for (i = 0, asm_p = asmap; asm_p; asm_p = asm_p->next, i++) {
		desc->numStripes++;
		stripeFuncs = rf_AllocFuncList();

		if (stripeFuncsEnd == NULL) {
			stripeFuncsList = stripeFuncs;
		} else {
			stripeFuncsEnd->next = stripeFuncs;
		}
		stripeFuncsEnd = stripeFuncs;

		(raidPtr->Layout.map->SelectionFunc) (raidPtr, type, asm_p, &(stripeFuncs->fp));
		/* check to see if we found a creation func for this stripe */
		if (stripeFuncs->fp == NULL) {
			/* could not find creation function for entire stripe
			 * so, let's see if we can find one for each stripe
			 * unit in the stripe */

			/* create a failed stripe structure to attempt to deal with the failure */
			failed_stripe = rf_AllocFailedStripeStruct();
			if (failed_stripes_list == NULL) {
				failed_stripes_list = failed_stripe;
				failed_stripes_list_end = failed_stripe;
			} else {
				failed_stripes_list_end->next = failed_stripe;
				failed_stripes_list_end = failed_stripe;
			}

			/* create an array of creation funcs (called
			 * stripeFuncs) for this stripe */
			numStripeUnits = asm_p->numStripeUnitsAccessed;

			/* lookup array of stripeUnitFuncs for this stripe */
			failed_stripes_asmh_u_end = NULL;
			failed_stripes_vfple_end = NULL;
			for (j = 0, physPtr = asm_p->physInfo; physPtr; physPtr = physPtr->next, j++) {
				/* remap for series of single stripe-unit
				 * accesses */
				address = physPtr->raidAddress;
				length = physPtr->numSector;
				buffer = physPtr->bufPtr;

				asmhle = rf_AllocASMHeaderListElem();
				if (failed_stripe->asmh_u == NULL) {
					failed_stripe->asmh_u = asmhle;      /* we're the head... */
					failed_stripes_asmh_u_end = asmhle;  /* and the tail      */
				} else {
					/* tack us onto the end of the list */
					failed_stripes_asmh_u_end->next = asmhle;
					failed_stripes_asmh_u_end = asmhle;
				}


				asmhle->asmh = rf_MapAccess(raidPtr, address, length, buffer, RF_DONT_REMAP);
				asm_up = asmhle->asmh->stripeMap;

				vfple = rf_AllocVFPListElem();
				if (failed_stripe->vfple == NULL) {
					failed_stripe->vfple = vfple;
					failed_stripes_vfple_end = vfple;
				} else {
					failed_stripes_vfple_end->next = vfple;
					failed_stripes_vfple_end = vfple;
				}

				/* get the creation func for this stripe unit */
				(raidPtr->Layout.map->SelectionFunc) (raidPtr, type, asm_up, &(vfple->fn));

				/* check to see if we found a creation func
				 * for this stripe unit */

				if (vfple->fn == NULL) {
					/* could not find creation function
					 * for stripe unit so, let's see if we
					 * can find one for each block in the
					 * stripe unit */

					numBlocks = physPtr->numSector;
					numBlockDags += numBlocks;

					/* lookup array of blockFuncs for this
					 * stripe unit */
					for (k = 0; k < numBlocks; k++) {
						/* remap for series of single
						 * stripe-unit accesses */
						address = physPtr->raidAddress + k;
						length = 1;
						buffer = (char *)physPtr->bufPtr + (k * (1 << raidPtr->logBytesPerSector));

						asmhle = rf_AllocASMHeaderListElem();
						if (failed_stripe->asmh_b == NULL) {
							failed_stripe->asmh_b = asmhle;
							failed_stripes_asmh_b_end = asmhle;
						} else {
							failed_stripes_asmh_b_end->next = asmhle;
							failed_stripes_asmh_b_end = asmhle;
						}

						asmhle->asmh = rf_MapAccess(raidPtr, address, length, buffer, RF_DONT_REMAP);
						asm_bp = asmhle->asmh->stripeMap;

						vfple = rf_AllocVFPListElem();
						if (failed_stripe->bvfple == NULL) {
							failed_stripe->bvfple = vfple;
							failed_stripes_bvfple_end = vfple;
						} else {
							failed_stripes_bvfple_end->next = vfple;
							failed_stripes_bvfple_end = vfple;
						}
						(raidPtr->Layout.map->SelectionFunc) (raidPtr, type, asm_bp, &(vfple->fn));

						/* check to see if we found a
						 * creation func for this
						 * stripe unit */

						if (vfple->fn == NULL)
							cantCreateDAGs = RF_TRUE;
					}
					numStripeUnitsBailed++;
				} else {
					numUnitDags++;
				}
			}
			RF_ASSERT(j == numStripeUnits);
			numStripesBailed++;
		}
	}

	if (cantCreateDAGs) {
		/* free memory and punt */
		if (numStripesBailed > 0) {
			stripeNum = 0;
			stripeFuncs = stripeFuncsList;
			failed_stripe = failed_stripes_list;
			for (i = 0, asm_p = asmap; asm_p; asm_p = asm_p->next, i++) {
				if (stripeFuncs->fp == NULL) {

					asmhle = failed_stripe->asmh_u;
					while (asmhle) {
						tmpasmhle= asmhle;
						asmhle = tmpasmhle->next;
						rf_FreeAccessStripeMap(tmpasmhle->asmh);
						rf_FreeASMHeaderListElem(tmpasmhle);
					}

					asmhle = failed_stripe->asmh_b;
					while (asmhle) {
						tmpasmhle= asmhle;
						asmhle = tmpasmhle->next;
						rf_FreeAccessStripeMap(tmpasmhle->asmh);
						rf_FreeASMHeaderListElem(tmpasmhle);
					}

					vfple = failed_stripe->vfple;
					while (vfple) {
						tmpvfple = vfple;
						vfple = tmpvfple->next;
						rf_FreeVFPListElem(tmpvfple);
					}

					vfple = failed_stripe->bvfple;
					while (vfple) {
						tmpvfple = vfple;
						vfple = tmpvfple->next;
						rf_FreeVFPListElem(tmpvfple);
					}

					stripeNum++;
					/* only move to the next failed stripe slot if the current one was used */
					tmpfailed_stripe = failed_stripe;
					failed_stripe = failed_stripe->next;
					rf_FreeFailedStripeStruct(tmpfailed_stripe);
				}
				stripeFuncs = stripeFuncs->next;
			}
			RF_ASSERT(stripeNum == numStripesBailed);
		}
		while (stripeFuncsList != NULL) {
			temp = stripeFuncsList;
			stripeFuncsList = stripeFuncsList->next;
			rf_FreeFuncList(temp);
		}
		desc->numStripes = 0;
		return (1);
	} else {
		/* begin dag creation */
		stripeNum = 0;
		stripeUnitNum = 0;

		/* create a list of dagLists and fill them in */

		dagListend = NULL;

		stripeFuncs = stripeFuncsList;
		failed_stripe = failed_stripes_list;
		for (i = 0, asm_p = asmap; asm_p; asm_p = asm_p->next, i++) {
			/* grab dag header for this stripe */
			dag_h = NULL;

			dagList = rf_AllocDAGList();

			/* always tack the new dagList onto the end of the list... */
			if (dagListend == NULL) {
				desc->dagList = dagList;
			} else {
				dagListend->next = dagList;
			}
			dagListend = dagList;

			dagList->desc = desc;

			if (stripeFuncs->fp == NULL) {
				/* use bailout functions for this stripe */
				asmhle = failed_stripe->asmh_u;
				vfple = failed_stripe->vfple;
				/* the following two may contain asm headers and
				   block function pointers for multiple asm within
				   this access.  We initialize tmpasmhle and tmpvfple
				   here in order to allow for that, and for correct
				   operation below */
				tmpasmhle = failed_stripe->asmh_b;
				tmpvfple = failed_stripe->bvfple;
				for (j = 0, physPtr = asm_p->physInfo; physPtr; physPtr = physPtr->next, j++) {
					uFunc = vfple->fn; /* stripeUnitFuncs[stripeNum][j]; */
					if (uFunc == NULL) {
						/* use bailout functions for
						 * this stripe unit */
						for (k = 0; k < physPtr->numSector; k++) {
							/* create a dag for
							 * this block */
							InitHdrNode(&tempdag_h, raidPtr, desc);
							dagList->numDags++;
							if (dag_h == NULL) {
								dag_h = tempdag_h;
							} else {
								lastdag_h->next = tempdag_h;
							}
							lastdag_h = tempdag_h;

							bFunc = tmpvfple->fn; /* blockFuncs[stripeUnitNum][k]; */
							RF_ASSERT(bFunc);
							asm_bp = tmpasmhle->asmh->stripeMap; /* asmh_b[stripeUnitNum][k]->stripeMap; */
							(*bFunc) (raidPtr, asm_bp, tempdag_h, bp, flags, tempdag_h->allocList);

							tmpasmhle = tmpasmhle->next;
							tmpvfple = tmpvfple->next;
						}
						stripeUnitNum++;
					} else {
						/* create a dag for this unit */
						InitHdrNode(&tempdag_h, raidPtr, desc);
						dagList->numDags++;
						if (dag_h == NULL) {
							dag_h = tempdag_h;
						} else {
							lastdag_h->next = tempdag_h;
						}
						lastdag_h = tempdag_h;

						asm_up = asmhle->asmh->stripeMap; /* asmh_u[stripeNum][j]->stripeMap; */
						(*uFunc) (raidPtr, asm_up, tempdag_h, bp, flags, tempdag_h->allocList);
					}
					asmhle = asmhle->next;
					vfple = vfple->next;
				}
				RF_ASSERT(j == asm_p->numStripeUnitsAccessed);
				/* merge linked bailout dag to existing dag
				 * collection */
				stripeNum++;
				failed_stripe = failed_stripe->next;
			} else {
				/* Create a dag for this parity stripe */
				InitHdrNode(&tempdag_h, raidPtr, desc);
				dagList->numDags++;
				dag_h = tempdag_h;
				lastdag_h = tempdag_h;

				(stripeFuncs->fp) (raidPtr, asm_p, tempdag_h, bp, flags, tempdag_h->allocList);
			}
			dagList->dags = dag_h;
			stripeFuncs = stripeFuncs->next;
		}
		RF_ASSERT(i == desc->numStripes);

		/* free memory */
		if ((numStripesBailed > 0) || (numStripeUnitsBailed > 0)) {
			stripeNum = 0;
			stripeUnitNum = 0;
			/* walk through io, stripe by stripe */
			/* here we build up dag_h->asmList for this dag...
			   we need all of these asm's to do the IO, and
			   want them in a convenient place for freeing at a
			   later time */
			stripeFuncs = stripeFuncsList;
			failed_stripe = failed_stripes_list;
			dagList = desc->dagList;

			for (i = 0, asm_p = asmap; asm_p; asm_p = asm_p->next, i++) {

				dag_h = dagList->dags;
				if (dag_h->asmList) {
					endASMList = dag_h->asmList;
					while (endASMList->next)
						endASMList = endASMList->next;
				} else
					endASMList = NULL;

				if (stripeFuncs->fp == NULL) {					
					numStripeUnits = asm_p->numStripeUnitsAccessed;
					/* walk through stripe, stripe unit by
					 * stripe unit */
					asmhle = failed_stripe->asmh_u;
					vfple = failed_stripe->vfple;
					/* this contains all of the asm headers for block funcs,
					   so we have to initialize this here instead of below.*/
					tmpasmhle = failed_stripe->asmh_b;
					for (j = 0, physPtr = asm_p->physInfo; physPtr; physPtr = physPtr->next, j++) {
						if (vfple->fn == NULL) {
							numBlocks = physPtr->numSector;
							/* walk through stripe
							 * unit, block by
							 * block */
							for (k = 0; k < numBlocks; k++) {
								if (dag_h->asmList == NULL) {
									dag_h->asmList = tmpasmhle->asmh; /* asmh_b[stripeUnitNum][k];*/
									endASMList = dag_h->asmList;
								} else {
									endASMList->next = tmpasmhle->asmh;
									endASMList = endASMList->next;
								}
								tmpasmhle = tmpasmhle->next;
							}
							stripeUnitNum++;
						}
						if (dag_h->asmList == NULL) {
							dag_h->asmList = asmhle->asmh;
							endASMList = dag_h->asmList;
						} else {
							endASMList->next = asmhle->asmh;
							endASMList = endASMList->next;
						}
						asmhle = asmhle->next;
						vfple = vfple->next;
					}
					stripeNum++;
					failed_stripe = failed_stripe->next;
				}
				dagList = dagList->next; /* need to move in stride with stripeFuncs */
				stripeFuncs = stripeFuncs->next;
			}
			RF_ASSERT(stripeNum == numStripesBailed);
			RF_ASSERT(stripeUnitNum == numStripeUnitsBailed);

			failed_stripe = failed_stripes_list;
			while (failed_stripe) {

				asmhle = failed_stripe->asmh_u;
				while (asmhle) {
					tmpasmhle= asmhle;
					asmhle = tmpasmhle->next;
					rf_FreeASMHeaderListElem(tmpasmhle);
				}

				asmhle = failed_stripe->asmh_b;
				while (asmhle) {
					tmpasmhle= asmhle;
					asmhle = tmpasmhle->next;
					rf_FreeASMHeaderListElem(tmpasmhle);
				}
				vfple = failed_stripe->vfple;
				while (vfple) {
					tmpvfple = vfple;
					vfple = tmpvfple->next;
					rf_FreeVFPListElem(tmpvfple);
				}

				vfple = failed_stripe->bvfple;
				while (vfple) {
					tmpvfple = vfple;
					vfple = tmpvfple->next;
					rf_FreeVFPListElem(tmpvfple);
				}

				tmpfailed_stripe = failed_stripe;
				failed_stripe = tmpfailed_stripe->next;
				rf_FreeFailedStripeStruct(tmpfailed_stripe);
			}
		}
		while (stripeFuncsList != NULL) {
			temp = stripeFuncsList;
			stripeFuncsList = stripeFuncsList->next;
			rf_FreeFuncList(temp);
		}
		return (0);
	}
}
