/*	$NetBSD: rf_raid1.c,v 1.35 2013/09/15 12:47:26 martin Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II
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
 * rf_raid1.c -- implements RAID Level 1
 *
 *****************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_raid1.c,v 1.35 2013/09/15 12:47:26 martin Exp $");

#include "rf_raid.h"
#include "rf_raid1.h"
#include "rf_dag.h"
#include "rf_dagffrd.h"
#include "rf_dagffwr.h"
#include "rf_dagdegrd.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_diskqueue.h"
#include "rf_general.h"
#include "rf_utils.h"
#include "rf_parityscan.h"
#include "rf_mcpair.h"
#include "rf_layout.h"
#include "rf_map.h"
#include "rf_engine.h"
#include "rf_reconbuffer.h"

typedef struct RF_Raid1ConfigInfo_s {
	RF_RowCol_t **stripeIdentifier;
}       RF_Raid1ConfigInfo_t;
/* start of day code specific to RAID level 1 */
int
rf_ConfigureRAID1(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
		  RF_Config_t *cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_Raid1ConfigInfo_t *info;
	RF_RowCol_t i;

	/* create a RAID level 1 configuration structure */
	RF_MallocAndAdd(info, sizeof(RF_Raid1ConfigInfo_t), (RF_Raid1ConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;

	/* ... and fill it in. */
	info->stripeIdentifier = rf_make_2d_array(raidPtr->numCol / 2, 2, raidPtr->cleanupList);
	if (info->stripeIdentifier == NULL)
		return (ENOMEM);
	for (i = 0; i < (raidPtr->numCol / 2); i++) {
		info->stripeIdentifier[i][0] = (2 * i);
		info->stripeIdentifier[i][1] = (2 * i) + 1;
	}

	/* this implementation of RAID level 1 uses one row of numCol disks
	 * and allows multiple (numCol / 2) stripes per row.  A stripe
	 * consists of a single data unit and a single parity (mirror) unit.
	 * stripe id = raidAddr / stripeUnitSize */
	raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * (raidPtr->numCol / 2) * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk * (raidPtr->numCol / 2);
	layoutPtr->dataSectorsPerStripe = layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numDataCol = 1;
	layoutPtr->numParityCol = 1;
	return (0);
}


/* returns the physical disk location of the primary copy in the mirror pair */
void
rf_MapSectorRAID1(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
		  RF_RowCol_t *col, RF_SectorNum_t *diskSector,
		  int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	RF_RowCol_t mirrorPair = SUID % (raidPtr->numCol / 2);

	*col = 2 * mirrorPair;
	*diskSector = ((SUID / (raidPtr->numCol / 2)) * raidPtr->Layout.sectorsPerStripeUnit) + (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}


/* Map Parity
 *
 * returns the physical disk location of the secondary copy in the mirror
 * pair
 */
void
rf_MapParityRAID1(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
		  RF_RowCol_t *col, RF_SectorNum_t *diskSector,
		  int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	RF_RowCol_t mirrorPair = SUID % (raidPtr->numCol / 2);

	*col = (2 * mirrorPair) + 1;

	*diskSector = ((SUID / (raidPtr->numCol / 2)) * raidPtr->Layout.sectorsPerStripeUnit) + (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}


/* IdentifyStripeRAID1
 *
 * returns a list of disks for a given redundancy group
 */
void
rf_IdentifyStripeRAID1(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
		       RF_RowCol_t **diskids)
{
	RF_StripeNum_t stripeID = rf_RaidAddressToStripeID(&raidPtr->Layout, addr);
	RF_Raid1ConfigInfo_t *info = raidPtr->Layout.layoutSpecificInfo;
	RF_ASSERT(stripeID >= 0);
	RF_ASSERT(addr >= 0);
	*diskids = info->stripeIdentifier[stripeID % (raidPtr->numCol / 2)];
	RF_ASSERT(*diskids);
}


/* MapSIDToPSIDRAID1
 *
 * maps a logical stripe to a stripe in the redundant array
 */
void
rf_MapSIDToPSIDRAID1(RF_RaidLayout_t *layoutPtr,
		     RF_StripeNum_t stripeID,
		     RF_StripeNum_t *psID, RF_ReconUnitNum_t *which_ru)
{
	*which_ru = 0;
	*psID = stripeID;
}



/******************************************************************************
 * select a graph to perform a single-stripe access
 *
 * Parameters:  raidPtr    - description of the physical array
 *              type       - type of operation (read or write) requested
 *              asmap      - logical & physical addresses for this access
 *              createFunc - name of function to use to create the graph
 *****************************************************************************/

void
rf_RAID1DagSelect(RF_Raid_t *raidPtr, RF_IoType_t type,
		  RF_AccessStripeMap_t *asmap, RF_VoidFuncPtr *createFunc)
{
	RF_RowCol_t fcol, oc __unused;
	RF_PhysDiskAddr_t *failedPDA;
	int     prior_recon;
	RF_RowStatus_t rstat;
	RF_SectorNum_t oo __unused;


	RF_ASSERT(RF_IO_IS_R_OR_W(type));

	if (asmap->numDataFailed + asmap->numParityFailed > 1) {
#if RF_DEBUG_DAG
		if (rf_dagDebug)
			RF_ERRORMSG("Multiple disks failed in a single group!  Aborting I/O operation.\n");
#endif
		*createFunc = NULL;
		return;
	}
	if (asmap->numDataFailed + asmap->numParityFailed) {
		/*
	         * We've got a fault. Re-map to spare space, iff applicable.
	         * Shouldn't the arch-independent code do this for us?
	         * Anyway, it turns out if we don't do this here, then when
	         * we're reconstructing, writes go only to the surviving
	         * original disk, and aren't reflected on the reconstructed
	         * spare. Oops. --jimz
	         */
		failedPDA = asmap->failedPDAs[0];
		fcol = failedPDA->col;
		rstat = raidPtr->status;
		prior_recon = (rstat == rf_rs_reconfigured) || (
		    (rstat == rf_rs_reconstructing) ?
		    rf_CheckRUReconstructed(raidPtr->reconControl->reconMap, failedPDA->startSector) : 0
		    );
		if (prior_recon) {
			oc = fcol;
			oo = failedPDA->startSector;
			/*
		         * If we did distributed sparing, we'd monkey with that here.
		         * But we don't, so we'll
		         */
			failedPDA->col = raidPtr->Disks[fcol].spareCol;
			/*
		         * Redirect other components, iff necessary. This looks
		         * pretty suspicious to me, but it's what the raid5
		         * DAG select does.
		         */
			if (asmap->parityInfo->next) {
				if (failedPDA == asmap->parityInfo) {
					failedPDA->next->col = failedPDA->col;
				} else {
					if (failedPDA == asmap->parityInfo->next) {
						asmap->parityInfo->col = failedPDA->col;
					}
				}
			}
#if RF_DEBUG_DAG > 0 || RF_DEBUG_MAP > 0
			if (rf_dagDebug || rf_mapDebug) {
				printf("raid%d: Redirected type '%c' c %d o %ld -> c %d o %ld\n",
				       raidPtr->raidid, type, oc,
				       (long) oo,
				       failedPDA->col,
				       (long) failedPDA->startSector);
			}
#endif
			asmap->numDataFailed = asmap->numParityFailed = 0;
		}
	}
	if (type == RF_IO_TYPE_READ) {
		if (asmap->numDataFailed == 0)
			*createFunc = (RF_VoidFuncPtr) rf_CreateMirrorIdleReadDAG;
		else
			*createFunc = (RF_VoidFuncPtr) rf_CreateRaidOneDegradedReadDAG;
	} else {
		*createFunc = (RF_VoidFuncPtr) rf_CreateRaidOneWriteDAG;
	}
}

int
rf_VerifyParityRAID1(RF_Raid_t *raidPtr, RF_RaidAddr_t raidAddr,
		     RF_PhysDiskAddr_t *parityPDA, int correct_it,
		     RF_RaidAccessFlags_t flags)
{
	int     nbytes, bcount, stripeWidth, ret, i, j, nbad, *bbufs;
	RF_DagNode_t *blockNode, *wrBlock;
	RF_DagHeader_t *rd_dag_h, *wr_dag_h;
	RF_AccessStripeMapHeader_t *asm_h;
	RF_AllocListElem_t *allocList;
#if RF_ACC_TRACE > 0
	RF_AccTraceEntry_t tracerec;
#endif
	RF_ReconUnitNum_t which_ru;
	RF_RaidLayout_t *layoutPtr;
	RF_AccessStripeMap_t *aasm;
	RF_SectorCount_t nsector;
	RF_RaidAddr_t startAddr;
	char   *bf, *buf1, *buf2;
	RF_PhysDiskAddr_t *pda;
	RF_StripeNum_t psID;
	RF_MCPair_t *mcpair;

	layoutPtr = &raidPtr->Layout;
	startAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, raidAddr);
	nsector = parityPDA->numSector;
	nbytes = rf_RaidAddressToByte(raidPtr, nsector);
	psID = rf_RaidAddressToParityStripeID(layoutPtr, raidAddr, &which_ru);

	asm_h = NULL;
	rd_dag_h = wr_dag_h = NULL;
	mcpair = NULL;

	ret = RF_PARITY_COULD_NOT_VERIFY;

	rf_MakeAllocList(allocList);
	if (allocList == NULL)
		return (RF_PARITY_COULD_NOT_VERIFY);
	mcpair = rf_AllocMCPair();
	if (mcpair == NULL)
		goto done;
	RF_ASSERT(layoutPtr->numDataCol == layoutPtr->numParityCol);
	stripeWidth = layoutPtr->numDataCol + layoutPtr->numParityCol;
	bcount = nbytes * (layoutPtr->numDataCol + layoutPtr->numParityCol);
	RF_MallocAndAdd(bf, bcount, (char *), allocList);
	if (bf == NULL)
		goto done;
#if RF_DEBUG_VERIFYPARITY
	if (rf_verifyParityDebug) {
		printf("raid%d: RAID1 parity verify: buf=%lx bcount=%d (%lx - %lx)\n",
		       raidPtr->raidid, (long) bf, bcount, (long) bf,
		       (long) bf + bcount);
	}
#endif
	/*
         * Generate a DAG which will read the entire stripe- then we can
         * just compare data chunks versus "parity" chunks.
         */

	rd_dag_h = rf_MakeSimpleDAG(raidPtr, stripeWidth, nbytes, bf,
	    rf_DiskReadFunc, rf_DiskReadUndoFunc, "Rod", allocList, flags,
	    RF_IO_NORMAL_PRIORITY);
	if (rd_dag_h == NULL)
		goto done;
	blockNode = rd_dag_h->succedents[0];

	/*
         * Map the access to physical disk addresses (PDAs)- this will
         * get us both a list of data addresses, and "parity" addresses
         * (which are really mirror copies).
         */
	asm_h = rf_MapAccess(raidPtr, startAddr, layoutPtr->dataSectorsPerStripe,
	    bf, RF_DONT_REMAP);
	aasm = asm_h->stripeMap;

	buf1 = bf;
	/*
         * Loop through the data blocks, setting up read nodes for each.
         */
	for (pda = aasm->physInfo, i = 0; i < layoutPtr->numDataCol; i++, pda = pda->next) {
		RF_ASSERT(pda);

		rf_RangeRestrictPDA(raidPtr, parityPDA, pda, 0, 1);

		RF_ASSERT(pda->numSector != 0);
		if (rf_TryToRedirectPDA(raidPtr, pda, 0)) {
			/* cannot verify parity with dead disk */
			goto done;
		}
		pda->bufPtr = buf1;
		blockNode->succedents[i]->params[0].p = pda;
		blockNode->succedents[i]->params[1].p = buf1;
		blockNode->succedents[i]->params[2].v = psID;
		blockNode->succedents[i]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, which_ru);
		buf1 += nbytes;
	}
	RF_ASSERT(pda == NULL);
	/*
         * keep i, buf1 running
         *
         * Loop through parity blocks, setting up read nodes for each.
         */
	for (pda = aasm->parityInfo; i < layoutPtr->numDataCol + layoutPtr->numParityCol; i++, pda = pda->next) {
		RF_ASSERT(pda);
		rf_RangeRestrictPDA(raidPtr, parityPDA, pda, 0, 1);
		RF_ASSERT(pda->numSector != 0);
		if (rf_TryToRedirectPDA(raidPtr, pda, 0)) {
			/* cannot verify parity with dead disk */
			goto done;
		}
		pda->bufPtr = buf1;
		blockNode->succedents[i]->params[0].p = pda;
		blockNode->succedents[i]->params[1].p = buf1;
		blockNode->succedents[i]->params[2].v = psID;
		blockNode->succedents[i]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, which_ru);
		buf1 += nbytes;
	}
	RF_ASSERT(pda == NULL);

#if RF_ACC_TRACE > 0
	memset((char *) &tracerec, 0, sizeof(tracerec));
	rd_dag_h->tracerec = &tracerec;
#endif
#if 0
	if (rf_verifyParityDebug > 1) {
		printf("raid%d: RAID1 parity verify read dag:\n",
		       raidPtr->raidid);
		rf_PrintDAGList(rd_dag_h);
	}
#endif
	RF_LOCK_MCPAIR(mcpair);
	mcpair->flag = 0;
	RF_UNLOCK_MCPAIR(mcpair);

	rf_DispatchDAG(rd_dag_h, (void (*) (void *)) rf_MCPairWakeupFunc,
	    (void *) mcpair);

	RF_LOCK_MCPAIR(mcpair);
	while (mcpair->flag == 0) {
		RF_WAIT_MCPAIR(mcpair);
	}
	RF_UNLOCK_MCPAIR(mcpair);

	if (rd_dag_h->status != rf_enable) {
		RF_ERRORMSG("Unable to verify raid1 parity: can't read stripe\n");
		ret = RF_PARITY_COULD_NOT_VERIFY;
		goto done;
	}
	/*
         * buf1 is the beginning of the data blocks chunk
         * buf2 is the beginning of the parity blocks chunk
         */
	buf1 = bf;
	buf2 = bf + (nbytes * layoutPtr->numDataCol);
	ret = RF_PARITY_OKAY;
	/*
         * bbufs is "bad bufs"- an array whose entries are the data
         * column numbers where we had miscompares. (That is, column 0
         * and column 1 of the array are mirror copies, and are considered
         * "data column 0" for this purpose).
         */
	RF_MallocAndAdd(bbufs, layoutPtr->numParityCol * sizeof(int), (int *),
	    allocList);
	nbad = 0;
	/*
         * Check data vs "parity" (mirror copy).
         */
	for (i = 0; i < layoutPtr->numDataCol; i++) {
#if RF_DEBUG_VERIFYPARITY
		if (rf_verifyParityDebug) {
			printf("raid%d: RAID1 parity verify %d bytes: i=%d buf1=%lx buf2=%lx buf=%lx\n",
			       raidPtr->raidid, nbytes, i, (long) buf1,
			       (long) buf2, (long) bf);
		}
#endif
		ret = memcmp(buf1, buf2, nbytes);
		if (ret) {
#if RF_DEBUG_VERIFYPARITY
			if (rf_verifyParityDebug > 1) {
				for (j = 0; j < nbytes; j++) {
					if (buf1[j] != buf2[j])
						break;
				}
				printf("psid=%ld j=%d\n", (long) psID, j);
				printf("buf1 %02x %02x %02x %02x %02x\n", buf1[0] & 0xff,
				    buf1[1] & 0xff, buf1[2] & 0xff, buf1[3] & 0xff, buf1[4] & 0xff);
				printf("buf2 %02x %02x %02x %02x %02x\n", buf2[0] & 0xff,
				    buf2[1] & 0xff, buf2[2] & 0xff, buf2[3] & 0xff, buf2[4] & 0xff);
			}
			if (rf_verifyParityDebug) {
				printf("raid%d: RAID1: found bad parity, i=%d\n", raidPtr->raidid, i);
			}
#endif
			/*
		         * Parity is bad. Keep track of which columns were bad.
		         */
			if (bbufs)
				bbufs[nbad] = i;
			nbad++;
			ret = RF_PARITY_BAD;
		}
		buf1 += nbytes;
		buf2 += nbytes;
	}

	if ((ret != RF_PARITY_OKAY) && correct_it) {
		ret = RF_PARITY_COULD_NOT_CORRECT;
#if RF_DEBUG_VERIFYPARITY
		if (rf_verifyParityDebug) {
			printf("raid%d: RAID1 parity verify: parity not correct\n", raidPtr->raidid);
		}
#endif
		if (bbufs == NULL)
			goto done;
		/*
	         * Make a DAG with one write node for each bad unit. We'll simply
	         * write the contents of the data unit onto the parity unit for
	         * correction. (It's possible that the mirror copy was the correct
	         * copy, and that we're spooging good data by writing bad over it,
	         * but there's no way we can know that.
	         */
		wr_dag_h = rf_MakeSimpleDAG(raidPtr, nbad, nbytes, bf,
		    rf_DiskWriteFunc, rf_DiskWriteUndoFunc, "Wnp", allocList, flags,
		    RF_IO_NORMAL_PRIORITY);
		if (wr_dag_h == NULL)
			goto done;
		wrBlock = wr_dag_h->succedents[0];
		/*
	         * Fill in a write node for each bad compare.
	         */
		for (i = 0; i < nbad; i++) {
			j = i + layoutPtr->numDataCol;
			pda = blockNode->succedents[j]->params[0].p;
			pda->bufPtr = blockNode->succedents[i]->params[1].p;
			wrBlock->succedents[i]->params[0].p = pda;
			wrBlock->succedents[i]->params[1].p = pda->bufPtr;
			wrBlock->succedents[i]->params[2].v = psID;
			wrBlock->succedents[i]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, which_ru);
		}
#if RF_ACC_TRACE > 0
		memset((char *) &tracerec, 0, sizeof(tracerec));
		wr_dag_h->tracerec = &tracerec;
#endif
#if 0
		if (rf_verifyParityDebug > 1) {
			printf("Parity verify write dag:\n");
			rf_PrintDAGList(wr_dag_h);
		}
#endif
		RF_LOCK_MCPAIR(mcpair);
		mcpair->flag = 0;
		RF_UNLOCK_MCPAIR(mcpair);

		/* fire off the write DAG */
		rf_DispatchDAG(wr_dag_h, (void (*) (void *)) rf_MCPairWakeupFunc,
		    (void *) mcpair);

		RF_LOCK_MCPAIR(mcpair);
		while (!mcpair->flag) {
			RF_WAIT_MCPAIR(mcpair);
		}
		RF_UNLOCK_MCPAIR(mcpair);
		if (wr_dag_h->status != rf_enable) {
			RF_ERRORMSG("Unable to correct RAID1 parity in VerifyParity\n");
			goto done;
		}
		ret = RF_PARITY_CORRECTED;
	}
done:
	/*
         * All done. We might've gotten here without doing part of the function,
         * so cleanup what we have to and return our running status.
         */
	if (asm_h)
		rf_FreeAccessStripeMap(asm_h);
	if (rd_dag_h)
		rf_FreeDAG(rd_dag_h);
	if (wr_dag_h)
		rf_FreeDAG(wr_dag_h);
	if (mcpair)
		rf_FreeMCPair(mcpair);
	rf_FreeAllocList(allocList);
#if RF_DEBUG_VERIFYPARITY
	if (rf_verifyParityDebug) {
		printf("raid%d: RAID1 parity verify, returning %d\n",
		       raidPtr->raidid, ret);
	}
#endif
	return (ret);
}

