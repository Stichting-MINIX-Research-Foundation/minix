/*	$NetBSD: rf_dag.h,v 1.19 2005/12/11 12:23:37 christos Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II, Mark Holland
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

/****************************************************************************
 *                                                                          *
 * dag.h -- header file for DAG-related data structures                     *
 *                                                                          *
 ****************************************************************************/

#ifndef _RF__RF_DAG_H_
#define _RF__RF_DAG_H_

#include <dev/raidframe/raidframevar.h>

#include "rf_threadstuff.h"
#include "rf_alloclist.h"
#include "rf_stripelocks.h"
#include "rf_layout.h"
#include "rf_dagflags.h"
#include "rf_acctrace.h"
#include "rf_desc.h"

#define RF_THREAD_CONTEXT   0	/* we were invoked from thread context */
#define RF_INTR_CONTEXT     1	/* we were invoked from interrupt context */
#define RF_MAX_ANTECEDENTS 20	/* max num of antecedents a node may posses */

#include <sys/buf.h>

struct RF_PropHeader_s {	/* structure for propagation of results */
	int     paramNum;	/* to parameter # paramNum */
	RF_PropHeader_t *next;	/* linked list for multiple results/params */
};

typedef enum RF_NodeStatus_e {
	rf_wait,		/* node is waiting to be executed */
	rf_fired,		/* node is currently executing its do function */
	rf_good,		/* node successfully completed execution of
				 * its do function */
	rf_bad,			/* node failed to successfully execute its do
				 * function */
	rf_skipped,		/* not used anymore, used to imply a node was
				 * not executed */
	rf_recover,		/* node is currently executing its undo
				 * function */
	rf_panic,		/* node failed to successfully execute its
				 * undo function */
	rf_undone		/* node successfully executed its undo
				 * function */
}       RF_NodeStatus_t;
/*
 * These were used to control skipping a node.
 * Now, these are only used as comments.
 */
typedef enum RF_AntecedentType_e {
	rf_trueData,
	rf_antiData,
	rf_outputData,
	rf_control
}       RF_AntecedentType_t;
#define RF_DAG_PTRCACHESIZE   40
#define RF_DAG_PARAMCACHESIZE 12

typedef RF_uint8 RF_DagNodeFlags_t;

struct RF_DagNode_s {
	RF_NodeStatus_t status;	/* current status of this node */
	int     (*doFunc) (RF_DagNode_t *);	/* normal function */
	int     (*undoFunc) (RF_DagNode_t *);	/* func to remove effect of
						 * doFunc */
	int     (*wakeFunc) (RF_DagNode_t *, int status);	/* func called when the
								 * node completes an I/O */
	int     numParams;	/* number of parameters required by *funcPtr */
	int     numResults;	/* number of results produced by *funcPtr */
	int     numAntecedents;	/* number of antecedents */
	int     numAntDone;	/* number of antecedents which have finished */
	int     numSuccedents;	/* number of succedents */
	int     numSuccFired;	/* incremented when a succedent is fired
				 * during forward execution */
	int     numSuccDone;	/* incremented when a succedent finishes
				 * during rollBackward */
	int     commitNode;	/* boolean flag - if true, this is a commit
				 * node */
	RF_DagNode_t **succedents;	/* succedents, array size
					 * numSuccedents */
	RF_DagNode_t **antecedents;	/* antecedents, array size
					 * numAntecedents */
	RF_AntecedentType_t antType[RF_MAX_ANTECEDENTS];	/* type of each
								 * antecedent */
	void  **results;	/* array of results produced by *funcPtr */
	RF_DagParam_t *params;	/* array of parameters required by *funcPtr */
	RF_PropHeader_t **propList;	/* propagation list, size
					 * numSuccedents */
	RF_DagHeader_t *dagHdr;	/* ptr to head of dag containing this node */
	void   *dagFuncData;	/* dag execution func uses this for whatever
				 * it wants */
	RF_DagNode_t *next;     /* next in terms of propagating results */
	RF_DagNode_t *list_next; /* next in the list of DAG nodes for this DAG */
	int     nodeNum;	/* used by PrintDAG for debug only */
	int     visited;	/* used to avoid re-visiting nodes on DAG
				 * walks */
	/* ANY CODE THAT USES THIS FIELD MUST MAINTAIN THE PROPERTY THAT AFTER
	 * IT FINISHES, ALL VISITED FLAGS IN THE DAG ARE IDENTICAL */
	const char   *name;	/* debug only */
	RF_DagNodeFlags_t flags;/* see below */
	RF_DagNode_t *big_dag_ptrs;  /* used in cases where the cache below isn't big enough */
	RF_DagParam_t *big_dag_params; /* used when the cache below isn't big enough */
	RF_DagNode_t *dag_ptrs[RF_DAG_PTRCACHESIZE];	/* cache for performance */
	RF_DagParam_t dag_params[RF_DAG_PARAMCACHESIZE];	/* cache for performance */
};
/*
 * Bit values for flags field of RF_DagNode_t
 */
