/*	$NetBSD: rf_dagffwr.c,v 1.34 2013/09/15 12:41:17 martin Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Daniel Stodolsky, William V. Courtright II
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
 * rf_dagff.c
 *
 * code for creating fault-free DAGs
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_dagffwr.c,v 1.34 2013/09/15 12:41:17 martin Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_debugMem.h"
#include "rf_dagffrd.h"
#include "rf_general.h"
#include "rf_dagffwr.h"
#include "rf_map.h"

/******************************************************************************
 *
 * General comments on DAG creation:
 *
 * All DAGs in this file use roll-away error recovery.  Each DAG has a single
 * commit node, usually called "Cmt."  If an error occurs before the Cmt node
 * is reached, the execution engine will halt forward execution and work
 * backward through the graph, executing the undo functions.  Assuming that
 * each node in the graph prior to the Cmt node are undoable and atomic - or -
 * does not make changes to permanent state, the graph will fail atomically.
 * If an error occurs after the Cmt node executes, the engine will roll-forward
 * through the graph, blindly executing nodes until it reaches the end.
 * If a graph reaches the end, it is assumed to have completed successfully.
 *
 * A graph has only 1 Cmt node.
 *
 */


/******************************************************************************
 *
 * The following wrappers map the standard DAG creation interface to the
 * DAG creation routines.  Additionally, these wrappers enable experimentation
 * with new DAG structures by providing an extra level of indirection, allowing
 * the DAG creation routines to be replaced at this single point.
 */


void
rf_CreateNonRedundantWriteDAG(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
			      RF_DagHeader_t *dag_h, void *bp,
			      RF_RaidAccessFlags_t flags,
			      RF_AllocListElem_t *allocList,
			      RF_IoType_t type)
{
	rf_CreateNonredundantDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
				 RF_IO_TYPE_WRITE);
}

void
rf_CreateRAID0WriteDAG(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
		       RF_DagHeader_t *dag_h, void *bp,
		       RF_RaidAccessFlags_t flags,
		       RF_AllocListElem_t *allocList,
		       RF_IoType_t type)
{
	rf_CreateNonredundantDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
				 RF_IO_TYPE_WRITE);
}

void
rf_CreateSmallWriteDAG(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
		       RF_DagHeader_t *dag_h, void *bp,
		       RF_RaidAccessFlags_t flags,
		       RF_AllocListElem_t *allocList)
{
	/* "normal" rollaway */
	rf_CommonCreateSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags,
				     allocList, &rf_xorFuncs, NULL);
}

void
rf_CreateLargeWriteDAG(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
		       RF_DagHeader_t *dag_h, void *bp,
		       RF_RaidAccessFlags_t flags,
		       RF_AllocListElem_t *allocList)
{
	/* "normal" rollaway */
	rf_CommonCreateLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags,
				     allocList, 1, rf_RegularXorFunc, RF_TRUE);
}


/******************************************************************************
 *
 * DAG creation code begins here
 */


/******************************************************************************
 *
 * creates a DAG to perform a large-write operation:
 *
 *           / Rod \           / Wnd \
 * H -- block- Rod - Xor - Cmt - Wnd --- T
 *           \ Rod /          \  Wnp /
 *                             \[Wnq]/
 *
 * The XOR node also does the Q calculation in the P+Q architecture.
 * All nodes are before the commit node (Cmt) are assumed to be atomic and
 * undoable - or - they make no changes to permanent state.
 *
 * Rod = read old data
 * Cmt = commit node
 * Wnp = write new parity
 * Wnd = write new data
 * Wnq = write new "q"
 * [] denotes optional segments in the graph
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (holds write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *              nfaults   - number of faults array can tolerate
 *                          (equal to # redundancy units in stripe)
 *              redfuncs  - list of redundancy generating functions
 *
 *****************************************************************************/

