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

static char const n8_id[] = "$Id: QMUtil.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file QMUtil.c
 *  @brief Queue manager utility file.
 *
 * This file contains some utility functions for the queue manager 
 * functionality.  These functions are all generic to the core type
 * (RNG, PKP, EA)
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 04/01/03 brr   Eliminated obslete variable form displayQMgr.
 * 03/20/03 brr   Added displayQMgr function.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 06/05/02 brr   Moved N8_QMgrQueryStatistics here from QMgrInit & removed
 *                obsolete functions.
 * 05/14/02 brr   Removed functions used to process chained requests.
 * 05/01/02 brr   Streamline chained request processing.
 * 04/03/02 brr   Fix compile error when N8DEBUG is defined.
 * 03/27/02 hml   Replaced all of the N8_QueueReturnCodes_t with N8_Status_t.
 * 03/07/02 brr   Perform CheckQueue operation in the IOCTL.
 * 03/06/02 msz   Use subsequent physical address to get virtual address
 *                (Chained subsequent messages in kernel support)
 * 03/01/02 brr   Deleted obsolete function QMgr_get_pseudo_device_handle.
 * 02/26/02 brr   Do not copy finished requests to user space, since they are
 *                now shared buffers.
 * 02/22/02 spm   Fixed compiler warning (Nested comment: Line 336).
 * 02/22/02 spm   Converted printk's to DBG's.
 * 01/19/02 brr   Moved callback processing to N8_EventCheck.
 * 02/19/02 brr   Copy API request back to user space upon completion of a
 *                request submitted from user space.
 * 01/31/02 brr   Removed QMCopy.
 * 01/23/02 msz   Fix for BUG 485, re-arrangement in QMgrFinishRequests.
 * 02/13/02 brr   Moved several function to the driver file n8_queueInit.c
 * 01/16/02 brr   Removed references to obsolete include files.
 * 12/05/01 msz   Moved some common RNQueue, QMQueue routines into here.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 11/05/01 hml   Added the N8_USE_ONE_CHIP #define and some debugging.
 * 10/30/01 dkm   Added N8_INVALID_UNIT error.
 * 10/05/01 msz   Added QMgr_QueueGetChipAndUnit
 * 09/24/01 hml   Converted QMgr_get_valid_unit_num to use N8_Unit_t.
 * 09/20/01 hml   Added QMgr_get_valid_unit_num.
 * 09/06/01 hml   Added QMgr_get_psuedo_device_handle.
 * 08/21/01 hml   Added QMgr_get_num_control_structs.
 * 09/26/01 msz   Added QMgr_QueueInitialized
 * 08/16/01 mmd   Now including n8_driver_api.h instead of simon_driver_api.h.
 *
 * 08/13/01 msz   Added QMgr_get_chip_for_request function.
 * 08/06/01 mel   Deleted include for strings.h.
 * 07/30/01 bac   Added QMCopy function.
 * 06/22/01 hml   Updated documentation and error checking.
 * 06/15/01 hml   Original version.
 *
 ****************************************************************************/

#include "n8_pub_types.h"
#include "n8_common.h"
#include "n8_malloc_common.h"
#include "n8_pub_errors.h"
#include "n8_enqueue_common.h"
#include "QMUtil.h"
#include "QMQueue.h"
#include "RN_Queue.h"
#include "helper.h"
#include "n8_driver_api.h"


/*****************************************************************************
 * QMgr_get_chip_for_request
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Given a component type, this function fetches the next available
 * pointer for the appropriate control structure.
 *
 * @param queue_pp WO: Ptr in which to store the address of the target
 *                     control structure. 
 * @param unit     RO: The execution unit.
 *
 * @par Externals:
 *    queueTable_g:  The global table of queue information.
 *
 * @return 
 *    The queue_pp will contian the pointer to the queue control structure
 *        that is being assigned.
 *    N8_STATUS_OK: The function succeeded. <BR>
 *    N8_INVALID_PARAMETER: The unit is invalid.
 *
 * @par Errors:
 *    Errors are returned form the function. See the return section.
 *
 * @par Assumptions:
 *    The global queue table has been initialized before this function is
 *    called.
 *****************************************************************************/

