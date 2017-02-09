/*	$NetBSD: rf_dagutils.c,v 1.53 2011/05/11 18:13:12 mrg Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Mark Holland, William V. Courtright II, Jim Zelenka
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

/******************************************************************************
 *
 * rf_dagutils.c -- utility routines for manipulating dags
 *
 *****************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_dagutils.c,v 1.53 2011/05/11 18:13:12 mrg Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_archs.h"
#include "rf_threadstuff.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_general.h"
#include "rf_map.h"
#include "rf_shutdown.h"

#define SNUM_DIFF(_a_,_b_) (((_a_)>(_b_))?((_a_)-(_b_)):((_b_)-(_a_)))

const RF_RedFuncs_t rf_xorFuncs = {
	rf_RegularXorFunc, "Reg Xr",
	rf_SimpleXorFunc, "Simple Xr"};

const RF_RedFuncs_t rf_xorRecoveryFuncs = {
	rf_RecoveryXorFunc, "Recovery Xr",
	rf_RecoveryXorFunc, "Recovery Xr"};

#if RF_DEBUG_VALIDATE_DAG
static void rf_RecurPrintDAG(RF_DagNode_t *, int, int);
static void rf_PrintDAG(RF_DagHeader_t *);
static int rf_ValidateBranch(RF_DagNode_t *, int *, int *,
			     RF_DagNode_t **, int);
static void rf_ValidateBranchVisitedBits(RF_DagNode_t *, int, int);
static void rf_ValidateVisitedBits(RF_DagHeader_t *);
#endif /* RF_DEBUG_VALIDATE_DAG */

/* The maximum number of nodes in a DAG is bounded by

(2 * raidPtr->Layout->numDataCol) + (1 * layoutPtr->numParityCol) +
	(1 * 2 * layoutPtr->numParityCol) + 3

which is:  2*RF_MAXCOL+1*2+1*2*2+3

For RF_MAXCOL of 40, this works out to 89.  We use this value to provide an estimate
on the maximum size needed for RF_DAGPCACHE_SIZE.  For RF_MAXCOL of 40, this structure
would be 534 bytes.  Too much to have on-hand in a RF_DagNode_t, but should be ok to
have a few kicking around.
*/
#define RF_DAGPCACHE_SIZE ((2*RF_MAXCOL+1*2+1*2*2+3) *(RF_MAX(sizeof(RF_DagParam_t), sizeof(RF_DagNode_t *))))


/******************************************************************************
 *
 * InitNode - initialize a dag node
 *
 * the size of the propList array is always the same as that of the
 * successors array.
 *
 *****************************************************************************/
void
rf_InitNode(RF_DagNode_t *node, RF_NodeStatus_t initstatus, int commit,
    int (*doFunc) (RF_DagNode_t *node),
    int (*undoFunc) (RF_DagNode_t *node),
    int (*wakeFunc) (RF_DagNode_t *node, int status),
    int nSucc, int nAnte, int nParam, int nResult,
    RF_DagHeader_t *hdr, const char *name, RF_AllocListElem_t *alist)
{
	void  **ptrs;
	int     nptrs;

	if (nAnte > RF_MAX_ANTECEDENTS)
		RF_PANIC();
	node->status = initstatus;
	node->commitNode = commit;
	node->doFunc = doFunc;
	node->undoFunc = undoFunc;
	node->wakeFunc = wakeFunc;
	node->numParams = nParam;
	node->numResults = nResult;
	node->numAntecedents = nAnte;
	node->numAntDone = 0;
	node->next = NULL;
	/* node->list_next = NULL */  /* Don't touch this here!
	                                 It may already be
					 in use by the caller! */
	node->numSuccedents = nSucc;
	node->name = name;
	node->dagHdr = hdr;
	node->big_dag_ptrs = NULL;
	node->big_dag_params = NULL;
	node->visited = 0;

	/* allocate all the pointers with one call to malloc */
	nptrs = nSucc + nAnte + nResult + nSucc;

	if (nptrs <= RF_DAG_PTRCACHESIZE) {
		/*
	         * The dag_ptrs field of the node is basically some scribble
	         * space to be used here. We could get rid of it, and always
	         * allocate the range of pointers, but that's expensive. So,
	         * we pick a "common case" size for the pointer cache. Hopefully,
	         * we'll find that:
	         * (1) Generally, nptrs doesn't exceed RF_DAG_PTRCACHESIZE by
	         *     only a little bit (least efficient case)
	         * (2) Generally, ntprs isn't a lot less than RF_DAG_PTRCACHESIZE
	         *     (wasted memory)
	         */
		ptrs = (void **) node->dag_ptrs;
	} else if (nptrs <= (RF_DAGPCACHE_SIZE / sizeof(RF_DagNode_t *))) {
		node->big_dag_ptrs = rf_AllocDAGPCache();
		ptrs = (void **) node->big_dag_ptrs;
	} else {
		RF_MallocAndAdd(ptrs, nptrs * sizeof(void *),
				(void **), alist);
	}
	node->succedents = (nSucc) ? (RF_DagNode_t **) ptrs : NULL;
	node->antecedents = (nAnte) ? (RF_DagNode_t **) (ptrs + nSucc) : NULL;
	node->results = (nResult) ? (void **) (ptrs + nSucc + nAnte) : NULL;
	node->propList = (nSucc) ? (RF_PropHeader_t **) (ptrs + nSucc + nAnte + nResult) : NULL;

	if (nParam) {
		if (nParam <= RF_DAG_PARAMCACHESIZE) {
			node->params = (RF_DagParam_t *) node->dag_params;
		} else if (nParam <= (RF_DAGPCACHE_SIZE / sizeof(RF_DagParam_t))) {
			node->big_dag_params = rf_AllocDAGPCache();
			node->params = node->big_dag_params;
		} else {
			RF_MallocAndAdd(node->params,
					nParam * sizeof(RF_DagParam_t),
					(RF_DagParam_t *), alist);
		}
	} else {
		node->params = NULL;
	}
}



/******************************************************************************
 *
 * allocation and deallocation routines
 *
 *****************************************************************************/

void
rf_FreeDAG(RF_DagHeader_t *dag_h)
{
	RF_AccessStripeMapHeader_t *asmap, *t_asmap;
	RF_PhysDiskAddr_t *pda;
	RF_DagNode_t *tmpnode;
	RF_DagHeader_t *nextDag;

	while (dag_h) {
		nextDag = dag_h->next;
		rf_FreeAllocList(dag_h->allocList);
		for (asmap = dag_h->asmList; asmap;) {
			t_asmap = asmap;
			asmap = asmap->next;
			rf_FreeAccessStripeMap(t_asmap);
		}
		while (dag_h->pda_cleanup_list) {
			pda = dag_h->pda_cleanup_list;
			dag_h->pda_cleanup_list = dag_h->pda_cleanup_list->next;
			rf_FreePhysDiskAddr(pda);
		}
		while (dag_h->nodes) {
			tmpnode = dag_h->nodes;
			dag_h->nodes = dag_h->nodes->list_next;
			rf_FreeDAGNode(tmpnode);
		}
		rf_FreeDAGHeader(dag_h);
		dag_h = nextDag;
	}
}

