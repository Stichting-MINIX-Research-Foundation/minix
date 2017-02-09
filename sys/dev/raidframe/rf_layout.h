/*	$NetBSD: rf_layout.h,v 1.17 2007/03/04 06:02:38 christos Exp $	*/
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

/* rf_layout.h -- header file defining layout data structures
 */

#ifndef _RF__RF_LAYOUT_H_
#define _RF__RF_LAYOUT_H_

#include <dev/raidframe/raidframevar.h>
#include "rf_archs.h"
#include "rf_alloclist.h"

/* enables remapping to spare location under dist sparing */
#define RF_REMAP       1
#define RF_DONT_REMAP  0

/*
 * Flags values for RF_AccessStripeMapFlags_t
 */
#define RF_NO_STRIPE_LOCKS   0x0001	/* suppress stripe locks */
#define RF_DISTRIBUTE_SPARE  0x0002	/* distribute spare space in archs
					 * that support it */
#define RF_BD_DECLUSTERED    0x0004	/* declustering uses block designs */

/*************************************************************************
 *
 * this structure forms the layout component of the main Raid
 * structure.  It describes everything needed to define and perform
 * the mapping of logical RAID addresses <-> physical disk addresses.
 *
 *************************************************************************/
struct RF_RaidLayout_s {
	/* configuration parameters */
	RF_SectorCount_t sectorsPerStripeUnit;	/* number of sectors in one
						 * stripe unit */
	RF_StripeCount_t SUsPerPU;	/* stripe units per parity unit */
	RF_StripeCount_t SUsPerRU;	/* stripe units per reconstruction
					 * unit */

	/* redundant-but-useful info computed from the above, used in all
	 * layouts */
	RF_StripeCount_t numStripe;	/* total number of stripes in the
					 * array */
	RF_SectorCount_t dataSectorsPerStripe;
	RF_StripeCount_t dataStripeUnitsPerDisk;
	RF_StripeCount_t numDataCol;	/* number of SUs of data per stripe
					 * (name here is a la RAID4) */
	RF_StripeCount_t numParityCol;	/* number of SUs of parity per stripe.
					 * Always 1 for now */
	RF_StripeCount_t numParityLogCol;	/* number of SUs of parity log
						 * per stripe.  Always 1 for
						 * now */
	RF_StripeCount_t stripeUnitsPerDisk;

	const RF_LayoutSW_t *map;	/* ptr to struct holding mapping fns and
					 * information */
	void   *layoutSpecificInfo;	/* ptr to a structure holding
					 * layout-specific params */
};
/*****************************************************************************************
 *
 * The mapping code returns a pointer to a list of AccessStripeMap structures, which
 * describes all the mapping information about an access.  The list contains one
 * AccessStripeMap structure per stripe touched by the access.  Each element in the list
 * contains a stripe identifier and a pointer to a list of PhysDiskAddr structuress.  Each
 * element in this latter list describes the physical location of a stripe unit accessed
 * within the corresponding stripe.
 *
 ****************************************************************************************/

#define RF_PDA_TYPE_DATA   0
#define RF_PDA_TYPE_PARITY 1
#define RF_PDA_TYPE_Q      2

struct RF_PhysDiskAddr_s {
	RF_RowCol_t col;	/* disk identifier */
	RF_SectorNum_t startSector;	/* sector offset into the disk */
	RF_SectorCount_t numSector;	/* number of sectors accessed */
	int     type;		/* used by higher levels: currently, data,
				 * parity, or q */
	void *bufPtr;		/* pointer to buffer supplying/receiving data */
	RF_RaidAddr_t raidAddress;	/* raid address corresponding to this
					 * physical disk address */
	RF_PhysDiskAddr_t *next;
};
#define RF_MAX_FAILED_PDA RF_MAXCOL

