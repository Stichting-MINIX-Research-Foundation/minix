/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

static char const n8_id[] = "$Id: QMgrInit.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file QMgrInit.c
 *  @brief This file contains the hardware independent initialization code
 *  for the NetOctave queue structures.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/15/03 brr   Do not include obsolete driver_api.h.
 * 02/20/03 brr   Eliminated all references to shared memory.
 * 11/25/02 brr   Removed the updateAllSem and made initialization/removal
 *                more consistant.
 * 06/05/02 brr   Init waitQueues & moved N8_QMgrQueryStatistics to QMUtil.c
 * 05/30/02 brr   Removed initialization of obsolete fields in queue struct.
 * 05/02/02 brr   Added requestsPreempted stat.
 * 05/17/02 brr   Removed references to hardwareAccessSem.
 * 05/02/02 brr   Moved all SKS initialization to SKS_Init.
 * 05/01/02 brr   Initialize sizeMask & haltingMask.
 * 03/27/02 brr   Removed references to driverHandle.
 * 03/21/02 mmd   Added N8_QMgrQueryStatistics.
 * 03/07/02 brr   Removed dependencies on n8_enum.
 * 03/01/02 brr   Removed obsolete references to driver handle and process
 *                initialization semaphore. Changed queue semaphore to a
 *                kernel type semaphore.
 * 02/18/02 brr   Removed obsolete context memory initialization.
 * 02/19/02 msz   No more randomParmSem.
 * 02/13/02 brr   Moved several functions from QMUtil to here since they are
 *                needed by the driver.
 * 01/22/02 spm   Added creation of SKS semaphore for PK.
 * 01/17/02 brr   Original version.
 ****************************************************************************/

#include "QMgrInit.h"
#include "n8_malloc_common.h"
#include "n8_enqueue_common.h"
#include "n8_OS_intf.h"
#include "helper.h"
#include "n8_driver_main.h"
#include "QMQueue.h"
#include "QMUtil.h"
#include "n8_driver_parms.h"

/*****************************************************************************
 * GLOBALS
 *****************************************************************************/
/** @ingroup QMgr
 * @brief These are the globals used by the open and intialization code in
 *        the QMgr .
 *
 * Internal Globals are to be used only within this file.
 * @par Internal:
 *    queueTable_g  The global queue table which contains the state needed to
 *                  maintain each queue.  There is one queue for each execution
 *                  unit, and one execution unit for each FPGA board.
 *    
 *****************************************************************************/

/* The one and only global queue structure */
struct QueueControlTable_s  queueTable_g = {0, 0, 0};

/* The queue control tables.  They are now based on the maximum number  */
/* of devices that can be present in the system.                        */
static
QueueControlSet_t QueueControlSets_g[DEF_MAX_SIMON_INSTANCES];

extern NspInstance_t NSPDeviceTable_g [];
extern n8_atomic_t  requestsComplete;

/*****************************************************************************
 * QMgr_init_control_sets
 *****************************************************************************/
/** @ingroup QMgr
 * @brief This function sets initial values for variables in the control set
 *  structures.
 *
 *  The current fields being set are driverHandle, type and queue_initialized.
 *  All of the rest of the fields will be zero since our N8_MALLOC uses
 *  calloc().
 * 
 * @param table_p RW: Pointer to the table to initialize.  Right now there is 
 *                    only one.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_PARAMETER:  The input pointer was NULL.
 *
 * @par Errors:
 *    
 *    This function will return N8_INVALID_PARAMETER if the input parameter
 *    is NULL.
 *
 * @par Locks:
 *    This function assumes the process initialization lock is held.
 *   
 * @par Assumptions:
 *    Unless table_p is NULL, we assume it is a "reasonable" pointer.
 *    Shared per execution unit data has already been allocated.
 *
 *****************************************************************************/