#define RF_MAX_FREE_DAGH 128
#define RF_MIN_FREE_DAGH  32

#define RF_MAX_FREE_DAGNODE 512 /* XXX Tune this... */
#define RF_MIN_FREE_DAGNODE 128 /* XXX Tune this... */

#define RF_MAX_FREE_DAGLIST 128
#define RF_MIN_FREE_DAGLIST  32

#define RF_MAX_FREE_DAGPCACHE 128
#define RF_MIN_FREE_DAGPCACHE   8

#define RF_MAX_FREE_FUNCLIST 128
#define RF_MIN_FREE_FUNCLIST  32

#define RF_MAX_FREE_BUFFERS 128
#define RF_MIN_FREE_BUFFERS  32

static void rf_ShutdownDAGs(void *);
static void
rf_ShutdownDAGs(void *ignored)
{
	pool_destroy(&rf_pools.dagh);
	pool_destroy(&rf_pools.dagnode);
	pool_destroy(&rf_pools.daglist);
	pool_destroy(&rf_pools.dagpcache);
	pool_destroy(&rf_pools.funclist);
}

int
rf_ConfigureDAGs(RF_ShutdownList_t **listp)
{

	rf_pool_init(&rf_pools.dagnode, sizeof(RF_DagNode_t),
		     "rf_dagnode_pl", RF_MIN_FREE_DAGNODE, RF_MAX_FREE_DAGNODE);
	rf_pool_init(&rf_pools.dagh, sizeof(RF_DagHeader_t),
		     "rf_dagh_pl", RF_MIN_FREE_DAGH, RF_MAX_FREE_DAGH);
	rf_pool_init(&rf_pools.daglist, sizeof(RF_DagList_t),
		     "rf_daglist_pl", RF_MIN_FREE_DAGLIST, RF_MAX_FREE_DAGLIST);
	rf_pool_init(&rf_pools.dagpcache, RF_DAGPCACHE_SIZE,
		     "rf_dagpcache_pl", RF_MIN_FREE_DAGPCACHE, RF_MAX_FREE_DAGPCACHE);
	rf_pool_init(&rf_pools.funclist, sizeof(RF_FuncList_t),
		     "rf_funclist_pl", RF_MIN_FREE_FUNCLIST, RF_MAX_FREE_FUNCLIST);
	rf_ShutdownCreate(listp, rf_ShutdownDAGs, NULL);

	return (0);
}

RF_DagHeader_t *
rf_AllocDAGHeader(void)
{
	RF_DagHeader_t *dh;

	dh = pool_get(&rf_pools.dagh, PR_WAITOK);
	memset((char *) dh, 0, sizeof(RF_DagHeader_t));
	return (dh);
}

void
rf_FreeDAGHeader(RF_DagHeader_t * dh)
{
	pool_put(&rf_pools.dagh, dh);
}

RF_DagNode_t *
rf_AllocDAGNode(void)
{
	RF_DagNode_t *node;

	node = pool_get(&rf_pools.dagnode, PR_WAITOK);
	memset(node, 0, sizeof(RF_DagNode_t));
	return (node);
}

void
rf_FreeDAGNode(RF_DagNode_t *node)
{
	if (node->big_dag_ptrs) {
		rf_FreeDAGPCache(node->big_dag_ptrs);
	}
	if (node->big_dag_params) {
		rf_FreeDAGPCache(node->big_dag_params);
	}
	pool_put(&rf_pools.dagnode, node);
}

RF_DagList_t *
rf_AllocDAGList(void)
{
	RF_DagList_t *dagList;

	dagList = pool_get(&rf_pools.daglist, PR_WAITOK);
	memset(dagList, 0, sizeof(RF_DagList_t));

	return (dagList);
}

void
rf_FreeDAGList(RF_DagList_t *dagList)
{
	pool_put(&rf_pools.daglist, dagList);
}

void *
rf_AllocDAGPCache(void)
{
	void *p;
	p = pool_get(&rf_pools.dagpcache, PR_WAITOK);
	memset(p, 0, RF_DAGPCACHE_SIZE);

	return (p);
}

void
rf_FreeDAGPCache(void *p)
{
	pool_put(&rf_pools.dagpcache, p);
}

RF_FuncList_t *
rf_AllocFuncList(void)
{
	RF_FuncList_t *funcList;

	funcList = pool_get(&rf_pools.funclist, PR_WAITOK);
	memset(funcList, 0, sizeof(RF_FuncList_t));

	return (funcList);
}

void
rf_FreeFuncList(RF_FuncList_t *funcList)
{
	pool_put(&rf_pools.funclist, funcList);
}

/* allocates a stripe buffer -- a buffer large enough to hold all the data
   in an entire stripe.
*/

void *
rf_AllocStripeBuffer(RF_Raid_t *raidPtr, RF_DagHeader_t *dag_h,
    int size)
{
	RF_VoidPointerListElem_t *vple;
	void *p;

	RF_ASSERT((size <= (raidPtr->numCol * (raidPtr->Layout.sectorsPerStripeUnit <<
					       raidPtr->logBytesPerSector))));

	p =  malloc( raidPtr->numCol * (raidPtr->Layout.sectorsPerStripeUnit <<
					raidPtr->logBytesPerSector),
		     M_RAIDFRAME, M_NOWAIT);
	if (!p) {
		rf_lock_mutex2(raidPtr->mutex);
		if (raidPtr->stripebuf_count > 0) {
			vple = raidPtr->stripebuf;
			raidPtr->stripebuf = vple->next;
			p = vple->p;
			rf_FreeVPListElem(vple);
			raidPtr->stripebuf_count--;
		} else {
#ifdef DIAGNOSTIC
			printf("raid%d: Help!  Out of emergency full-stripe buffers!\n", raidPtr->raidid);
#endif
		}
		rf_unlock_mutex2(raidPtr->mutex);
		if (!p) {
			/* We didn't get a buffer... not much we can do other than wait,
			   and hope that someone frees up memory for us.. */
			p = malloc( raidPtr->numCol * (raidPtr->Layout.sectorsPerStripeUnit <<
						       raidPtr->logBytesPerSector), M_RAIDFRAME, M_WAITOK);
		}
	}
	memset(p, 0, raidPtr->numCol * (raidPtr->Layout.sectorsPerStripeUnit << raidPtr->logBytesPerSector));

	vple = rf_AllocVPListElem();
	vple->p = p;
        vple->next = dag_h->desc->stripebufs;
        dag_h->desc->stripebufs = vple;

	return (p);
}