/* rbuf          - the recon buffer to submit
 * keep_it       - whether we can keep this buffer or we have to return it
 * use_committed - whether to use a committed or an available recon buffer
 */

int
rf_SubmitReconBufferRAID1(RF_ReconBuffer_t *rbuf, int keep_it,
			  int use_committed)
{
	RF_ReconParityStripeStatus_t *pssPtr;
	RF_ReconCtrl_t *reconCtrlPtr;
	int     retcode;
	RF_CallbackDesc_t *cb, *p;
	RF_ReconBuffer_t *t;
	RF_Raid_t *raidPtr;
	void *ta;

	retcode = 0;

	raidPtr = rbuf->raidPtr;
	reconCtrlPtr = raidPtr->reconControl;

	RF_ASSERT(rbuf);
	RF_ASSERT(rbuf->col != reconCtrlPtr->fcol);

#if RF_DEBUG_RECON
	if (rf_reconbufferDebug) {
		printf("raid%d: RAID1 reconbuffer submission c%d psid %ld ru%d (failed offset %ld)\n",
		       raidPtr->raidid, rbuf->col,
		       (long) rbuf->parityStripeID, rbuf->which_ru,
		       (long) rbuf->failedDiskSectorOffset);
	}
#endif
	if (rf_reconDebug) {
		unsigned char *b = rbuf->buffer;
		printf("RAID1 reconbuffer submit psid %ld buf %lx\n",
		    (long) rbuf->parityStripeID, (long) rbuf->buffer);
		printf("RAID1 psid %ld   %02x %02x %02x %02x %02x\n",
		    (long)rbuf->parityStripeID, b[0], b[1], b[2], b[3], b[4]);
	}
	RF_LOCK_PSS_MUTEX(raidPtr, rbuf->parityStripeID);

	rf_lock_mutex2(reconCtrlPtr->rb_mutex);
	while(reconCtrlPtr->rb_lock) {
		rf_wait_cond2(reconCtrlPtr->rb_cv, reconCtrlPtr->rb_mutex);
	}
	reconCtrlPtr->rb_lock = 1;
	rf_unlock_mutex2(reconCtrlPtr->rb_mutex);

	pssPtr = rf_LookupRUStatus(raidPtr, reconCtrlPtr->pssTable,
	    rbuf->parityStripeID, rbuf->which_ru, RF_PSS_NONE, NULL);
	RF_ASSERT(pssPtr);	/* if it didn't exist, we wouldn't have gotten
				 * an rbuf for it */

	/*
         * Since this is simple mirroring, the first submission for a stripe is also
         * treated as the last.
         */

	t = NULL;
	if (keep_it) {
#if RF_DEBUG_RECON
		if (rf_reconbufferDebug) {
			printf("raid%d: RAID1 rbuf submission: keeping rbuf\n",
			       raidPtr->raidid);
		}
#endif
		t = rbuf;
	} else {
		if (use_committed) {
#if RF_DEBUG_RECON
			if (rf_reconbufferDebug) {
				printf("raid%d: RAID1 rbuf submission: using committed rbuf\n", raidPtr->raidid);
			}
#endif
			t = reconCtrlPtr->committedRbufs;
			RF_ASSERT(t);
			reconCtrlPtr->committedRbufs = t->next;
			t->next = NULL;
		} else
			if (reconCtrlPtr->floatingRbufs) {
#if RF_DEBUG_RECON
				if (rf_reconbufferDebug) {
					printf("raid%d: RAID1 rbuf submission: using floating rbuf\n", raidPtr->raidid);
				}
#endif
				t = reconCtrlPtr->floatingRbufs;
				reconCtrlPtr->floatingRbufs = t->next;
				t->next = NULL;
			}
	}
	if (t == NULL) {
#if RF_DEBUG_RECON
		if (rf_reconbufferDebug) {
			printf("raid%d: RAID1 rbuf submission: waiting for rbuf\n", raidPtr->raidid);
		}
#endif
		RF_ASSERT((keep_it == 0) && (use_committed == 0));
		raidPtr->procsInBufWait++;
		if ((raidPtr->procsInBufWait == (raidPtr->numCol - 1))
		    && (raidPtr->numFullReconBuffers == 0)) {
			/* ruh-ro */
			RF_ERRORMSG("Buffer wait deadlock\n");
			rf_PrintPSStatusTable(raidPtr);
			RF_PANIC();
		}
		pssPtr->flags |= RF_PSS_BUFFERWAIT;
		cb = rf_AllocCallbackDesc();
		cb->col = rbuf->col;
		cb->callbackArg.v = rbuf->parityStripeID;
		cb->next = NULL;
		if (reconCtrlPtr->bufferWaitList == NULL) {
			/* we are the wait list- lucky us */
			reconCtrlPtr->bufferWaitList = cb;
		} else {
			/* append to wait list */
			for (p = reconCtrlPtr->bufferWaitList; p->next; p = p->next);
			p->next = cb;
		}
		retcode = 1;
		goto out;
	}
	if (t != rbuf) {
		t->col = reconCtrlPtr->fcol;
		t->parityStripeID = rbuf->parityStripeID;
		t->which_ru = rbuf->which_ru;
		t->failedDiskSectorOffset = rbuf->failedDiskSectorOffset;
		t->spCol = rbuf->spCol;
		t->spOffset = rbuf->spOffset;
		/* Swap buffers. DANCE! */
		ta = t->buffer;
		t->buffer = rbuf->buffer;
		rbuf->buffer = ta;
	}
	/*
         * Use the rbuf we've been given as the target.
         */
	RF_ASSERT(pssPtr->rbuf == NULL);
	pssPtr->rbuf = t;

	t->count = 1;
	/*
         * Below, we use 1 for numDataCol (which is equal to the count in the
         * previous line), so we'll always be done.
         */
	rf_CheckForFullRbuf(raidPtr, reconCtrlPtr, pssPtr, 1);

out:
	RF_UNLOCK_PSS_MUTEX(raidPtr, rbuf->parityStripeID);
	rf_lock_mutex2(reconCtrlPtr->rb_mutex);
	reconCtrlPtr->rb_lock = 0;
	rf_broadcast_cond2(reconCtrlPtr->rb_cv);
	rf_unlock_mutex2(reconCtrlPtr->rb_mutex);
#if RF_DEBUG_RECON
	if (rf_reconbufferDebug) {
		printf("raid%d: RAID1 rbuf submission: returning %d\n",
		       raidPtr->raidid, retcode);
	}
#endif
	return (retcode);
}

RF_HeadSepLimit_t
rf_GetDefaultHeadSepLimitRAID1(RF_Raid_t *raidPtr)
{
	return (10);
}