#define RF_DAGNODE_FLAG_NONE  0x00
#define RF_DAGNODE_FLAG_YIELD 0x01	/* in the kernel, yield the processor
					 * before firing this node */

/* enable - DAG ready for normal execution, no errors encountered
 * rollForward - DAG encountered an error after commit point, rolling forward
 * rollBackward - DAG encountered an error prior to commit point, rolling backward
 */
typedef enum RF_DagStatus_e {
	rf_enable,
	rf_rollForward,
	rf_rollBackward
}       RF_DagStatus_t;
#define RF_MAX_HDR_SUCC 1

struct RF_DagHeader_s {
	RF_DagStatus_t status;	/* status of this DAG */
	int     numSuccedents;	/* DAG may be a tree, i.e. may have > 1 root */
	int     numCommitNodes;	/* number of commit nodes in graph */
	int     numCommits;	/* number of commit nodes which have been
				 * fired  */
	RF_DagNode_t *succedents[RF_MAX_HDR_SUCC];	/* array of succedents,
							 * size numSuccedents */
	RF_DagHeader_t *next;	/* ptr to allow a list of dags */
	RF_AllocListElem_t *allocList;	/* ptr to list of ptrs to be freed
					 * prior to freeing DAG */
	RF_AccessStripeMapHeader_t *asmList;	/* list of access stripe maps
						 * to be freed */
	int     nodeNum;	/* used by PrintDAG for debug only */
	int     numNodesCompleted;
#if RF_ACC_TRACE > 0
	RF_AccTraceEntry_t *tracerec;	/* perf mon only */
#endif
	void    (*cbFunc) (void *);	/* function to call when the dag
					 * completes */
	void   *cbArg;		/* argument for cbFunc */
	const char   *creator;	/* name of function used to create this dag */
	RF_DagNode_t *nodes;    /* linked list of nodes used in this DAG */
	RF_PhysDiskAddr_t *pda_cleanup_list; /* for PDAs that can't get
						cleaned up any other way... */
	RF_Raid_t *raidPtr;	/* the descriptor for the RAID device this DAG
				 * is for */
	RF_RaidAccessDesc_t *desc;	/* ptr to descriptor for this access */
	void   *bp;		/* the bp for this I/O passed down from the
				 * file system. ignored outside kernel */
};

struct RF_DagList_s {
	/* common info for a list of dags which will be fired sequentially */
	int     numDags;	/* number of dags in the list */
	int     numDagsFired;	/* number of dags in list which have initiated
				 * execution */
	int     numDagsDone;	/* number of dags in list which have completed
				 * execution */
	RF_DagHeader_t *dags;	/* list of dags */
	RF_RaidAccessDesc_t *desc;	/* ptr to descriptor for this access */
	RF_AccTraceEntry_t tracerec;	/* perf mon info for dags (not user
					 * info) */
	struct RF_DagList_s *next;     /* next DagList, if any */
};

/* convience macro for declaring a create dag function */

#define RF_CREATE_DAG_FUNC_DECL(_name_) \
void _name_ ( \
	RF_Raid_t             *raidPtr, \
	RF_AccessStripeMap_t  *asmap, \
	RF_DagHeader_t        *dag_h, \
	void                  *bp, \
	RF_RaidAccessFlags_t   flags, \
	RF_AllocListElem_t    *allocList)

#endif				/* !_RF__RF_DAG_H_ */