struct RF_AccessStripeMap_s {
	RF_StripeNum_t stripeID;/* the stripe index */
	RF_RaidAddr_t raidAddress;	/* the starting raid address within
					 * this stripe */
	RF_RaidAddr_t endRaidAddress;	/* raid address one sector past the
					 * end of the access */
	RF_SectorCount_t totalSectorsAccessed;	/* total num sectors
						 * identified in physInfo list */
	RF_StripeCount_t numStripeUnitsAccessed;	/* total num elements in
							 * physInfo list */
	int     numDataFailed;	/* number of failed data disks accessed */
	int     numParityFailed;/* number of failed parity disks accessed (0
				 * or 1) */
	int     numQFailed;	/* number of failed Q units accessed (0 or 1) */
	RF_AccessStripeMapFlags_t flags;	/* various flags */
	int     numFailedPDAs;	/* number of failed phys addrs */
	RF_PhysDiskAddr_t *failedPDAs[RF_MAX_FAILED_PDA];	/* array of failed phys
								 * addrs */
	RF_PhysDiskAddr_t *physInfo;	/* a list of PhysDiskAddr structs */
	RF_PhysDiskAddr_t *parityInfo;	/* list of physical addrs for the
					 * parity (P of P + Q ) */
	RF_PhysDiskAddr_t *qInfo;	/* list of physical addrs for the Q of
					 * P + Q */
	RF_LockReqDesc_t lockReqDesc;	/* used for stripe locking */
	RF_AccessStripeMap_t *next;
};
/* flag values */
#define RF_ASM_REDIR_LARGE_WRITE   0x00000001	/* allows large-write creation
						 * code to redirect failed
						 * accs */
#define RF_ASM_BAILOUT_DAG_USED    0x00000002	/* allows us to detect
						 * recursive calls to the
						 * bailout write dag */
#define RF_ASM_FLAGS_LOCK_TRIED    0x00000004	/* we've acquired the lock on
						 * the first parity range in
						 * this parity stripe */
#define RF_ASM_FLAGS_LOCK_TRIED2   0x00000008	/* we've acquired the lock on
						 * the 2nd   parity range in
						 * this parity stripe */
#define RF_ASM_FLAGS_FORCE_TRIED   0x00000010	/* we've done the force-recon
						 * call on this parity stripe */
#define RF_ASM_FLAGS_RECON_BLOCKED 0x00000020	/* we blocked recon => we must
						 * unblock it later */

struct RF_AccessStripeMapHeader_s {
	RF_StripeCount_t numStripes;	/* total number of stripes touched by
					 * this acc */
	RF_AccessStripeMap_t *stripeMap;	/* pointer to the actual map.
						 * Also used for making lists */
	RF_AccessStripeMapHeader_t *next;
};

/* A structure to be used in a linked list to keep track of function pointers. */
typedef struct RF_VoidFunctionPointerListElem_s RF_VoidFunctionPointerListElem_t;
struct RF_VoidFunctionPointerListElem_s {
	RF_VoidFuncPtr fn;
	RF_VoidFunctionPointerListElem_t *next;
};

/* We need something to just be a linked list of anonymous pointers
   to stuff */
typedef struct RF_VoidPointerListElem_s RF_VoidPointerListElem_t;
struct RF_VoidPointerListElem_s {
	void *p;
	RF_VoidPointerListElem_t *next;
};

/* A structure to be used in a linked list to keep track of ASM Headers */
typedef struct RF_ASMHeaderListElem_s RF_ASMHeaderListElem_t;
struct RF_ASMHeaderListElem_s {
	RF_AccessStripeMapHeader_t *asmh;
	RF_ASMHeaderListElem_t *next;
};

/* A structure to keep track of all the data structures associated with
a failed stripe.  Used for constructing the appropriate DAGs in
rf_SelectAlgorithm() in rf_aselect.c */
typedef struct RF_FailedStripe_s RF_FailedStripe_t;
struct RF_FailedStripe_s {
	RF_VoidFunctionPointerListElem_t *vfple;   /* linked list of pointers to DAG creation
						      functions for stripes */
	RF_VoidFunctionPointerListElem_t *bvfple;  /* linked list of poitners to DAG creation
						      functions for blocks */
	RF_ASMHeaderListElem_t *asmh_u;            /* Access Stripe Map Headers for regular
						      stripes */
	RF_ASMHeaderListElem_t *asmh_b;            /* Access Stripe Map Headers used for the
						      block functions */
	RF_FailedStripe_t *next;
};



/*****************************************************************************************
 *
 * various routines mapping addresses in the RAID address space.  These work across
 * all layouts.  DON'T PUT ANY LAYOUT-SPECIFIC CODE HERE.
 *
 ****************************************************************************************/