void
rf_FreeStripeBuffer(RF_Raid_t *raidPtr, RF_VoidPointerListElem_t *vple)
{
	rf_lock_mutex2(raidPtr->mutex);
	if (raidPtr->stripebuf_count < raidPtr->numEmergencyStripeBuffers) {
		/* just tack it in */
		vple->next = raidPtr->stripebuf;
		raidPtr->stripebuf = vple;
		raidPtr->stripebuf_count++;
	} else {
		free(vple->p, M_RAIDFRAME);
		rf_FreeVPListElem(vple);
	}
	rf_unlock_mutex2(raidPtr->mutex);
}

/* allocates a buffer big enough to hold the data described by the
caller (ie. the data of the associated PDA).  Glue this buffer
into our dag_h cleanup structure. */

void *
rf_AllocBuffer(RF_Raid_t *raidPtr, RF_DagHeader_t *dag_h, int size)
{
	RF_VoidPointerListElem_t *vple;
	void *p;

	p = rf_AllocIOBuffer(raidPtr, size);
	vple = rf_AllocVPListElem();
	vple->p = p;
	vple->next = dag_h->desc->iobufs;
	dag_h->desc->iobufs = vple;

	return (p);
}

void *
rf_AllocIOBuffer(RF_Raid_t *raidPtr, int size)
{
	RF_VoidPointerListElem_t *vple;
	void *p;

	RF_ASSERT((size <= (raidPtr->Layout.sectorsPerStripeUnit <<
			   raidPtr->logBytesPerSector)));

	p =  malloc( raidPtr->Layout.sectorsPerStripeUnit <<
				 raidPtr->logBytesPerSector,
				 M_RAIDFRAME, M_NOWAIT);
	if (!p) {
		rf_lock_mutex2(raidPtr->mutex);
		if (raidPtr->iobuf_count > 0) {
			vple = raidPtr->iobuf;
			raidPtr->iobuf = vple->next;
			p = vple->p;
			rf_FreeVPListElem(vple);
			raidPtr->iobuf_count--;
		} else {
#ifdef DIAGNOSTIC
			printf("raid%d: Help!  Out of emergency buffers!\n", raidPtr->raidid);
#endif
		}
		rf_unlock_mutex2(raidPtr->mutex);
		if (!p) {
			/* We didn't get a buffer... not much we can do other than wait,
			   and hope that someone frees up memory for us.. */
			p = malloc( raidPtr->Layout.sectorsPerStripeUnit <<
				    raidPtr->logBytesPerSector,
				    M_RAIDFRAME, M_WAITOK);
		}
	}
	memset(p, 0, raidPtr->Layout.sectorsPerStripeUnit << raidPtr->logBytesPerSector);
	return (p);
}

void
rf_FreeIOBuffer(RF_Raid_t *raidPtr, RF_VoidPointerListElem_t *vple)
{
	rf_lock_mutex2(raidPtr->mutex);
	if (raidPtr->iobuf_count < raidPtr->numEmergencyBuffers) {
		/* just tack it in */
		vple->next = raidPtr->iobuf;
		raidPtr->iobuf = vple;
		raidPtr->iobuf_count++;
	} else {
		free(vple->p, M_RAIDFRAME);
		rf_FreeVPListElem(vple);
	}
	rf_unlock_mutex2(raidPtr->mutex);
}



#if RF_DEBUG_VALIDATE_DAG
/******************************************************************************
 *
 * debug routines
 *
 *****************************************************************************/

char   *
rf_NodeStatusString(RF_DagNode_t *node)
{
	switch (node->status) {
	case rf_wait:
		return ("wait");
	case rf_fired:
		return ("fired");
	case rf_good:
		return ("good");
	case rf_bad:
		return ("bad");
	default:
		return ("?");
	}
}

void
rf_PrintNodeInfoString(RF_DagNode_t *node)
{
	RF_PhysDiskAddr_t *pda;
	int     (*df) (RF_DagNode_t *) = node->doFunc;
	int     i, lk, unlk;
	void   *bufPtr;

	if ((df == rf_DiskReadFunc) || (df == rf_DiskWriteFunc)
	    || (df == rf_DiskReadMirrorIdleFunc)
	    || (df == rf_DiskReadMirrorPartitionFunc)) {
		pda = (RF_PhysDiskAddr_t *) node->params[0].p;
		bufPtr = (void *) node->params[1].p;
		lk = 0;
		unlk = 0;
		RF_ASSERT(!(lk && unlk));
		printf("c %d offs %ld nsect %d buf 0x%lx %s\n", pda->col,
		    (long) pda->startSector, (int) pda->numSector, (long) bufPtr,
		    (lk) ? "LOCK" : ((unlk) ? "UNLK" : " "));
		return;
	}
	if ((df == rf_SimpleXorFunc) || (df == rf_RegularXorFunc)
	    || (df == rf_RecoveryXorFunc)) {
		printf("result buf 0x%lx\n", (long) node->results[0]);
		for (i = 0; i < node->numParams - 1; i += 2) {
			pda = (RF_PhysDiskAddr_t *) node->params[i].p;
			bufPtr = (RF_PhysDiskAddr_t *) node->params[i + 1].p;
			printf("    buf 0x%lx c%d offs %ld nsect %d\n",
			    (long) bufPtr, pda->col,
			    (long) pda->startSector, (int) pda->numSector);
		}
		return;
	}
#if RF_INCLUDE_PARITYLOGGING > 0
	if (df == rf_ParityLogOverwriteFunc || df == rf_ParityLogUpdateFunc) {
		for (i = 0; i < node->numParams - 1; i += 2) {
			pda = (RF_PhysDiskAddr_t *) node->params[i].p;
			bufPtr = (RF_PhysDiskAddr_t *) node->params[i + 1].p;
			printf(" c%d offs %ld nsect %d buf 0x%lx\n",
			    pda->col, (long) pda->startSector,
			    (int) pda->numSector, (long) bufPtr);
		}
		return;
	}
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */

	if ((df == rf_TerminateFunc) || (df == rf_NullNodeFunc)) {
		printf("\n");
		return;
	}
	printf("?\n");
}
#ifdef DEBUG
static void
rf_RecurPrintDAG(RF_DagNode_t *node, int depth, int unvisited)
{
	char   *anttype;
	int     i;

	node->visited = (unvisited) ? 0 : 1;
	printf("(%d) %d C%d %s: %s,s%d %d/%d,a%d/%d,p%d,r%d S{", depth,
	    node->nodeNum, node->commitNode, node->name, rf_NodeStatusString(node),
	    node->numSuccedents, node->numSuccFired, node->numSuccDone,
	    node->numAntecedents, node->numAntDone, node->numParams, node->numResults);
	for (i = 0; i < node->numSuccedents; i++) {
		printf("%d%s", node->succedents[i]->nodeNum,
		    ((i == node->numSuccedents - 1) ? "\0" : " "));
	}
	printf("} A{");
	for (i = 0; i < node->numAntecedents; i++) {
		switch (node->antType[i]) {
		case rf_trueData:
			anttype = "T";
			break;
		case rf_antiData:
			anttype = "A";
			break;
		case rf_outputData:
			anttype = "O";
			break;
		case rf_control:
			anttype = "C";
			break;
		default:
			anttype = "?";
			break;
		}
		printf("%d(%s)%s", node->antecedents[i]->nodeNum, anttype, (i == node->numAntecedents - 1) ? "\0" : " ");
	}
	printf("}; ");
	rf_PrintNodeInfoString(node);
	for (i = 0; i < node->numSuccedents; i++) {
		if (node->succedents[i]->visited == unvisited)
			rf_RecurPrintDAG(node->succedents[i], depth + 1, unvisited);
	}
}