void
rf_CommonCreateLargeWriteDAG(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
			     RF_DagHeader_t *dag_h, void *bp,
			     RF_RaidAccessFlags_t flags,
			     RF_AllocListElem_t *allocList,
			     int nfaults, int (*redFunc) (RF_DagNode_t *),
			     int allowBufferRecycle)
{
	RF_DagNode_t *wndNodes, *rodNodes, *xorNode, *wnpNode, *tmpNode;
	RF_DagNode_t *blockNode, *commitNode, *termNode;
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	RF_DagNode_t *wnqNode;
#endif
	int     nWndNodes, nRodNodes, i, nodeNum, asmNum;
	RF_AccessStripeMapHeader_t *new_asm_h[2];
	RF_StripeNum_t parityStripeID;
	char   *sosBuffer, *eosBuffer;
	RF_ReconUnitNum_t which_ru;
	RF_RaidLayout_t *layoutPtr;
	RF_PhysDiskAddr_t *pda;

	layoutPtr = &(raidPtr->Layout);
	parityStripeID = rf_RaidAddressToParityStripeID(layoutPtr,
							asmap->raidAddress,
							&which_ru);

#if RF_DEBUG_DAG
	if (rf_dagDebug) {
		printf("[Creating large-write DAG]\n");
	}
#endif
	dag_h->creator = "LargeWriteDAG";

	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* alloc the nodes: Wnd, xor, commit, block, term, and  Wnp */
	nWndNodes = asmap->numStripeUnitsAccessed;

	for (i = 0; i < nWndNodes; i++) {
		tmpNode = rf_AllocDAGNode();
		tmpNode->list_next = dag_h->nodes;
		dag_h->nodes = tmpNode;
	}
	wndNodes = dag_h->nodes;

	xorNode = rf_AllocDAGNode();
	xorNode->list_next = dag_h->nodes;
	dag_h->nodes = xorNode;

	wnpNode = rf_AllocDAGNode();
	wnpNode->list_next = dag_h->nodes;
	dag_h->nodes = wnpNode;

	blockNode = rf_AllocDAGNode();
	blockNode->list_next = dag_h->nodes;
	dag_h->nodes = blockNode;

	commitNode = rf_AllocDAGNode();
	commitNode->list_next = dag_h->nodes;
	dag_h->nodes = commitNode;

	termNode = rf_AllocDAGNode();
	termNode->list_next = dag_h->nodes;
	dag_h->nodes = termNode;

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	if (nfaults == 2) {
		wnqNode = rf_AllocDAGNode();
	} else {
		wnqNode = NULL;
	}
#endif
	rf_MapUnaccessedPortionOfStripe(raidPtr, layoutPtr, asmap, dag_h,
					new_asm_h, &nRodNodes, &sosBuffer,
					&eosBuffer, allocList);
	if (nRodNodes > 0) {
		for (i = 0; i < nRodNodes; i++) {
			tmpNode = rf_AllocDAGNode();
			tmpNode->list_next = dag_h->nodes;
			dag_h->nodes = tmpNode;
		}
		rodNodes = dag_h->nodes;
	} else {
		rodNodes = NULL;
	}

	/* begin node initialization */
	if (nRodNodes > 0) {
		rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc,
			    rf_NullNodeUndoFunc, NULL, nRodNodes, 0, 0, 0,
			    dag_h, "Nil", allocList);
	} else {
		rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc,
			    rf_NullNodeUndoFunc, NULL, 1, 0, 0, 0,
			    dag_h, "Nil", allocList);
	}

	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc,
		    rf_NullNodeUndoFunc, NULL, nWndNodes + nfaults, 1, 0, 0,
		    dag_h, "Cmt", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc,
		    rf_TerminateUndoFunc, NULL, 0, nWndNodes + nfaults, 0, 0,
		    dag_h, "Trm", allocList);

	/* initialize the Rod nodes */
	tmpNode = rodNodes;
	for (nodeNum = asmNum = 0; asmNum < 2; asmNum++) {
		if (new_asm_h[asmNum]) {
			pda = new_asm_h[asmNum]->stripeMap->physInfo;
			while (pda) {
				rf_InitNode(tmpNode, rf_wait,
					    RF_FALSE, rf_DiskReadFunc,
					    rf_DiskReadUndoFunc,
					    rf_GenericWakeupFunc,
					    1, 1, 4, 0, dag_h,
					    "Rod", allocList);
				tmpNode->params[0].p = pda;
				tmpNode->params[1].p = pda->bufPtr;
				tmpNode->params[2].v = parityStripeID;
				tmpNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
				    which_ru);
				nodeNum++;
				pda = pda->next;
				tmpNode = tmpNode->list_next;
			}
		}
	}
	RF_ASSERT(nodeNum == nRodNodes);

	/* initialize the wnd nodes */
	pda = asmap->physInfo;
	tmpNode = wndNodes;
	for (i = 0; i < nWndNodes; i++) {
		rf_InitNode(tmpNode, rf_wait, RF_FALSE,
			    rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			    rf_GenericWakeupFunc, 1, 1, 4, 0,
			    dag_h, "Wnd", allocList);
		RF_ASSERT(pda != NULL);
		tmpNode->params[0].p = pda;
		tmpNode->params[1].p = pda->bufPtr;
		tmpNode->params[2].v = parityStripeID;
		tmpNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, which_ru);
		pda = pda->next;
		tmpNode = tmpNode->list_next;
	}

	/* initialize the redundancy node */
	if (nRodNodes > 0) {
		rf_InitNode(xorNode, rf_wait, RF_FALSE, redFunc,
			    rf_NullNodeUndoFunc, NULL, 1,
			    nRodNodes, 2 * (nWndNodes + nRodNodes) + 1,
			    nfaults, dag_h, "Xr ", allocList);
	} else {
		rf_InitNode(xorNode, rf_wait, RF_FALSE, redFunc,
			    rf_NullNodeUndoFunc, NULL, 1,
			    1, 2 * (nWndNodes + nRodNodes) + 1,
			    nfaults, dag_h, "Xr ", allocList);
	}
	xorNode->flags |= RF_DAGNODE_FLAG_YIELD;
	tmpNode = wndNodes;
	for (i = 0; i < nWndNodes; i++) {
		/* pda */
		xorNode->params[2 * i + 0] = tmpNode->params[0];
		/* buf ptr */
		xorNode->params[2 * i + 1] = tmpNode->params[1];
		tmpNode = tmpNode->list_next;
	}
	tmpNode = rodNodes;
	for (i = 0; i < nRodNodes; i++) {
		/* pda */
		xorNode->params[2 * (nWndNodes + i) + 0] = tmpNode->params[0];
		/* buf ptr */
		xorNode->params[2 * (nWndNodes + i) + 1] = tmpNode->params[1];
		tmpNode = tmpNode->list_next;
	}
	/* xor node needs to get at RAID information */
	xorNode->params[2 * (nWndNodes + nRodNodes)].p = raidPtr;

	/*
         * Look for an Rod node that reads a complete SU. If none,
         * alloc a buffer to receive the parity info. Note that we
         * can't use a new data buffer because it will not have gotten
         * written when the xor occurs.  */
	if (allowBufferRecycle) {
		tmpNode = rodNodes;
		for (i = 0; i < nRodNodes; i++) {
			if (((RF_PhysDiskAddr_t *) tmpNode->params[0].p)->numSector == raidPtr->Layout.sectorsPerStripeUnit)
				break;
			tmpNode = tmpNode->list_next;
		}
	}
	if ((!allowBufferRecycle) || (i == nRodNodes)) {
		xorNode->results[0] = rf_AllocBuffer(raidPtr, dag_h, rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit));
	} else {
		/* this works because the only way we get here is if
		   allowBufferRecycle is true and we went through the
		   above for loop, and exited via the break before
		   i==nRodNodes was true.  That means tmpNode will
		   still point to a valid node -- the one we want for
		   here! */
		xorNode->results[0] = tmpNode->params[1].p;
	}

	/* initialize the Wnp node */
	rf_InitNode(wnpNode, rf_wait, RF_FALSE, rf_DiskWriteFunc,
		    rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0,
		    dag_h, "Wnp", allocList);
	wnpNode->params[0].p = asmap->parityInfo;
	wnpNode->params[1].p = xorNode->results[0];
	wnpNode->params[2].v = parityStripeID;
	wnpNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, which_ru);
	/* parityInfo must describe entire parity unit */
	RF_ASSERT(asmap->parityInfo->next == NULL);

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	if (nfaults == 2) {
		/*
	         * We never try to recycle a buffer for the Q calcuation
	         * in addition to the parity. This would cause two buffers
	         * to get smashed during the P and Q calculation, guaranteeing
	         * one would be wrong.
	         */
		RF_MallocAndAdd(xorNode->results[1],
				rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit),
				(void *), allocList);
		rf_InitNode(wnqNode, rf_wait, RF_FALSE, rf_DiskWriteFunc,
			    rf_DiskWriteUndoFunc, rf_GenericWakeupFunc,
			    1, 1, 4, 0, dag_h, "Wnq", allocList);
		wnqNode->params[0].p = asmap->qInfo;
		wnqNode->params[1].p = xorNode->results[1];
		wnqNode->params[2].v = parityStripeID;
		wnqNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, which_ru);
		/* parityInfo must describe entire parity unit */
		RF_ASSERT(asmap->parityInfo->next == NULL);
	}