N8_Status_t
QMgr_get_chip_for_request (QueueControl_t  **queue_pp, 
                           N8_Component_t    unit )
{
   if (unit >= N8_NUM_COMPONENTS)
   {
      /* Requested an invalid chip/unit or passed a null pointer */
      return (N8_INVALID_PARAMETER);
   }                               

   /* Get the current control set */
   *queue_pp = &(queueTable_g.controlSets_p[queueTable_g.currentSet][unit]);

#if defined N8_USE_ONE_CHIP
   /* If the library has been built with N8_USE_ONE_CHIP defined, then 
      set the currentSet to 0.   This is for testing/analyses use only and 
      should NEVER be turned on in normal usage. */
   queueTable_g.currentSet = 0;
#else

   /* Bump to the next control set */
   queueTable_g.currentSet = ( queueTable_g.currentSet + 1 )
                               % queueTable_g.nControlSets;
#endif

   DBG(("QMgr_get_chip_for_request: providing chip %d.\n", 
        (*queue_pp)->chip));
   return N8_STATUS_OK;

} /* QMgr_get_chip_for_request */


/*****************************************************************************
 * QMgr_get_control_struct
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Given a component type and a chip index, this function fetches the 
 * pointer for the appropriate control structure.
 *
 * @param queue_pp WO: Ptr in which to store the address of the target
 *                     control structure. 
 * @param unit     RO: The execution unit.
 * @param chip     RO: The chip identifier.
 *
 * @par Externals:
 *    queueTable_g:  The global table of queue information.
 *
 * @return 
 *    N8_STATUS_OK: The function succeeded. <BR>
 *    N8_INVALID_PARAMETER: The unit or chip is invalid or the input pointer
 *                          is invalid.
 *
 * @par Errors:
 *    Errors are returned form the function. See the return section.
 *
 * @par Assumptions:
 *    The global queue table has been initialized before this function is
 *    called.
 *****************************************************************************/

N8_Status_t
QMgr_get_control_struct(QueueControl_t  **queue_pp, 
                        N8_Component_t    unit,
                        int               chip)
{
   if ((chip >= queueTable_g.nControlSets) ||
       (unit >= N8_NUM_COMPONENTS) ||
       (!queue_pp))
   {
      /* Requested an invalid chip/unit or passed a null pointer */
      return (N8_INVALID_PARAMETER);
   }                               

   *queue_pp = &(queueTable_g.controlSets_p[chip][unit]);
   return N8_STATUS_OK;
} /* QMgr_get_control_struct */


/*****************************************************************************
 * QMgr_get_valid_unit_number
 *****************************************************************************/
/** @ingroup QMgr
 * @brief This function takes a unit type specifier and a unit number and 
 * validates the unit number.
 *
 * This function will detect invalid unit numbers and fulfill requests for 
 * the API to generate a unit number.
 *
 * @param type        RO: The unit type for the request.
 * @param unitRequest RO: The unit id requested.
 * @param unitReturn  WO: The validated unit to return.  The unit returned
 *                        will be the unit requested unless the user asked
 *                        us to pick one for him.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function succeeded. <BR>
 *    N8_INVALID_PARAMETER: The unitReturn parameter is NULL <BR>
 *    N8_INVALID_UNIT: The unit requested is invalid.
 *
 * @par Errors:
 *    Errors are returned form the function. See the return section.
 *   
 * @par Assumptions:
 *    None.
 *****************************************************************************/