static void
rf_PrintDAG(RF_DagHeader_t *dag_h)
{
	int     unvisited, i;
	char   *status;

	/* set dag status */
	switch (dag_h->status) {
	case rf_enable:
		status = "enable";
		break;
	case rf_rollForward:
		status = "rollForward";
		break;
	case rf_rollBackward:
		status = "rollBackward";
		break;
	default:
		status = "illegal!";
		break;
	}
	/* find out if visited bits are currently set or clear */
	unvisited = dag_h->succedents[0]->visited;

	printf("DAG type:  %s\n", dag_h->creator);
	printf("format is (depth) num commit type: status,nSucc nSuccFired/nSuccDone,nAnte/nAnteDone,nParam,nResult S{x} A{x(type)};  info\n");
	printf("(0) %d Hdr: %s, s%d, (commit %d/%d) S{", dag_h->nodeNum,
	    status, dag_h->numSuccedents, dag_h->numCommitNodes, dag_h->numCommits);
	for (i = 0; i < dag_h->numSuccedents; i++) {
		printf("%d%s", dag_h->succedents[i]->nodeNum,
		    ((i == dag_h->numSuccedents - 1) ? "\0" : " "));
	}
	printf("};\n");
	for (i = 0; i < dag_h->numSuccedents; i++) {
		if (dag_h->succedents[i]->visited == unvisited)
			rf_RecurPrintDAG(dag_h->succedents[i], 1, unvisited);
	}
}
#endif
/* assigns node numbers */
int
rf_AssignNodeNums(RF_DagHeader_t * dag_h)
{
	int     unvisited, i, nnum;
	RF_DagNode_t *node;

	nnum = 0;
	unvisited = dag_h->succedents[0]->visited;

	dag_h->nodeNum = nnum++;
	for (i = 0; i < dag_h->numSuccedents; i++) {
		node = dag_h->succedents[i];
		if (node->visited == unvisited) {
			nnum = rf_RecurAssignNodeNums(dag_h->succedents[i], nnum, unvisited);
		}
	}
	return (nnum);
}

int
rf_RecurAssignNodeNums(RF_DagNode_t *node, int num, int unvisited)
{
	int     i;

	node->visited = (unvisited) ? 0 : 1;

	node->nodeNum = num++;
	for (i = 0; i < node->numSuccedents; i++) {
		if (node->succedents[i]->visited == unvisited) {
			num = rf_RecurAssignNodeNums(node->succedents[i], num, unvisited);
		}
	}
	return (num);
}
/* set the header pointers in each node to "newptr" */
void
rf_ResetDAGHeaderPointers(RF_DagHeader_t *dag_h, RF_DagHeader_t *newptr)
{
	int     i;
	for (i = 0; i < dag_h->numSuccedents; i++)
		if (dag_h->succedents[i]->dagHdr != newptr)
			rf_RecurResetDAGHeaderPointers(dag_h->succedents[i], newptr);
}

void
rf_RecurResetDAGHeaderPointers(RF_DagNode_t *node, RF_DagHeader_t *newptr)
{
	int     i;
	node->dagHdr = newptr;
	for (i = 0; i < node->numSuccedents; i++)
		if (node->succedents[i]->dagHdr != newptr)
			rf_RecurResetDAGHeaderPointers(node->succedents[i], newptr);
}


void
rf_PrintDAGList(RF_DagHeader_t * dag_h)
{
	int     i = 0;

	for (; dag_h; dag_h = dag_h->next) {
		rf_AssignNodeNums(dag_h);
		printf("\n\nDAG %d IN LIST:\n", i++);
		rf_PrintDAG(dag_h);
	}
}

static int
rf_ValidateBranch(RF_DagNode_t *node, int *scount, int *acount,
		  RF_DagNode_t **nodes, int unvisited)
{
	int     i, retcode = 0;

	/* construct an array of node pointers indexed by node num */
	node->visited = (unvisited) ? 0 : 1;
	nodes[node->nodeNum] = node;

	if (node->next != NULL) {
		printf("INVALID DAG: next pointer in node is not NULL\n");
		retcode = 1;
	}
	if (node->status != rf_wait) {
		printf("INVALID DAG: Node status is not wait\n");
		retcode = 1;
	}
	if (node->numAntDone != 0) {
		printf("INVALID DAG: numAntDone is not zero\n");
		retcode = 1;
	}
	if (node->doFunc == rf_TerminateFunc) {
		if (node->numSuccedents != 0) {
			printf("INVALID DAG: Terminator node has succedents\n");
			retcode = 1;
		}
	} else {
		if (node->numSuccedents == 0) {
			printf("INVALID DAG: Non-terminator node has no succedents\n");
			retcode = 1;
		}
	}
	for (i = 0; i < node->numSuccedents; i++) {
		if (!node->succedents[i]) {
			printf("INVALID DAG: succedent %d of node %s is NULL\n", i, node->name);
			retcode = 1;
		}
		scount[node->succedents[i]->nodeNum]++;
	}
	for (i = 0; i < node->numAntecedents; i++) {
		if (!node->antecedents[i]) {
			printf("INVALID DAG: antecedent %d of node %s is NULL\n", i, node->name);
			retcode = 1;
		}
		acount[node->antecedents[i]->nodeNum]++;
	}
	for (i = 0; i < node->numSuccedents; i++) {
		if (node->succedents[i]->visited == unvisited) {
			if (rf_ValidateBranch(node->succedents[i], scount,
				acount, nodes, unvisited)) {
				retcode = 1;
			}
		}
	}
	return (retcode);
}

static void
rf_ValidateBranchVisitedBits(RF_DagNode_t *node, int unvisited, int rl)
{
	int     i;

	RF_ASSERT(node->visited == unvisited);
	for (i = 0; i < node->numSuccedents; i++) {
		if (node->succedents[i] == NULL) {
			printf("node=%lx node->succedents[%d] is NULL\n", (long) node, i);
			RF_ASSERT(0);
		}
		rf_ValidateBranchVisitedBits(node->succedents[i], unvisited, rl + 1);
	}
}
/* NOTE:  never call this on a big dag, because it is exponential
 * in execution time
 */