#endif
	/*
         * Connect nodes to form graph.
         */

	/* connect dag header to block node */
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	if (nRodNodes > 0) {
		/* connect the block node to the Rod nodes */
		RF_ASSERT(blockNode->numSuccedents == nRodNodes);
		RF_ASSERT(xorNode->numAntecedents == nRodNodes);
		tmpNode = rodNodes;
		for (i = 0; i < nRodNodes; i++) {
			RF_ASSERT(tmpNode->numAntecedents == 1);
			blockNode->succedents[i] = tmpNode;
			tmpNode->antecedents[0] = blockNode;
			tmpNode->antType[0] = rf_control;

			/* connect the Rod nodes to the Xor node */
			RF_ASSERT(tmpNode->numSuccedents == 1);
			tmpNode->succedents[0] = xorNode;
			xorNode->antecedents[i] = tmpNode;
			xorNode->antType[i] = rf_trueData;
			tmpNode = tmpNode->list_next;
		}
	} else {
		/* connect the block node to the Xor node */
		RF_ASSERT(blockNode->numSuccedents == 1);
		RF_ASSERT(xorNode->numAntecedents == 1);
		blockNode->succedents[0] = xorNode;
		xorNode->antecedents[0] = blockNode;
		xorNode->antType[0] = rf_control;
	}

	/* connect the xor node to the commit node */
	RF_ASSERT(xorNode->numSuccedents == 1);
	RF_ASSERT(commitNode->numAntecedents == 1);
	xorNode->succedents[0] = commitNode;
	commitNode->antecedents[0] = xorNode;
	commitNode->antType[0] = rf_control;

	/* connect the commit node to the write nodes */
	RF_ASSERT(commitNode->numSuccedents == nWndNodes + nfaults);
	tmpNode = wndNodes;
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes->numAntecedents == 1);
		commitNode->succedents[i] = tmpNode;
		tmpNode->antecedents[0] = commitNode;
		tmpNode->antType[0] = rf_control;
		tmpNode = tmpNode->list_next;
	}
	RF_ASSERT(wnpNode->numAntecedents == 1);
	commitNode->succedents[nWndNodes] = wnpNode;
	wnpNode->antecedents[0] = commitNode;
	wnpNode->antType[0] = rf_trueData;
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	if (nfaults == 2) {
		RF_ASSERT(wnqNode->numAntecedents == 1);
		commitNode->succedents[nWndNodes + 1] = wnqNode;
		wnqNode->antecedents[0] = commitNode;
		wnqNode->antType[0] = rf_trueData;
	}
#endif
	/* connect the write nodes to the term node */
	RF_ASSERT(termNode->numAntecedents == nWndNodes + nfaults);
	RF_ASSERT(termNode->numSuccedents == 0);
	tmpNode = wndNodes;
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes->numSuccedents == 1);
		tmpNode->succedents[0] = termNode;
		termNode->antecedents[i] = tmpNode;
		termNode->antType[i] = rf_control;
		tmpNode = tmpNode->list_next;
	}
	RF_ASSERT(wnpNode->numSuccedents == 1);
	wnpNode->succedents[0] = termNode;
	termNode->antecedents[nWndNodes] = wnpNode;
	termNode->antType[nWndNodes] = rf_control;
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	if (nfaults == 2) {
		RF_ASSERT(wnqNode->numSuccedents == 1);
		wnqNode->succedents[0] = termNode;
		termNode->antecedents[nWndNodes + 1] = wnqNode;
		termNode->antType[nWndNodes + 1] = rf_control;
	}
#endif
}
/******************************************************************************
 *
 * creates a DAG to perform a small-write operation (either raid 5 or pq),
 * which is as follows:
 *
 * Hdr -> Nil -> Rop -> Xor -> Cmt ----> Wnp [Unp] --> Trm
 *            \- Rod X      /     \----> Wnd [Und]-/
 *           [\- Rod X     /       \---> Wnd [Und]-/]
 *           [\- Roq -> Q /         \--> Wnq [Unq]-/]
 *
 * Rop = read old parity
 * Rod = read old data
 * Roq = read old "q"
 * Cmt = commit node
 * Und = unlock data disk
 * Unp = unlock parity disk
 * Unq = unlock q disk
 * Wnp = write new parity
 * Wnd = write new data
 * Wnq = write new "q"
 * [ ] denotes optional segments in the graph
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (holds write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *              pfuncs    - list of parity generating functions
 *              qfuncs    - list of q generating functions
 *
 * A null qfuncs indicates single fault tolerant
 *****************************************************************************/

void
rf_CommonCreateSmallWriteDAG(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
			     RF_DagHeader_t *dag_h, void *bp,
			     RF_RaidAccessFlags_t flags,
			     RF_AllocListElem_t *allocList,
			     const RF_RedFuncs_t *pfuncs,
			     const RF_RedFuncs_t *qfuncs)
{
	RF_DagNode_t *readDataNodes, *readParityNodes, *termNode;
	RF_DagNode_t *tmpNode, *tmpreadDataNode, *tmpreadParityNode;
	RF_DagNode_t *xorNodes, *blockNode, *commitNode;
	RF_DagNode_t *writeDataNodes, *writeParityNodes;
	RF_DagNode_t *tmpxorNode, *tmpwriteDataNode;
	RF_DagNode_t *tmpwriteParityNode;
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	RF_DagNode_t *tmpwriteQNode, *tmpreadQNode, *tmpqNode, *readQNodes,
	     *writeQNodes, *qNodes;
#endif
	int     i, j, nNodes;
	RF_ReconUnitNum_t which_ru;
	int     (*func) (RF_DagNode_t *), (*undoFunc) (RF_DagNode_t *);
	int     (*qfunc) (RF_DagNode_t *) __unused;
	int     numDataNodes, numParityNodes;
	RF_StripeNum_t parityStripeID;
	RF_PhysDiskAddr_t *pda;
	const char *name, *qname __unused;
	long    nfaults;

	nfaults = qfuncs ? 2 : 1;

	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout),
	    asmap->raidAddress, &which_ru);
	pda = asmap->physInfo;
	numDataNodes = asmap->numStripeUnitsAccessed;
	numParityNodes = (asmap->parityInfo->next) ? 2 : 1;

#if RF_DEBUG_DAG
	if (rf_dagDebug) {
		printf("[Creating small-write DAG]\n");
	}