/* return the identifier of the stripe containing the given address */
#define rf_RaidAddressToStripeID(_layoutPtr_, _addr_) \
  ( ((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) / (_layoutPtr_)->numDataCol )

/* return the raid address of the start of the indicates stripe ID */
#define rf_StripeIDToRaidAddress(_layoutPtr_, _sid_) \
  ( ((_sid_) * (_layoutPtr_)->sectorsPerStripeUnit) * (_layoutPtr_)->numDataCol )

/* return the identifier of the stripe containing the given stripe unit id */
#define rf_StripeUnitIDToStripeID(_layoutPtr_, _addr_) \
  ( (_addr_) / (_layoutPtr_)->numDataCol )

/* return the identifier of the stripe unit containing the given address */
#define rf_RaidAddressToStripeUnitID(_layoutPtr_, _addr_) \
  ( ((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) )

/* return the RAID address of next stripe boundary beyond the given address */
#define rf_RaidAddressOfNextStripeBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->dataSectorsPerStripe)+1) * (_layoutPtr_)->dataSectorsPerStripe )

/* return the RAID address of the start of the stripe containing the given address */
#define rf_RaidAddressOfPrevStripeBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->dataSectorsPerStripe)+0) * (_layoutPtr_)->dataSectorsPerStripe )

/* return the RAID address of next stripe unit boundary beyond the given address */
#define rf_RaidAddressOfNextStripeUnitBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->sectorsPerStripeUnit)+1L)*(_layoutPtr_)->sectorsPerStripeUnit )

/* return the RAID address of the start of the stripe unit containing RAID address _addr_ */
#define rf_RaidAddressOfPrevStripeUnitBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->sectorsPerStripeUnit)+0)*(_layoutPtr_)->sectorsPerStripeUnit )

/* returns the offset into the stripe.  used by RaidAddressStripeAligned */
#define rf_RaidAddressStripeOffset(_layoutPtr_, _addr_) \
  ( (_addr_) % ((_layoutPtr_)->dataSectorsPerStripe) )

/* returns the offset into the stripe unit.  */
#define rf_StripeUnitOffset(_layoutPtr_, _addr_) \
  ( (_addr_) % ((_layoutPtr_)->sectorsPerStripeUnit) )

/* returns nonzero if the given RAID address is stripe-aligned */
#define rf_RaidAddressStripeAligned( __layoutPtr__, __addr__ ) \
  ( rf_RaidAddressStripeOffset(__layoutPtr__, __addr__) == 0 )

/* returns nonzero if the given address is stripe-unit aligned */
#define rf_StripeUnitAligned( __layoutPtr__, __addr__ ) \
  ( rf_StripeUnitOffset(__layoutPtr__, __addr__) == 0 )

/* convert an address expressed in RAID blocks to/from an addr expressed in bytes */
#define rf_RaidAddressToByte(_raidPtr_, _addr_) \
  ( (_addr_) << ( (_raidPtr_)->logBytesPerSector ) )

#define rf_ByteToRaidAddress(_raidPtr_, _addr_) \
  ( (_addr_) >> ( (_raidPtr_)->logBytesPerSector ) )

/* convert a raid address to/from a parity stripe ID.  Conversion to raid address is easy,
 * since we're asking for the address of the first sector in the parity stripe.  Conversion to a
 * parity stripe ID is more complex, since stripes are not contiguously allocated in
 * parity stripes.
 */
#define rf_RaidAddressToParityStripeID(_layoutPtr_, _addr_, _ru_num_) \
  rf_MapStripeIDToParityStripeID( (_layoutPtr_), rf_RaidAddressToStripeID( (_layoutPtr_), (_addr_) ), (_ru_num_) )

#define rf_ParityStripeIDToRaidAddress(_layoutPtr_, _psid_) \
  ( (_psid_) * (_layoutPtr_)->SUsPerPU * (_layoutPtr_)->numDataCol * (_layoutPtr_)->sectorsPerStripeUnit )

const RF_LayoutSW_t *rf_GetLayout(RF_ParityConfig_t parityConfig);
int
rf_ConfigureLayout(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
RF_StripeNum_t
rf_MapStripeIDToParityStripeID(RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID, RF_ReconUnitNum_t * which_ru);

#endif				/* !_RF__RF_LAYOUT_H_ */