static void
rf_ValidateVisitedBits(RF_DagHeader_t *dag)
{
	int     i, unvisited;

	unvisited = dag->succedents[0]->visited;

	for (i = 0; i < dag->numSuccedents; i++) {
		if (dag->succedents[i] == NULL) {
			printf("dag=%lx dag->succedents[%d] is NULL\n", (long) dag, i);
			RF_ASSERT(0);
		}
		rf_ValidateBranchVisitedBits(dag->succedents[i], unvisited, 0);
	}
}
/* validate a DAG.  _at entry_ verify that:
 *   -- numNodesCompleted is zero
 *   -- node queue is null
 *   -- dag status is rf_enable
 *   -- next pointer is null on every node
 *   -- all nodes have status wait
 *   -- numAntDone is zero in all nodes
 *   -- terminator node has zero successors
 *   -- no other node besides terminator has zero successors
 *   -- no successor or antecedent pointer in a node is NULL
 *   -- number of times that each node appears as a successor of another node
 *      is equal to the antecedent count on that node
 *   -- number of times that each node appears as an antecedent of another node
 *      is equal to the succedent count on that node
 *   -- what else?
 */
int
rf_ValidateDAG(RF_DagHeader_t *dag_h)
{
	int     i, nodecount;
	int    *scount, *acount;/* per-node successor and antecedent counts */
	RF_DagNode_t **nodes;	/* array of ptrs to nodes in dag */
	int     retcode = 0;
	int     unvisited;
	int     commitNodeCount = 0;

	if (rf_validateVisitedDebug)
		rf_ValidateVisitedBits(dag_h);

	if (dag_h->numNodesCompleted != 0) {
		printf("INVALID DAG: num nodes completed is %d, should be 0\n", dag_h->numNodesCompleted);
		retcode = 1;
		goto validate_dag_bad;
	}
	if (dag_h->status != rf_enable) {
		printf("INVALID DAG: not enabled\n");
		retcode = 1;
		goto validate_dag_bad;
	}
	if (dag_h->numCommits != 0) {
		printf("INVALID DAG: numCommits != 0 (%d)\n", dag_h->numCommits);
		retcode = 1;
		goto validate_dag_bad;
	}
	if (dag_h->numSuccedents != 1) {
		/* currently, all dags must have only one succedent */
		printf("INVALID DAG: numSuccedents !1 (%d)\n", dag_h->numSuccedents);
		retcode = 1;
		goto validate_dag_bad;
	}
	nodecount = rf_AssignNodeNums(dag_h);

	unvisited = dag_h->succedents[0]->visited;

	RF_Malloc(scount, nodecount * sizeof(int), (int *));
	RF_Malloc(acount, nodecount * sizeof(int), (int *));
	RF_Malloc(nodes, nodecount * sizeof(RF_DagNode_t *),
		  (RF_DagNode_t **));
	for (i = 0; i < dag_h->numSuccedents; i++) {
		if ((dag_h->succedents[i]->visited == unvisited)
		    && rf_ValidateBranch(dag_h->succedents[i], scount,
			acount, nodes, unvisited)) {
			retcode = 1;
		}
	}
	/* start at 1 to skip the header node */
	for (i = 1; i < nodecount; i++) {
		if (nodes[i]->commitNode)
			commitNodeCount++;
		if (nodes[i]->doFunc == NULL) {
			printf("INVALID DAG: node %s has an undefined doFunc\n", nodes[i]->name);
			retcode = 1;
			goto validate_dag_out;
		}
		if (nodes[i]->undoFunc == NULL) {
			printf("INVALID DAG: node %s has an undefined doFunc\n", nodes[i]->name);
			retcode = 1;
			goto validate_dag_out;
		}
		if (nodes[i]->numAntecedents != scount[nodes[i]->nodeNum]) {
			printf("INVALID DAG: node %s has %d antecedents but appears as a succedent %d times\n",
			    nodes[i]->name, nodes[i]->numAntecedents, scount[nodes[i]->nodeNum]);
			retcode = 1;
			goto validate_dag_out;
		}
		if (nodes[i]->numSuccedents != acount[nodes[i]->nodeNum]) {
			printf("INVALID DAG: node %s has %d succedents but appears as an antecedent %d times\n",
			    nodes[i]->name, nodes[i]->numSuccedents, acount[nodes[i]->nodeNum]);
			retcode = 1;
			goto validate_dag_out;
		}
	}

	if (dag_h->numCommitNodes != commitNodeCount) {
		printf("INVALID DAG: incorrect commit node count.  hdr->numCommitNodes (%d) found (%d) commit nodes in graph\n",
		    dag_h->numCommitNodes, commitNodeCount);
		retcode = 1;
		goto validate_dag_out;
	}
validate_dag_out:
	RF_Free(scount, nodecount * sizeof(int));
	RF_Free(acount, nodecount * sizeof(int));
	RF_Free(nodes, nodecount * sizeof(RF_DagNode_t *));
	if (retcode)
		rf_PrintDAGList(dag_h);

	if (rf_validateVisitedDebug)
		rf_ValidateVisitedBits(dag_h);

	return (retcode);

validate_dag_bad:
	rf_PrintDAGList(dag_h);
	return (retcode);
}

#endif /* RF_DEBUG_VALIDATE_DAG */

/******************************************************************************
 *
 * misc construction routines
 *
 *****************************************************************************/

void
rf_redirect_asm(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap)
{
	int     ds = (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) ? 1 : 0;
	int     fcol = raidPtr->reconControl->fcol;
	int     scol = raidPtr->reconControl->spareCol;
	RF_PhysDiskAddr_t *pda;

	RF_ASSERT(raidPtr->status == rf_rs_reconstructing);
	for (pda = asmap->physInfo; pda; pda = pda->next) {
		if (pda->col == fcol) {
#if RF_DEBUG_DAG
			if (rf_dagDebug) {
				if (!rf_CheckRUReconstructed(raidPtr->reconControl->reconMap,
					pda->startSector)) {
					RF_PANIC();
				}
			}
#endif
			/* printf("Remapped data for large write\n"); */
			if (ds) {
				raidPtr->Layout.map->MapSector(raidPtr, pda->raidAddress,
				    &pda->col, &pda->startSector, RF_REMAP);
			} else {
				pda->col = scol;
			}
		}
	}
	for (pda = asmap->parityInfo; pda; pda = pda->next) {
		if (pda->col == fcol) {
#if RF_DEBUG_DAG
			if (rf_dagDebug) {
				if (!rf_CheckRUReconstructed(raidPtr->reconControl->reconMap, pda->startSector)) {
					RF_PANIC();
				}
			}
#endif
		}
		if (ds) {
			(raidPtr->Layout.map->MapParity) (raidPtr, pda->raidAddress, &pda->col, &pda->startSector, RF_REMAP);
		} else {
			pda->col = scol;
		}
	}
}