#endif
	RF_ASSERT(numDataNodes > 0);
	dag_h->creator = "SmallWriteDAG";

	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/*
         * DAG creation occurs in four steps:
         * 1. count the number of nodes in the DAG
         * 2. create the nodes
         * 3. initialize the nodes
         * 4. connect the nodes
         */

	/*
         * Step 1. compute number of nodes in the graph
         */

	/* number of nodes: a read and write for each data unit a
	 * redundancy computation node for each parity node (nfaults *
	 * nparity) a read and write for each parity unit a block and
	 * commit node (2) a terminate node if atomic RMW an unlock
	 * node for each data unit, redundancy unit
	 * totalNumNodes = (2 * numDataNodes) + (nfaults * numParityNodes)
	 *   + (nfaults * 2 * numParityNodes) + 3;
	 */

	/*
         * Step 2. create the nodes
         */

	blockNode = rf_AllocDAGNode();
	blockNode->list_next = dag_h->nodes;
	dag_h->nodes = blockNode;

	commitNode = rf_AllocDAGNode();
	commitNode->list_next = dag_h->nodes;
	dag_h->nodes = commitNode;

	for (i = 0; i < numDataNodes; i++) {
		tmpNode = rf_AllocDAGNode();
		tmpNode->list_next = dag_h->nodes;
		dag_h->nodes = tmpNode;
	}
	readDataNodes = dag_h->nodes;

	for (i = 0; i < numParityNodes; i++) {
		tmpNode = rf_AllocDAGNode();
		tmpNode->list_next = dag_h->nodes;
		dag_h->nodes = tmpNode;
	}
	readParityNodes = dag_h->nodes;

	for (i = 0; i < numDataNodes; i++) {
		tmpNode = rf_AllocDAGNode();
		tmpNode->list_next = dag_h->nodes;
		dag_h->nodes = tmpNode;
	}
	writeDataNodes = dag_h->nodes;

	for (i = 0; i < numParityNodes; i++) {
		tmpNode = rf_AllocDAGNode();
		tmpNode->list_next = dag_h->nodes;
		dag_h->nodes = tmpNode;
	}
	writeParityNodes = dag_h->nodes;

	for (i = 0; i < numParityNodes; i++) {
		tmpNode = rf_AllocDAGNode();
		tmpNode->list_next = dag_h->nodes;
		dag_h->nodes = tmpNode;
	}
	xorNodes = dag_h->nodes;

	termNode = rf_AllocDAGNode();
	termNode->list_next = dag_h->nodes;
	dag_h->nodes = termNode;

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	if (nfaults == 2) {
		for (i = 0; i < numParityNodes; i++) {
			tmpNode = rf_AllocDAGNode();
			tmpNode->list_next = dag_h->nodes;
			dag_h->nodes = tmpNode;
		}
		readQNodes = dag_h->nodes;

		for (i = 0; i < numParityNodes; i++) {
			tmpNode = rf_AllocDAGNode();
			tmpNode->list_next = dag_h->nodes;
			dag_h->nodes = tmpNode;
		}
		writeQNodes = dag_h->nodes;

		for (i = 0; i < numParityNodes; i++) {
			tmpNode = rf_AllocDAGNode();
			tmpNode->list_next = dag_h->nodes;
			dag_h->nodes = tmpNode;
		}
		qNodes = dag_h->nodes;
	} else {
		readQNodes = writeQNodes = qNodes = NULL;
	}
#endif

	/*
         * Step 3. initialize the nodes
         */
	/* initialize block node (Nil) */
	nNodes = numDataNodes + (nfaults * numParityNodes);
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc,
		    rf_NullNodeUndoFunc, NULL, nNodes, 0, 0, 0,
		    dag_h, "Nil", allocList);

	/* initialize commit node (Cmt) */
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc,
		    rf_NullNodeUndoFunc, NULL, nNodes,
		    (nfaults * numParityNodes), 0, 0, dag_h, "Cmt", allocList);

	/* initialize terminate node (Trm) */
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc,
		    rf_TerminateUndoFunc, NULL, 0, nNodes, 0, 0,
		    dag_h, "Trm", allocList);

	/* initialize nodes which read old data (Rod) */
	tmpreadDataNode = readDataNodes;
	for (i = 0; i < numDataNodes; i++) {
		rf_InitNode(tmpreadDataNode, rf_wait, RF_FALSE,
			    rf_DiskReadFunc, rf_DiskReadUndoFunc,
			    rf_GenericWakeupFunc, (nfaults * numParityNodes),
			    1, 4, 0, dag_h, "Rod", allocList);
		RF_ASSERT(pda != NULL);
		/* physical disk addr desc */
		tmpreadDataNode->params[0].p = pda;
		/* buffer to hold old data */
		tmpreadDataNode->params[1].p = rf_AllocBuffer(raidPtr, dag_h, pda->numSector << raidPtr->logBytesPerSector);
		tmpreadDataNode->params[2].v = parityStripeID;
		tmpreadDataNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
		    which_ru);
		pda = pda->next;
		for (j = 0; j < tmpreadDataNode->numSuccedents; j++) {
			tmpreadDataNode->propList[j] = NULL;
		}
		tmpreadDataNode = tmpreadDataNode->list_next;
	}

	/* initialize nodes which read old parity (Rop) */
	pda = asmap->parityInfo;
	i = 0;
	tmpreadParityNode = readParityNodes;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(tmpreadParityNode, rf_wait, RF_FALSE,
			    rf_DiskReadFunc, rf_DiskReadUndoFunc,
			    rf_GenericWakeupFunc, numParityNodes, 1, 4, 0,
			    dag_h, "Rop", allocList);
		tmpreadParityNode->params[0].p = pda;
		/* buffer to hold old parity */
		tmpreadParityNode->params[1].p = rf_AllocBuffer(raidPtr, dag_h, pda->numSector << raidPtr->logBytesPerSector);
		tmpreadParityNode->params[2].v = parityStripeID;
		tmpreadParityNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
		    which_ru);
		pda = pda->next;
		for (j = 0; j < tmpreadParityNode->numSuccedents; j++) {
			tmpreadParityNode->propList[0] = NULL;
		}
		tmpreadParityNode = tmpreadParityNode->list_next;
	}

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	/* initialize nodes which read old Q (Roq) */
	if (nfaults == 2) {
		pda = asmap->qInfo;
		tmpreadQNode = readQNodes;
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(pda != NULL);
			rf_InitNode(tmpreadQNode, rf_wait, RF_FALSE,
				    rf_DiskReadFunc, rf_DiskReadUndoFunc,
				    rf_GenericWakeupFunc, numParityNodes,
				    1, 4, 0, dag_h, "Roq", allocList);
			tmpreadQNode->params[0].p = pda;
			/* buffer to hold old Q */
			tmpreadQNode->params[1].p = rf_AllocBuffer(raidPtr, dag_h,
								   pda->numSector << raidPtr->logBytesPerSector);
			tmpreadQNode->params[2].v = parityStripeID;
			tmpreadQNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
			    which_ru);
			pda = pda->next;
			for (j = 0; j < tmpreadQNode->numSuccedents; j++) {
				tmpreadQNode->propList[0] = NULL;
			}
			tmpreadQNode = tmpreadQNode->list_next;
		}
	}