static
void
QMgr_init_control_set(QueueControlTable_t *table_p, int i)
{
   int                          j;
   QueueControl_t              *queue_p;
   QueueControl_t              *queuePK_p;
   QueueControl_t              *queueRN_p;
   QueueControl_t              *queueEA_p;
   NspInstance_t               *NSPData_p;


   n8_atomic_set(requestsComplete, 0);

   queuePK_p = &table_p->controlSets_p[i][N8_PKP];
   queueRN_p = &table_p->controlSets_p[i][N8_RNG];
   queueEA_p = &table_p->controlSets_p[i][N8_EA];

   /* Initialize data that is generic across queue type. */
   for (j = 0; j < N8_NUM_COMPONENTS; j++)
   {
      queue_p = &table_p->controlSets_p[i][j];
      queue_p->hardwareType           = N8_NSP2000_HW;
      queue_p->chip                   = i;
   }

   queuePK_p->type = N8_PKP;
   queueRN_p->type = N8_RNG;
   queueEA_p->type = N8_EA;

   /* Initialize the semaphores */                         
   for (j = 0; j < N8_NUM_COMPONENTS; j++)
   {
      queue_p = &table_p->controlSets_p[i][j];
      N8_AtomicLockInit(queue_p->queueControlSem);
   }


   /* FETCH DEVICE RESOURCE INFORMATION FROM THE DRIVER */
   NSPData_p = &NSPDeviceTable_g[i];

   /* Setup the register and queue pointers for this unit */
   queueEA_p->n8reg_p = (N8CommonRegs_t *)((int)NSPData_p->NSPregs_p + EA_REG_OFFSET);
   queueEA_p->cmdQueVirtPtr = (unsigned long)NSPData_p->EAqueue_p;
   queueEA_p->EAq_head_p = (EA_CMD_BLOCK_t *)queueEA_p->cmdQueVirtPtr;
   queueEA_p->sizeOfQueue = NSPData_p->EAqueue_size;
   queueEA_p->sizeMask = NSPData_p->EAqueue_size-1;

   /* Setup the command entry size */
   queueEA_p->cmdEntrySize = sizeof(EA_CMD_BLOCK_t);

   /* Setup the register and queue pointers for this unit */
   queuePK_p->n8reg_p = (N8CommonRegs_t *)((int)NSPData_p->NSPregs_p + PKE_REG_OFFSET);
   queuePK_p->cmdQueVirtPtr = (unsigned long)NSPData_p->PKqueue_p;
   queuePK_p->PKq_head_p = (PK_CMD_BLOCK_t *)queuePK_p->cmdQueVirtPtr;
   queuePK_p->sizeOfQueue = NSPData_p->PKqueue_size;
   queuePK_p->sizeMask = NSPData_p->PKqueue_size-1;

   /* Setup the command entry size */
   queuePK_p->cmdEntrySize = sizeof(PK_CMD_BLOCK_t);


   /* Setup the register and queue pointers for this unit */
   queueRN_p->rngReg_p = (N8RNGRegs_t *)((int)NSPData_p->NSPregs_p + RNG_REG_OFFSET);
   queueRN_p->cmdQueVirtPtr = (unsigned long)NSPData_p->RNqueue_p;
   queueRN_p->sizeOfQueue = NSPData_p->RNqueue_size;
   queueRN_p->sizeMask = NSPData_p->RNqueue_size-1;

   /* Initialize the wait queues */                         
   for (j = 0; j < MAX_API_REQS; j++)
   {
      N8_InitWaitQueue(&queueEA_p->waitQueue[j]);
      N8_InitWaitQueue(&queuePK_p->waitQueue[j]);
   }

   /* Retrieve the read index location from the driver's data structure */
   queueEA_p->readIndex_p = &NSPData_p->EAreadIndex;
   queuePK_p->readIndex_p = &NSPData_p->PKreadIndex;

} /* QMgr_init_control_set */



/*****************************************************************************
 * QMgrInit
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Initialize the queue structures used by the NSP2000.
 *
 *   This sets up the queue control structures used by the NSP hardware.
 *
 * @param index RO: Index of a global hardware instance.
 *
 * @par Externals:
 *    queueTable_g: RW: The global queue control table.
 *
 * @return 
 *    N8_STATUS_OK: The function succeeded.
 *    N8_MALLOC_FAILED: Memory allocation failure.
 *
 * @par Errors:
 *    All error codes are returned including N8_MALLOC_FAILED if the function
 *    fails to allocate space for the global queue control table, and whatever
 *    error conditions are returned by the functions listed above.
 *    
 * @par Locks:
 *    This function starts by grabbing the process initialization lock and 
 *    holds it until the end of the function.
 *   
 * @par Assumptions:
 *    The call of this function IS NOT holding the process initialization
 *    lock.
 *    Devices are all NSPs.
 *    This is called exactly once per hardware instance.
 *
 *****************************************************************************/

N8_Status_t QMgrInit(uint32_t index)
{

   N8_Status_t        results = N8_STATUS_OK;


   /* Set up the pointer to the control sets in the global queue table. */
   /* Allocate controls sets, the size is based on the maximum number   */
   /* of possible devices in the system.                                */
   if ( queueTable_g.controlSets_p == NULL )
   {
      memset(&QueueControlSets_g, 0, sizeof(QueueControlSets_g));
      queueTable_g.controlSets_p = QueueControlSets_g;
      /* Set the current control set to be the first control set.          */
      queueTable_g.currentSet = 0;
   }

   /* Set the number of control sets */
   queueTable_g.nControlSets++;

   /* Initialize the control set for our hardware */ 
   QMgr_init_control_set(&queueTable_g,index);

   return (results);

} /* QMgrInit */

/*****************************************************************************
 * QMgrRemove
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Deallocate the queue structures used by the NSP2000.
 *
 *   This sets deallocates the queue control structures used by the NSP hardware.
 *
 * @param index RO: Index of a global hardware instance.
 *
 * @par Externals:
 *    queueTable_g: RW: The global queue control table.
 *
 * @return 
 *    N8_STATUS_OK: The function succeeded.
 *
 * @par Errors:
 *    
 * @par Locks:
 *   
 * @par Assumptions:
 *    The call is called by the driver's exit handler and will delete the
 *    QMgr's resources. It therefore cannot obtain any locks and assumes
 *    the caller has ensured it is safe to deallocate these resources.
 *
 *****************************************************************************/

N8_Status_t QMgrRemove(uint32_t index)
{
   int                          j;
   QueueControl_t              *queuePK_p;
   QueueControl_t              *queueRN_p;
   QueueControl_t              *queueEA_p;

   queuePK_p = &queueTable_g.controlSets_p[index][N8_PKP];
   queueRN_p = &queueTable_g.controlSets_p[index][N8_RNG];
   queueEA_p = &queueTable_g.controlSets_p[index][N8_EA];

   /* Delete the queue control semaphores. */
   N8_AtomicLockDel(queuePK_p->queueControlSem);
   N8_AtomicLockDel(queueRN_p->queueControlSem);
   N8_AtomicLockDel(queueEA_p->queueControlSem);

   /* Remove the wait queues */                         
   for (j = 0; j < MAX_API_REQS; j++)
   {
      N8_DelWaitQueue(queuePK_p->waitQueue[j]);
      N8_DelWaitQueue(queueEA_p->waitQueue[j]);
   }
   return N8_STATUS_OK;

}