/* this routine allocates read buffers and generates stripe maps for the
 * regions of the array from the start of the stripe to the start of the
 * access, and from the end of the access to the end of the stripe.  It also
 * computes and returns the number of DAG nodes needed to read all this data.
 * Note that this routine does the wrong thing if the access is fully
 * contained within one stripe unit, so we RF_ASSERT against this case at the
 * start.
 *
 * layoutPtr - in: layout information
 * asmap     - in: access stripe map
 * dag_h     - in: header of the dag to create
 * new_asm_h - in: ptr to array of 2 headers.  to be filled in
 * nRodNodes - out: num nodes to be generated to read unaccessed data
 * sosBuffer, eosBuffer - out: pointers to newly allocated buffer
 */
void
rf_MapUnaccessedPortionOfStripe(RF_Raid_t *raidPtr,
				RF_RaidLayout_t *layoutPtr,
				RF_AccessStripeMap_t *asmap,
				RF_DagHeader_t *dag_h,
				RF_AccessStripeMapHeader_t **new_asm_h,
				int *nRodNodes,
				char **sosBuffer, char **eosBuffer,
				RF_AllocListElem_t *allocList)
{
	RF_RaidAddr_t sosRaidAddress, eosRaidAddress;
	RF_SectorNum_t sosNumSector, eosNumSector;

	RF_ASSERT(asmap->numStripeUnitsAccessed > (layoutPtr->numDataCol / 2));
	/* generate an access map for the region of the array from start of
	 * stripe to start of access */
	new_asm_h[0] = new_asm_h[1] = NULL;
	*nRodNodes = 0;
	if (!rf_RaidAddressStripeAligned(layoutPtr, asmap->raidAddress)) {
		sosRaidAddress = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
		sosNumSector = asmap->raidAddress - sosRaidAddress;
		*sosBuffer = rf_AllocStripeBuffer(raidPtr, dag_h, rf_RaidAddressToByte(raidPtr, sosNumSector));
		new_asm_h[0] = rf_MapAccess(raidPtr, sosRaidAddress, sosNumSector, *sosBuffer, RF_DONT_REMAP);
		new_asm_h[0]->next = dag_h->asmList;
		dag_h->asmList = new_asm_h[0];
		*nRodNodes += new_asm_h[0]->stripeMap->numStripeUnitsAccessed;

		RF_ASSERT(new_asm_h[0]->stripeMap->next == NULL);
		/* we're totally within one stripe here */
		if (asmap->flags & RF_ASM_REDIR_LARGE_WRITE)
			rf_redirect_asm(raidPtr, new_asm_h[0]->stripeMap);
	}
	/* generate an access map for the region of the array from end of
	 * access to end of stripe */
	if (!rf_RaidAddressStripeAligned(layoutPtr, asmap->endRaidAddress)) {
		eosRaidAddress = asmap->endRaidAddress;
		eosNumSector = rf_RaidAddressOfNextStripeBoundary(layoutPtr, eosRaidAddress) - eosRaidAddress;
		*eosBuffer = rf_AllocStripeBuffer(raidPtr, dag_h, rf_RaidAddressToByte(raidPtr, eosNumSector));
		new_asm_h[1] = rf_MapAccess(raidPtr, eosRaidAddress, eosNumSector, *eosBuffer, RF_DONT_REMAP);
		new_asm_h[1]->next = dag_h->asmList;
		dag_h->asmList = new_asm_h[1];
		*nRodNodes += new_asm_h[1]->stripeMap->numStripeUnitsAccessed;

		RF_ASSERT(new_asm_h[1]->stripeMap->next == NULL);
		/* we're totally within one stripe here */
		if (asmap->flags & RF_ASM_REDIR_LARGE_WRITE)
			rf_redirect_asm(raidPtr, new_asm_h[1]->stripeMap);
	}
}



/* returns non-zero if the indicated ranges of stripe unit offsets overlap */
int
rf_PDAOverlap(RF_RaidLayout_t *layoutPtr,
	      RF_PhysDiskAddr_t *src, RF_PhysDiskAddr_t *dest)
{
	RF_SectorNum_t soffs = rf_StripeUnitOffset(layoutPtr, src->startSector);
	RF_SectorNum_t doffs = rf_StripeUnitOffset(layoutPtr, dest->startSector);
	/* use -1 to be sure we stay within SU */
	RF_SectorNum_t send = rf_StripeUnitOffset(layoutPtr, src->startSector + src->numSector - 1);
	RF_SectorNum_t dend = rf_StripeUnitOffset(layoutPtr, dest->startSector + dest->numSector - 1);
	return ((RF_MAX(soffs, doffs) <= RF_MIN(send, dend)) ? 1 : 0);
}


/* GenerateFailedAccessASMs
 *
 * this routine figures out what portion of the stripe needs to be read
 * to effect the degraded read or write operation.  It's primary function
 * is to identify everything required to recover the data, and then
 * eliminate anything that is already being accessed by the user.
 *
 * The main result is two new ASMs, one for the region from the start of the
 * stripe to the start of the access, and one for the region from the end of
 * the access to the end of the stripe.  These ASMs describe everything that
 * needs to be read to effect the degraded access.  Other results are:
 *    nXorBufs -- the total number of buffers that need to be XORed together to
 *                recover the lost data,
 *    rpBufPtr -- ptr to a newly-allocated buffer to hold the parity.  If NULL
 *                at entry, not allocated.
 *    overlappingPDAs --
 *                describes which of the non-failed PDAs in the user access
 *                overlap data that needs to be read to effect recovery.
 *                overlappingPDAs[i]==1 if and only if, neglecting the failed
 *                PDA, the ith pda in the input asm overlaps data that needs
 *                to be read for recovery.
 */
 /* in: asm - ASM for the actual access, one stripe only */
 /* in: failedPDA - which component of the access has failed */
 /* in: dag_h - header of the DAG we're going to create */
 /* out: new_asm_h - the two new ASMs */
 /* out: nXorBufs - the total number of xor bufs required */
 /* out: rpBufPtr - a buffer for the parity read */