N8_Status_t
QMgr_get_valid_unit_num(N8_Component_t type, 
                        N8_Unit_t      unitRequest, 
                        N8_Unit_t     *unitReturn)
{
   int              nChips;
   N8_Status_t      ret = N8_STATUS_OK; 
   QueueControl_t  *queue_p;

   if (unitReturn == NULL)
   {
      return N8_INVALID_PARAMETER;
   }

   do
   {
      /* Use num chips as num units. */
      nChips = queueTable_g.nControlSets;

      if (unitRequest >= 0 && unitRequest < nChips)
      {                                           
         /* This case is fine */
         *unitReturn = unitRequest;                                      
         DBG(("QMgr_get_valid_unit_num: using requested chip %d.\n", 
              *unitReturn));
         break;
      }

      else if (unitRequest == N8_ANY_UNIT)
      {
         /* The user has asked us to select a chip for the request */
         ret = QMgr_get_chip_for_request(&queue_p, type);  
         if (ret != N8_STATUS_OK)
         {
            break;
         }
         *unitReturn = queue_p->chip;
         DBG(("QMgr_get_valid_unit_num: using N8_ANY_UNIT set to %d.\n", 
              *unitReturn));
         break;
      }
      
      else
      {
         /* The unit requested is neither valid or N8_ANY_UNIT */
         ret = N8_INVALID_UNIT;
      }
   }  while (FALSE);

   return (ret);
}

/*****************************************************************************
 * N8_QMgrQueryStatistics
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Returns values of all stat counters in the Queue Manager.
 *
 *    Returns values of all stat counters in the Queue Manager.
 *
 * @param index RO: Index of a global hardware instance.
 *
 * @par Externals:
 *    queueTable_g: RW: The global queue control table.
 *
 * @return 
 *    N8_STATUS_OK: The function succeeded.
 *    N8_INVALID_PARAMETER: Invalid hardware instance specified.
 *
 * @par Errors:
 *    The only possible failure, is if an invalid hardware instance is
 *    requested.
 *    
 * @par Locks:
 *    n/a
 *   
 * @par Assumptions:
 *    That HWindex is a valid NSP2000 hardware instance.
 *
 *****************************************************************************/

N8_Status_t N8_QMgrQueryStatistics(N8_QueueStatistics_t *stats)
{

   /* GET LOCAL POINTERS TO THIS HARDWARE INSTANCE'S QUEUE CONTROL STRUCTS */
   QueueControl_t *queuePK_p, *queueRN_p, *queueEA_p;
   if (QMgr_get_control_struct(&queuePK_p, N8_PKP, stats->chip) == N8_INVALID_PARAMETER)
   {
        return N8_INVALID_PARAMETER;
   }
   if (QMgr_get_control_struct(&queueRN_p, N8_RNG, stats->chip) == N8_INVALID_PARAMETER)
   {
        return N8_INVALID_PARAMETER;
   }
   if (QMgr_get_control_struct(&queueEA_p, N8_EA,  stats->chip) == N8_INVALID_PARAMETER)
   {
        return N8_INVALID_PARAMETER;
   }

   /* LOAD QueueStatistics_t RETURN STRUCT WITH ALL PK COUNTER VALUES */
   stats->PKstats = queuePK_p->stats;

   /* LOAD QueueStatistics_t RETURN STRUCT WITH ALL EA COUNTER VALUES */
   stats->EAstats = queueEA_p->stats;

   /* LOAD QueueStatistics_t RETURN STRUCT WITH ALL RN COUNTER VALUES */
   stats->RNstats = queueRN_p->stats;

   return N8_STATUS_OK;

} /* N8_QMgrQueryStatistics */

/*****************************************************************************
 * displayQMgr
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Prints Queue Manager debug information to the kernel log.
 *
 * @param  none
 *
 * @par Externals:
 *
 * @return 
 *
 * @par Errors:
 *    
 *****************************************************************************/

void displayQMgr(void)
{
   N8_PRINT(KERN_CRIT "displayQMgr: EA queueCount = %d, PK queueCount = %d\n", 
		      QMgrCount(N8_EA), QMgrCount(N8_PKP));
}