#endif
	/* initialize nodes which write new data (Wnd) */
	pda = asmap->physInfo;
	tmpwriteDataNode = writeDataNodes;
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(tmpwriteDataNode, rf_wait, RF_FALSE,
			    rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h,
			    "Wnd", allocList);
		/* physical disk addr desc */
		tmpwriteDataNode->params[0].p = pda;
		/* buffer holding new data to be written */
		tmpwriteDataNode->params[1].p = pda->bufPtr;
		tmpwriteDataNode->params[2].v = parityStripeID;
		tmpwriteDataNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
		    which_ru);
		pda = pda->next;
		tmpwriteDataNode = tmpwriteDataNode->list_next;
	}

	/*
         * Initialize nodes which compute new parity and Q.
         */
	/*
         * We use the simple XOR func in the double-XOR case, and when
         * we're accessing only a portion of one stripe unit. The
         * distinction between the two is that the regular XOR func
         * assumes that the targbuf is a full SU in size, and examines
         * the pda associated with the buffer to decide where within
         * the buffer to XOR the data, whereas the simple XOR func
         * just XORs the data into the start of the buffer.  */
	if ((numParityNodes == 2) || ((numDataNodes == 1)
		&& (asmap->totalSectorsAccessed <
		    raidPtr->Layout.sectorsPerStripeUnit))) {
		func = pfuncs->simple;
		undoFunc = rf_NullNodeUndoFunc;
		name = pfuncs->SimpleName;
		if (qfuncs) {
			qfunc = qfuncs->simple;
			qname = qfuncs->SimpleName;
		} else {
			qfunc = NULL;
			qname = NULL;
		}
	} else {
		func = pfuncs->regular;
		undoFunc = rf_NullNodeUndoFunc;
		name = pfuncs->RegularName;
		if (qfuncs) {
			qfunc = qfuncs->regular;
			qname = qfuncs->RegularName;
		} else {
			qfunc = NULL;
			qname = NULL;
		}
	}
	/*
         * Initialize the xor nodes: params are {pda,buf}
         * from {Rod,Wnd,Rop} nodes, and raidPtr
         */
	if (numParityNodes == 2) {
		/* double-xor case */
		tmpxorNode = xorNodes;
		tmpreadDataNode = readDataNodes;
		tmpreadParityNode = readParityNodes;
		tmpwriteDataNode = writeDataNodes;
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
		tmpqNode = qNodes;
		tmpreadQNode = readQNodes;
#endif
		for (i = 0; i < numParityNodes; i++) {
			/* note: no wakeup func for xor */
			rf_InitNode(tmpxorNode, rf_wait, RF_FALSE, func,
				    undoFunc, NULL, 1,
				    (numDataNodes + numParityNodes),
				    7, 1, dag_h, name, allocList);
			tmpxorNode->flags |= RF_DAGNODE_FLAG_YIELD;
			tmpxorNode->params[0] = tmpreadDataNode->params[0];
			tmpxorNode->params[1] = tmpreadDataNode->params[1];
			tmpxorNode->params[2] = tmpreadParityNode->params[0];
			tmpxorNode->params[3] = tmpreadParityNode->params[1];
			tmpxorNode->params[4] = tmpwriteDataNode->params[0];
			tmpxorNode->params[5] = tmpwriteDataNode->params[1];
			tmpxorNode->params[6].p = raidPtr;
			/* use old parity buf as target buf */
			tmpxorNode->results[0] = tmpreadParityNode->params[1].p;
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
			if (nfaults == 2) {
				/* note: no wakeup func for qor */
				rf_InitNode(tmpqNode, rf_wait, RF_FALSE,
					    qfunc, undoFunc, NULL, 1,
					    (numDataNodes + numParityNodes),
					    7, 1, dag_h, qname, allocList);
				tmpqNode->params[0] = tmpreadDataNode->params[0];
				tmpqNode->params[1] = tmpreadDataNode->params[1];
				tmpqNode->params[2] = tmpreadQNode->.params[0];
				tmpqNode->params[3] = tmpreadQNode->params[1];
				tmpqNode->params[4] = tmpwriteDataNode->params[0];
				tmpqNode->params[5] = tmpwriteDataNode->params[1];
				tmpqNode->params[6].p = raidPtr;
				/* use old Q buf as target buf */
				tmpqNode->results[0] = tmpreadQNode->params[1].p;
				tmpqNode = tmpqNode->list_next;
				tmpreadQNodes = tmpreadQNodes->list_next;
			}
#endif
			tmpxorNode = tmpxorNode->list_next;
			tmpreadDataNode = tmpreadDataNode->list_next;
			tmpreadParityNode = tmpreadParityNode->list_next;
			tmpwriteDataNode = tmpwriteDataNode->list_next;
		}
	} else {
		/* there is only one xor node in this case */
		rf_InitNode(xorNodes, rf_wait, RF_FALSE, func,
			    undoFunc, NULL, 1, (numDataNodes + numParityNodes),
			    (2 * (numDataNodes + numDataNodes + 1) + 1), 1,
			    dag_h, name, allocList);
		xorNodes->flags |= RF_DAGNODE_FLAG_YIELD;
		tmpreadDataNode = readDataNodes;
		for (i = 0; i < numDataNodes; i++) { /* used to be"numDataNodes + 1" until we factored
							out the "+1" into the "deal with Rop separately below */
			/* set up params related to Rod nodes */
			xorNodes->params[2 * i + 0] = tmpreadDataNode->params[0];	/* pda */
			xorNodes->params[2 * i + 1] = tmpreadDataNode->params[1];	/* buffer ptr */
			tmpreadDataNode = tmpreadDataNode->list_next;
		}
		/* deal with Rop separately */
		xorNodes->params[2 * numDataNodes + 0] = readParityNodes->params[0];    /* pda */
		xorNodes->params[2 * numDataNodes + 1] = readParityNodes->params[1];    /* buffer ptr */

		tmpwriteDataNode = writeDataNodes;
		for (i = 0; i < numDataNodes; i++) {
			/* set up params related to Wnd and Wnp nodes */
			xorNodes->params[2 * (numDataNodes + 1 + i) + 0] =	/* pda */
			    tmpwriteDataNode->params[0];
			xorNodes->params[2 * (numDataNodes + 1 + i) + 1] =	/* buffer ptr */
			    tmpwriteDataNode->params[1];
			tmpwriteDataNode = tmpwriteDataNode->list_next;
		}
		/* xor node needs to get at RAID information */
		xorNodes->params[2 * (numDataNodes + numDataNodes + 1)].p = raidPtr;
		xorNodes->results[0] = readParityNodes->params[1].p;
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
		if (nfaults == 2) {
			rf_InitNode(qNodes, rf_wait, RF_FALSE, qfunc,
				    undoFunc, NULL, 1,
				    (numDataNodes + numParityNodes),
				    (2 * (numDataNodes + numDataNodes + 1) + 1), 1,
				    dag_h, qname, allocList);
			tmpreadDataNode = readDataNodes;
			for (i = 0; i < numDataNodes; i++) {
				/* set up params related to Rod */
				qNodes->params[2 * i + 0] = tmpreadDataNode->params[0];	/* pda */
				qNodes->params[2 * i + 1] = tmpreadDataNode->params[1];	/* buffer ptr */
				tmpreadDataNode = tmpreadDataNode->list_next;
			}
			/* and read old q */
			qNodes->params[2 * numDataNodes + 0] =	/* pda */
			    readQNodes->params[0];
			qNodes->params[2 * numDataNodes + 1] =	/* buffer ptr */
			    readQNodes->params[1];
			tmpwriteDataNode = writeDataNodes;
			for (i = 0; i < numDataNodes; i++) {
				/* set up params related to Wnd nodes */
				qNodes->params[2 * (numDataNodes + 1 + i) + 0] =	/* pda */
				    tmpwriteDataNode->params[0];
				qNodes->params[2 * (numDataNodes + 1 + i) + 1] =	/* buffer ptr */
				    tmpwriteDataNode->params[1];
				tmpwriteDataNode = tmpwriteDataNode->list_next;
			}
			/* xor node needs to get at RAID information */
			qNodes->params[2 * (numDataNodes + numDataNodes + 1)].p = raidPtr;
			qNodes->results[0] = readQNodes->params[1].p;
		}
#endif
	}

	/* initialize nodes which write new parity (Wnp) */
	pda = asmap->parityInfo;
	tmpwriteParityNode = writeParityNodes;
	tmpxorNode = xorNodes;
	for (i = 0; i < numParityNodes; i++) {
		rf_InitNode(tmpwriteParityNode, rf_wait, RF_FALSE,
			    rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h,
			    "Wnp", allocList);
		RF_ASSERT(pda != NULL);
		tmpwriteParityNode->params[0].p = pda;	/* param 1 (bufPtr)
				  			 * filled in by xor node */
		tmpwriteParityNode->params[1].p = tmpxorNode->results[0];	/* buffer pointer for
				  						 * parity write
				  						 * operation */
		tmpwriteParityNode->params[2].v = parityStripeID;
		tmpwriteParityNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
		    which_ru);
		pda = pda->next;
		tmpwriteParityNode = tmpwriteParityNode->list_next;
		tmpxorNode = tmpxorNode->list_next;
	}

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	/* initialize nodes which write new Q (Wnq) */
	if (nfaults == 2) {
		pda = asmap->qInfo;
		tmpwriteQNode = writeQNodes;
		tmpqNode = qNodes;
		for (i = 0; i < numParityNodes; i++) {
			rf_InitNode(tmpwriteQNode, rf_wait, RF_FALSE,
				    rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
				    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h,
				    "Wnq", allocList);
			RF_ASSERT(pda != NULL);
			tmpwriteQNode->params[0].p = pda;	/* param 1 (bufPtr)
								 * filled in by xor node */
			tmpwriteQNode->params[1].p = tmpqNode->results[0];	/* buffer pointer for
										 * parity write
										 * operation */
			tmpwriteQNode->params[2].v = parityStripeID;
			tmpwriteQNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
			    which_ru);
			pda = pda->next;
			tmpwriteQNode = tmpwriteQNode->list_next;
			tmpqNode = tmpqNode->list_next;
		}
	}