void
rf_GenerateFailedAccessASMs(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
			    RF_PhysDiskAddr_t *failedPDA,
			    RF_DagHeader_t *dag_h,
			    RF_AccessStripeMapHeader_t **new_asm_h,
			    int *nXorBufs, char **rpBufPtr,
			    char *overlappingPDAs,
			    RF_AllocListElem_t *allocList)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);

	/* s=start, e=end, s=stripe, a=access, f=failed, su=stripe unit */
	RF_RaidAddr_t sosAddr, sosEndAddr, eosStartAddr, eosAddr;
	RF_PhysDiskAddr_t *pda;
	int     foundit, i;

	foundit = 0;
	/* first compute the following raid addresses: start of stripe,
	 * (sosAddr) MIN(start of access, start of failed SU),   (sosEndAddr)
	 * MAX(end of access, end of failed SU),       (eosStartAddr) end of
	 * stripe (i.e. start of next stripe)   (eosAddr) */
	sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
	sosEndAddr = RF_MIN(asmap->raidAddress, rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, failedPDA->raidAddress));
	eosStartAddr = RF_MAX(asmap->endRaidAddress, rf_RaidAddressOfNextStripeUnitBoundary(layoutPtr, failedPDA->raidAddress));
	eosAddr = rf_RaidAddressOfNextStripeBoundary(layoutPtr, asmap->raidAddress);

	/* now generate access stripe maps for each of the above regions of
	 * the stripe.  Use a dummy (NULL) buf ptr for now */

	new_asm_h[0] = (sosAddr != sosEndAddr) ? rf_MapAccess(raidPtr, sosAddr, sosEndAddr - sosAddr, NULL, RF_DONT_REMAP) : NULL;
	new_asm_h[1] = (eosStartAddr != eosAddr) ? rf_MapAccess(raidPtr, eosStartAddr, eosAddr - eosStartAddr, NULL, RF_DONT_REMAP) : NULL;

	/* walk through the PDAs and range-restrict each SU to the region of
	 * the SU touched on the failed PDA.  also compute total data buffer
	 * space requirements in this step.  Ignore the parity for now. */
	/* Also count nodes to find out how many bufs need to be xored together */
	(*nXorBufs) = 1;	/* in read case, 1 is for parity.  In write
				 * case, 1 is for failed data */

	if (new_asm_h[0]) {
		new_asm_h[0]->next = dag_h->asmList;
		dag_h->asmList = new_asm_h[0];
		for (pda = new_asm_h[0]->stripeMap->physInfo; pda; pda = pda->next) {
			rf_RangeRestrictPDA(raidPtr, failedPDA, pda, RF_RESTRICT_NOBUFFER, 0);
			pda->bufPtr = rf_AllocBuffer(raidPtr, dag_h, pda->numSector << raidPtr->logBytesPerSector);
		}
		(*nXorBufs) += new_asm_h[0]->stripeMap->numStripeUnitsAccessed;
	}
	if (new_asm_h[1]) {
		new_asm_h[1]->next = dag_h->asmList;
		dag_h->asmList = new_asm_h[1];
		for (pda = new_asm_h[1]->stripeMap->physInfo; pda; pda = pda->next) {
			rf_RangeRestrictPDA(raidPtr, failedPDA, pda, RF_RESTRICT_NOBUFFER, 0);
			pda->bufPtr = rf_AllocBuffer(raidPtr, dag_h, pda->numSector << raidPtr->logBytesPerSector);
		}
		(*nXorBufs) += new_asm_h[1]->stripeMap->numStripeUnitsAccessed;
	}

	/* allocate a buffer for parity */
	if (rpBufPtr)
		*rpBufPtr = rf_AllocBuffer(raidPtr, dag_h, failedPDA->numSector << raidPtr->logBytesPerSector);

	/* the last step is to figure out how many more distinct buffers need
	 * to get xor'd to produce the missing unit.  there's one for each
	 * user-data read node that overlaps the portion of the failed unit
	 * being accessed */

	for (foundit = i = 0, pda = asmap->physInfo; pda; i++, pda = pda->next) {
		if (pda == failedPDA) {
			i--;
			foundit = 1;
			continue;
		}
		if (rf_PDAOverlap(layoutPtr, pda, failedPDA)) {
			overlappingPDAs[i] = 1;
			(*nXorBufs)++;
		}
	}
	if (!foundit) {
		RF_ERRORMSG("GenerateFailedAccessASMs: did not find failedPDA in asm list\n");
		RF_ASSERT(0);
	}
#if RF_DEBUG_DAG
	if (rf_degDagDebug) {
		if (new_asm_h[0]) {
			printf("First asm:\n");
			rf_PrintFullAccessStripeMap(new_asm_h[0], 1);
		}
		if (new_asm_h[1]) {
			printf("Second asm:\n");
			rf_PrintFullAccessStripeMap(new_asm_h[1], 1);
		}
	}
#endif
}


/* adjusts the offset and number of sectors in the destination pda so that
 * it covers at most the region of the SU covered by the source PDA.  This
 * is exclusively a restriction:  the number of sectors indicated by the
 * target PDA can only shrink.
 *
 * For example:  s = sectors within SU indicated by source PDA
 *               d = sectors within SU indicated by dest PDA
 *               r = results, stored in dest PDA
 *
 * |--------------- one stripe unit ---------------------|
 * |           sssssssssssssssssssssssssssssssss         |
 * |    ddddddddddddddddddddddddddddddddddddddddddddd    |
 * |           rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr         |
 *
 * Another example:
 *
 * |--------------- one stripe unit ---------------------|
 * |           sssssssssssssssssssssssssssssssss         |
 * |    ddddddddddddddddddddddd                          |
 * |           rrrrrrrrrrrrrrrr                          |
 *
 */
void
rf_RangeRestrictPDA(RF_Raid_t *raidPtr, RF_PhysDiskAddr_t *src,
		    RF_PhysDiskAddr_t *dest, int dobuffer, int doraidaddr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_SectorNum_t soffs = rf_StripeUnitOffset(layoutPtr, src->startSector);
	RF_SectorNum_t doffs = rf_StripeUnitOffset(layoutPtr, dest->startSector);
	RF_SectorNum_t send = rf_StripeUnitOffset(layoutPtr, src->startSector + src->numSector - 1);	/* use -1 to be sure we
													 * stay within SU */
	RF_SectorNum_t dend = rf_StripeUnitOffset(layoutPtr, dest->startSector + dest->numSector - 1);
	RF_SectorNum_t subAddr = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, dest->startSector);	/* stripe unit boundary */

	dest->startSector = subAddr + RF_MAX(soffs, doffs);
	dest->numSector = subAddr + RF_MIN(send, dend) + 1 - dest->startSector;

	if (dobuffer)
		dest->bufPtr = (char *)(dest->bufPtr) + ((soffs > doffs) ? rf_RaidAddressToByte(raidPtr, soffs - doffs) : 0);
	if (doraidaddr) {
		dest->raidAddress = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, dest->raidAddress) +
		    rf_StripeUnitOffset(layoutPtr, dest->startSector);
	}
}

#if (RF_INCLUDE_CHAINDECLUSTER > 0)

/*
 * Want the highest of these primes to be the largest one
 * less than the max expected number of columns (won't hurt
 * to be too small or too large, but won't be optimal, either)
 * --jimz
 */
#define NLOWPRIMES 8
static int lowprimes[NLOWPRIMES] = {2, 3, 5, 7, 11, 13, 17, 19};
/*****************************************************************************
 * compute the workload shift factor.  (chained declustering)
 *
 * return nonzero if access should shift to secondary, otherwise,
 * access is to primary
 *****************************************************************************/