#endif
	/*
         * Step 4. connect the nodes.
         */

	/* connect header to block node */
	dag_h->succedents[0] = blockNode;

	/* connect block node to read old data nodes */
	RF_ASSERT(blockNode->numSuccedents == (numDataNodes + (numParityNodes * nfaults)));
	tmpreadDataNode = readDataNodes;
	for (i = 0; i < numDataNodes; i++) {
		blockNode->succedents[i] = tmpreadDataNode;
		RF_ASSERT(tmpreadDataNode->numAntecedents == 1);
		tmpreadDataNode->antecedents[0] = blockNode;
		tmpreadDataNode->antType[0] = rf_control;
		tmpreadDataNode = tmpreadDataNode->list_next;
	}

	/* connect block node to read old parity nodes */
	tmpreadParityNode = readParityNodes;
	for (i = 0; i < numParityNodes; i++) {
		blockNode->succedents[numDataNodes + i] = tmpreadParityNode;
		RF_ASSERT(tmpreadParityNode->numAntecedents == 1);
		tmpreadParityNode->antecedents[0] = blockNode;
		tmpreadParityNode->antType[0] = rf_control;
		tmpreadParityNode = tmpreadParityNode->list_next;
	}

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	/* connect block node to read old Q nodes */
	if (nfaults == 2) {
		tmpreadQNode = readQNodes;
		for (i = 0; i < numParityNodes; i++) {
			blockNode->succedents[numDataNodes + numParityNodes + i] = tmpreadQNode;
			RF_ASSERT(tmpreadQNode->numAntecedents == 1);
			tmpreadQNode->antecedents[0] = blockNode;
			tmpreadQNode->antType[0] = rf_control;
			tmpreadQNode = tmpreadQNode->list_next;
		}
	}
#endif
	/* connect read old data nodes to xor nodes */
	tmpreadDataNode = readDataNodes;
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(tmpreadDataNode->numSuccedents == (nfaults * numParityNodes));
		tmpxorNode = xorNodes;
		for (j = 0; j < numParityNodes; j++) {
			RF_ASSERT(tmpxorNode->numAntecedents == numDataNodes + numParityNodes);
			tmpreadDataNode->succedents[j] = tmpxorNode;
			tmpxorNode->antecedents[i] = tmpreadDataNode;
			tmpxorNode->antType[i] = rf_trueData;
			tmpxorNode = tmpxorNode->list_next;
		}
		tmpreadDataNode = tmpreadDataNode->list_next;
	}

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	/* connect read old data nodes to q nodes */
	if (nfaults == 2) {
		tmpreadDataNode = readDataNodes;
		for (i = 0; i < numDataNodes; i++) {
			tmpqNode = qNodes;
			for (j = 0; j < numParityNodes; j++) {
				RF_ASSERT(tmpqNode->numAntecedents == numDataNodes + numParityNodes);
				tmpreadDataNode->succedents[numParityNodes + j] = tmpqNode;
				tmpqNode->antecedents[i] = tmpreadDataNode;
				tmpqNode->antType[i] = rf_trueData;
				tmpqNode = tmpqNode->list_next;
			}
			tmpreadDataNode = tmpreadDataNode->list_next;
		}
	}
#endif
	/* connect read old parity nodes to xor nodes */
	tmpreadParityNode = readParityNodes;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(tmpreadParityNode->numSuccedents == numParityNodes);
		tmpxorNode = xorNodes;
		for (j = 0; j < numParityNodes; j++) {
			tmpreadParityNode->succedents[j] = tmpxorNode;
			tmpxorNode->antecedents[numDataNodes + i] = tmpreadParityNode;
			tmpxorNode->antType[numDataNodes + i] = rf_trueData;
			tmpxorNode = tmpxorNode->list_next;
		}
		tmpreadParityNode = tmpreadParityNode->list_next;
	}

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	/* connect read old q nodes to q nodes */
	if (nfaults == 2) {
		tmpreadParityNode = readParityNodes;
		tmpreadQNode = readQNodes;
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(tmpreadParityNode->numSuccedents == numParityNodes);
			tmpqNode = qNodes;
			for (j = 0; j < numParityNodes; j++) {
				tmpreadQNode->succedents[j] = tmpqNode;
				tmpqNode->antecedents[numDataNodes + i] = tmpreadQNodes;
				tmpqNode->antType[numDataNodes + i] = rf_trueData;
				tmpqNode = tmpqNode->list_next;
			}
			tmpreadParityNode = tmpreadParityNode->list_next;
			tmpreadQNode = tmpreadQNode->list_next;
		}
	}
#endif
	/* connect xor nodes to commit node */
	RF_ASSERT(commitNode->numAntecedents == (nfaults * numParityNodes));
	tmpxorNode = xorNodes;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(tmpxorNode->numSuccedents == 1);
		tmpxorNode->succedents[0] = commitNode;
		commitNode->antecedents[i] = tmpxorNode;
		commitNode->antType[i] = rf_control;
		tmpxorNode = tmpxorNode->list_next;
	}

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	/* connect q nodes to commit node */
	if (nfaults == 2) {
		tmpqNode = qNodes;
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(tmpqNode->numSuccedents == 1);
			tmpqNode->succedents[0] = commitNode;
			commitNode->antecedents[i + numParityNodes] = tmpqNode;
			commitNode->antType[i + numParityNodes] = rf_control;
			tmpqNode = tmpqNode->list_next;
		}
	}
#endif
	/* connect commit node to write nodes */
	RF_ASSERT(commitNode->numSuccedents == (numDataNodes + (nfaults * numParityNodes)));
	tmpwriteDataNode = writeDataNodes;
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(tmpwriteDataNode->numAntecedents == 1);
		commitNode->succedents[i] = tmpwriteDataNode;
		tmpwriteDataNode->antecedents[0] = commitNode;
		tmpwriteDataNode->antType[0] = rf_trueData;
		tmpwriteDataNode = tmpwriteDataNode->list_next;
	}
	tmpwriteParityNode = writeParityNodes;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(tmpwriteParityNode->numAntecedents == 1);
		commitNode->succedents[i + numDataNodes] = tmpwriteParityNode;
		tmpwriteParityNode->antecedents[0] = commitNode;
		tmpwriteParityNode->antType[0] = rf_trueData;
		tmpwriteParityNode = tmpwriteParityNode->list_next;
	}
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	if (nfaults == 2) {
		tmpwriteQNode = writeQNodes;
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(tmpwriteQNode->numAntecedents == 1);
			commitNode->succedents[i + numDataNodes + numParityNodes] = tmpwriteQNode;
			tmpwriteQNode->antecedents[0] = commitNode;
			tmpwriteQNode->antType[0] = rf_trueData;
			tmpwriteQNode = tmpwriteQNode->list_next;
		}
	}
#endif
	RF_ASSERT(termNode->numAntecedents == (numDataNodes + (nfaults * numParityNodes)));
	RF_ASSERT(termNode->numSuccedents == 0);
	tmpwriteDataNode = writeDataNodes;
	for (i = 0; i < numDataNodes; i++) {
		/* connect write new data nodes to term node */
		RF_ASSERT(tmpwriteDataNode->numSuccedents == 1);
		RF_ASSERT(termNode->numAntecedents == (numDataNodes + (nfaults * numParityNodes)));
		tmpwriteDataNode->succedents[0] = termNode;
		termNode->antecedents[i] = tmpwriteDataNode;
		termNode->antType[i] = rf_control;
		tmpwriteDataNode = tmpwriteDataNode->list_next;
	}

	tmpwriteParityNode = writeParityNodes;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(tmpwriteParityNode->numSuccedents == 1);
		tmpwriteParityNode->succedents[0] = termNode;
		termNode->antecedents[numDataNodes + i] = tmpwriteParityNode;
		termNode->antType[numDataNodes + i] = rf_control;
		tmpwriteParityNode = tmpwriteParityNode->list_next;
	}

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)
	if (nfaults == 2) {
		tmpwriteQNode = writeQNodes;
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(tmpwriteQNode->numSuccedents == 1);
			tmpwriteQNode->succedents[0] = termNode;
			termNode->antecedents[numDataNodes + numParityNodes + i] = tmpwriteQNode;
			termNode->antType[numDataNodes + numParityNodes + i] = rf_control;
			tmpwriteQNode = tmpwriteQNode->list_next;
		}
	}
#endif
}


/******************************************************************************
 * create a write graph (fault-free or degraded) for RAID level 1
 *
 * Hdr -> Commit -> Wpd -> Nil -> Trm
 *               -> Wsd ->
 *
 * The "Wpd" node writes data to the primary copy in the mirror pair
 * The "Wsd" node writes data to the secondary copy in the mirror pair
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (holds write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *****************************************************************************/