int
rf_compute_workload_shift(RF_Raid_t *raidPtr, RF_PhysDiskAddr_t *pda)
{
	/*
         * variables:
         *  d   = column of disk containing primary
         *  f   = column of failed disk
         *  n   = number of disks in array
         *  sd  = "shift distance" (number of columns that d is to the right of f)
         *  v   = numerator of redirection ratio
         *  k   = denominator of redirection ratio
         */
	RF_RowCol_t d, f, sd, n;
	int     k, v, ret, i;

	n = raidPtr->numCol;

	/* assign column of primary copy to d */
	d = pda->col;

	/* assign column of dead disk to f */
	for (f = 0; ((!RF_DEAD_DISK(raidPtr->Disks[f].status)) && (f < n)); f++);

	RF_ASSERT(f < n);
	RF_ASSERT(f != d);

	sd = (f > d) ? (n + d - f) : (d - f);
	RF_ASSERT(sd < n);

	/*
         * v of every k accesses should be redirected
         *
         * v/k := (n-1-sd)/(n-1)
         */
	v = (n - 1 - sd);
	k = (n - 1);

#if 1
	/*
         * XXX
         * Is this worth it?
         *
         * Now reduce the fraction, by repeatedly factoring
         * out primes (just like they teach in elementary school!)
         */
	for (i = 0; i < NLOWPRIMES; i++) {
		if (lowprimes[i] > v)
			break;
		while (((v % lowprimes[i]) == 0) && ((k % lowprimes[i]) == 0)) {
			v /= lowprimes[i];
			k /= lowprimes[i];
		}
	}
#endif

	raidPtr->hist_diskreq[d]++;
	if (raidPtr->hist_diskreq[d] > v) {
		ret = 0;	/* do not redirect */
	} else {
		ret = 1;	/* redirect */
	}

#if 0
	printf("d=%d f=%d sd=%d v=%d k=%d ret=%d h=%d\n", d, f, sd, v, k, ret,
	    raidPtr->hist_diskreq[d]);
#endif

	if (raidPtr->hist_diskreq[d] >= k) {
		/* reset counter */
		raidPtr->hist_diskreq[d] = 0;
	}
	return (ret);
}
#endif /* (RF_INCLUDE_CHAINDECLUSTER > 0) */

/*
 * Disk selection routines
 */

/*
 * Selects the disk with the shortest queue from a mirror pair.
 * Both the disk I/Os queued in RAIDframe as well as those at the physical
 * disk are counted as members of the "queue"
 */
void
rf_SelectMirrorDiskIdle(RF_DagNode_t * node)
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->dagHdr->raidPtr;
	RF_RowCol_t colData, colMirror;
	int     dataQueueLength, mirrorQueueLength, usemirror;
	RF_PhysDiskAddr_t *data_pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	RF_PhysDiskAddr_t *mirror_pda = (RF_PhysDiskAddr_t *) node->params[4].p;
	RF_PhysDiskAddr_t *tmp_pda;
	RF_RaidDisk_t *disks = raidPtr->Disks;
	RF_DiskQueue_t *dqs = raidPtr->Queues, *dataQueue, *mirrorQueue;

	/* return the [row col] of the disk with the shortest queue */
	colData = data_pda->col;
	colMirror = mirror_pda->col;
	dataQueue = &(dqs[colData]);
	mirrorQueue = &(dqs[colMirror]);

#ifdef RF_LOCK_QUEUES_TO_READ_LEN
	RF_LOCK_QUEUE_MUTEX(dataQueue, "SelectMirrorDiskIdle");
#endif				/* RF_LOCK_QUEUES_TO_READ_LEN */
	dataQueueLength = dataQueue->queueLength + dataQueue->numOutstanding;
#ifdef RF_LOCK_QUEUES_TO_READ_LEN
	RF_UNLOCK_QUEUE_MUTEX(dataQueue, "SelectMirrorDiskIdle");
	RF_LOCK_QUEUE_MUTEX(mirrorQueue, "SelectMirrorDiskIdle");
#endif				/* RF_LOCK_QUEUES_TO_READ_LEN */
	mirrorQueueLength = mirrorQueue->queueLength + mirrorQueue->numOutstanding;
#ifdef RF_LOCK_QUEUES_TO_READ_LEN
	RF_UNLOCK_QUEUE_MUTEX(mirrorQueue, "SelectMirrorDiskIdle");
#endif				/* RF_LOCK_QUEUES_TO_READ_LEN */

	usemirror = 0;
	if (RF_DEAD_DISK(disks[colMirror].status)) {
		usemirror = 0;
	} else
		if (RF_DEAD_DISK(disks[colData].status)) {
			usemirror = 1;
		} else
			if (raidPtr->parity_good == RF_RAID_DIRTY) {
				/* Trust only the main disk */
				usemirror = 0;
			} else
				if (dataQueueLength < mirrorQueueLength) {
					usemirror = 0;
				} else
					if (mirrorQueueLength < dataQueueLength) {
						usemirror = 1;
					} else {
						/* queues are equal length. attempt
						 * cleverness. */
						if (SNUM_DIFF(dataQueue->last_deq_sector, data_pda->startSector)
						    <= SNUM_DIFF(mirrorQueue->last_deq_sector, mirror_pda->startSector)) {
							usemirror = 0;
						} else {
							usemirror = 1;
						}
					}

	if (usemirror) {
		/* use mirror (parity) disk, swap params 0 & 4 */
		tmp_pda = data_pda;
		node->params[0].p = mirror_pda;
		node->params[4].p = tmp_pda;
	} else {
		/* use data disk, leave param 0 unchanged */
	}
	/* printf("dataQueueLength %d, mirrorQueueLength
	 * %d\n",dataQueueLength, mirrorQueueLength); */
}
#if (RF_INCLUDE_CHAINDECLUSTER > 0) || (RF_INCLUDE_INTERDECLUSTER > 0) || (RF_DEBUG_VALIDATE_DAG > 0)
/*
 * Do simple partitioning. This assumes that
 * the data and parity disks are laid out identically.
 */
void
rf_SelectMirrorDiskPartition(RF_DagNode_t * node)
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->dagHdr->raidPtr;
	RF_RowCol_t colData, colMirror;
	RF_PhysDiskAddr_t *data_pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	RF_PhysDiskAddr_t *mirror_pda = (RF_PhysDiskAddr_t *) node->params[4].p;
	RF_PhysDiskAddr_t *tmp_pda;
	RF_RaidDisk_t *disks = raidPtr->Disks;
	int     usemirror;

	/* return the [row col] of the disk with the shortest queue */
	colData = data_pda->col;
	colMirror = mirror_pda->col;

	usemirror = 0;
	if (RF_DEAD_DISK(disks[colMirror].status)) {
		usemirror = 0;
	} else
		if (RF_DEAD_DISK(disks[colData].status)) {
			usemirror = 1;
		} else
			if (raidPtr->parity_good == RF_RAID_DIRTY) {
				/* Trust only the main disk */
				usemirror = 0;
			} else
				if (data_pda->startSector <
				    (disks[colData].numBlocks / 2)) {
					usemirror = 0;
				} else {
					usemirror = 1;
				}

	if (usemirror) {
		/* use mirror (parity) disk, swap params 0 & 4 */
		tmp_pda = data_pda;
		node->params[0].p = mirror_pda;
		node->params[4].p = tmp_pda;
	} else {
		/* use data disk, leave param 0 unchanged */
	}
}
#endif