void
rf_CreateRaidOneWriteDAG(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
			 RF_DagHeader_t *dag_h, void *bp,
			 RF_RaidAccessFlags_t flags,
			 RF_AllocListElem_t *allocList)
{
	RF_DagNode_t *unblockNode, *termNode, *commitNode;
	RF_DagNode_t *wndNode, *wmirNode;
	RF_DagNode_t *tmpNode, *tmpwndNode, *tmpwmirNode;
	int     nWndNodes, nWmirNodes, i;
	RF_ReconUnitNum_t which_ru;
	RF_PhysDiskAddr_t *pda, *pdaP;
	RF_StripeNum_t parityStripeID;

	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout),
	    asmap->raidAddress, &which_ru);
#if RF_DEBUG_DAG
	if (rf_dagDebug) {
		printf("[Creating RAID level 1 write DAG]\n");
	}
#endif
	dag_h->creator = "RaidOneWriteDAG";

	/* 2 implies access not SU aligned */
	nWmirNodes = (asmap->parityInfo->next) ? 2 : 1;
	nWndNodes = (asmap->physInfo->next) ? 2 : 1;

	/* alloc the Wnd nodes and the Wmir node */
	if (asmap->numDataFailed == 1)
		nWndNodes--;
	if (asmap->numParityFailed == 1)
		nWmirNodes--;

	/* total number of nodes = nWndNodes + nWmirNodes + (commit + unblock
	 * + terminator) */
	for (i = 0; i < nWndNodes; i++) {
		tmpNode = rf_AllocDAGNode();
		tmpNode->list_next = dag_h->nodes;
		dag_h->nodes = tmpNode;
	}
	wndNode = dag_h->nodes;

	for (i = 0; i < nWmirNodes; i++) {
		tmpNode = rf_AllocDAGNode();
		tmpNode->list_next = dag_h->nodes;
		dag_h->nodes = tmpNode;
	}
	wmirNode = dag_h->nodes;

	commitNode = rf_AllocDAGNode();
	commitNode->list_next = dag_h->nodes;
	dag_h->nodes = commitNode;

	unblockNode = rf_AllocDAGNode();
	unblockNode->list_next = dag_h->nodes;
	dag_h->nodes = unblockNode;

	termNode = rf_AllocDAGNode();
	termNode->list_next = dag_h->nodes;
	dag_h->nodes = termNode;

	/* this dag can commit immediately */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* initialize the commit, unblock, and term nodes */
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc,
		    rf_NullNodeUndoFunc, NULL, (nWndNodes + nWmirNodes),
		    0, 0, 0, dag_h, "Cmt", allocList);
	rf_InitNode(unblockNode, rf_wait, RF_FALSE, rf_NullNodeFunc,
		    rf_NullNodeUndoFunc, NULL, 1, (nWndNodes + nWmirNodes),
		    0, 0, dag_h, "Nil", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc,
		    rf_TerminateUndoFunc, NULL, 0, 1, 0, 0,
		    dag_h, "Trm", allocList);

	/* initialize the wnd nodes */
	if (nWndNodes > 0) {
		pda = asmap->physInfo;
		tmpwndNode = wndNode;
		for (i = 0; i < nWndNodes; i++) {
			rf_InitNode(tmpwndNode, rf_wait, RF_FALSE,
				    rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
				    rf_GenericWakeupFunc, 1, 1, 4, 0,
				    dag_h, "Wpd", allocList);
			RF_ASSERT(pda != NULL);
			tmpwndNode->params[0].p = pda;
			tmpwndNode->params[1].p = pda->bufPtr;
			tmpwndNode->params[2].v = parityStripeID;
			tmpwndNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, which_ru);
			pda = pda->next;
			tmpwndNode = tmpwndNode->list_next;
		}
		RF_ASSERT(pda == NULL);
	}
	/* initialize the mirror nodes */
	if (nWmirNodes > 0) {
		pda = asmap->physInfo;
		pdaP = asmap->parityInfo;
		tmpwmirNode = wmirNode;
		for (i = 0; i < nWmirNodes; i++) {
			rf_InitNode(tmpwmirNode, rf_wait, RF_FALSE,
				    rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
				    rf_GenericWakeupFunc, 1, 1, 4, 0,
				    dag_h, "Wsd", allocList);
			RF_ASSERT(pda != NULL);
			tmpwmirNode->params[0].p = pdaP;
			tmpwmirNode->params[1].p = pda->bufPtr;
			tmpwmirNode->params[2].v = parityStripeID;
			tmpwmirNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, which_ru);
			pda = pda->next;
			pdaP = pdaP->next;
			tmpwmirNode = tmpwmirNode->list_next;
		}
		RF_ASSERT(pda == NULL);
		RF_ASSERT(pdaP == NULL);
	}
	/* link the header node to the commit node */
	RF_ASSERT(dag_h->numSuccedents == 1);
	RF_ASSERT(commitNode->numAntecedents == 0);
	dag_h->succedents[0] = commitNode;

	/* link the commit node to the write nodes */
	RF_ASSERT(commitNode->numSuccedents == (nWndNodes + nWmirNodes));
	tmpwndNode = wndNode;
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(tmpwndNode->numAntecedents == 1);
		commitNode->succedents[i] = tmpwndNode;
		tmpwndNode->antecedents[0] = commitNode;
		tmpwndNode->antType[0] = rf_control;
		tmpwndNode = tmpwndNode->list_next;
	}
	tmpwmirNode = wmirNode;
	for (i = 0; i < nWmirNodes; i++) {
		RF_ASSERT(tmpwmirNode->numAntecedents == 1);
		commitNode->succedents[i + nWndNodes] = tmpwmirNode;
		tmpwmirNode->antecedents[0] = commitNode;
		tmpwmirNode->antType[0] = rf_control;
		tmpwmirNode = tmpwmirNode->list_next;
	}

	/* link the write nodes to the unblock node */
	RF_ASSERT(unblockNode->numAntecedents == (nWndNodes + nWmirNodes));
	tmpwndNode = wndNode;
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(tmpwndNode->numSuccedents == 1);
		tmpwndNode->succedents[0] = unblockNode;
		unblockNode->antecedents[i] = tmpwndNode;
		unblockNode->antType[i] = rf_control;
		tmpwndNode = tmpwndNode->list_next;
	}
	tmpwmirNode = wmirNode;
	for (i = 0; i < nWmirNodes; i++) {
		RF_ASSERT(tmpwmirNode->numSuccedents == 1);
		tmpwmirNode->succedents[0] = unblockNode;
		unblockNode->antecedents[i + nWndNodes] = tmpwmirNode;
		unblockNode->antType[i + nWndNodes] = rf_control;
		tmpwmirNode = tmpwmirNode->list_next;
	}

	/* link the unblock node to the term node */
	RF_ASSERT(unblockNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	unblockNode->succedents[0] = termNode;
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;
}
