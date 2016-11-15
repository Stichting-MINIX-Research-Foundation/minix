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

static char const n8_id[] = "$Id: n8_sksInit.c,v 1.1 2008/10/30 12:02:14 darran Exp $";

/*****************************************************************************/
/** @file n8_sksInit.c
 *  @brief Handle SKS driver initialization.
 *
 * Defines functions that are used by the driver to initialize SKS for use.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 01/16/04 jpw   Add N8_NO_64_BURST to prevent 64bit bursts to 32 bit regs
 * 		  during sequential slave io accesses.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 06/07/02 brr   Added function SKS_Remove.
 * 05/02/02 brr   Removed all references to queue structures.
 * 04/29/02 spm   Simplified n8_SKSInitialize.  This fixed Bug #733: BSDi
 *                crash due to n8_daemon_sys_init program.
 * 04/06/02 brr   Do not define N8DEBUG.
 * 04/05/02 bac   Fixed typo in user-visible output message.
 * 04/02/02 spm   Changed proto for n8_SKSInitialize to support SKS
 *                re-architecture.  This will allow the SKS allocation unit
 *                mapping to be accomplished by a normal (forward) ioctl,
 *                thus eliminating the need for a nasty blocking (reverse)
 *                ioctl.
 * 02/22/02 spm   Converted printk's to DBG's.  Added include of n8_OS_intf.h
 * 02/13/02 brr   Removed reliance on QMgr.
 * 01/21/02 spm   Original version.
 ****************************************************************************/
/** @defgroup subsystem_name Subsystem Title (not used for a header file)
 */
#include "helper.h"
#include "n8_driver_main.h"
#include "n8_driver_parms.h"
#include "irq.h"
#include "displayRegs.h"
#include "n8_malloc_common.h"
#include "nsp_ioctl.h"
#include "n8_sks.h"
#include "n8_daemon_common.h"
#include "n8_daemon_sks.h"
#include "n8_OS_intf.h"
#include "n8_sksInit.h"

/* Values for Secure Key Storage control register */
#define PK_SKS_Go_Busy                       0x80000000
#define PK_SKS_PROM_Error                    0x40000000
#define PK_SKS_Access_Error                  0x20000000
#define PK_SKS_Operation_Mask                0x00003000
#define PK_SKS_From_PROM_Mask                0x00002000
#define PK_SKS_Cache_Only_Mask               0x00001000
#define PK_SKS_Address_Mask                  0x00000fff
#define PK_SKS_Op_Address_Mask               0x00003fff
#define PK_SKS_Any_Error_Mask                0x60000000
#define PK_SKS_Max_Length                    0x00001000

#define PK_Cmd_SKS_Offset_Mask               0x00000fff



extern int NSPcount_g;
extern NspInstance_t NSPDeviceTable_g [];

int SKS_Prom2Cache(volatile NSP2000REGS_t *nsp);
/*****************************************************************************
 * SKS_Prom2Cache
 *****************************************************************************/
/** @ingroup NSP2000 Driver
 * @brief This routine copies the entire 4K x 32-bit content of SKS PROM to
 * the SKS cache
 *
 * TODO: What happens when there is no SKS PROM?  Well, currently there is
 *       no way for software to detect this.  If software could detect this
 *       situation, 1. the prom-to-cache copy done in this routine
 *       should be disabled and 2.  the prom write performed by the
 *       SKS management application should be disabled, so that ONLY the
 *       cache write is performed.
 *
 * @param void
 *
 * @par Externals:
 *    None
 *
 * @return
 *    N8_STATUS_OK on success
 *
 * @par Errors:
 *    N8_UNEXPECTED_ERROR if there was an error writing to the SKS cache
 *  
 * @par Locks:
 *    Requires that the caller (QMgrSetupQueue if not in driver)
 *    have the system semaphore.
 *
 * @par Assumptions: See Locks.
 *****************************************************************************/
int SKS_Prom2Cache(volatile NSP2000REGS_t *nsp)
{
   int i = 0;

   for( i = 0; i < N8_SKS_PROM_SIZE; i++ )
   {

      /* Cannot access data register while PK_SKS_Go_Busy is on. */
      while( ( nsp->pkh_secure_key_storage_control & PK_SKS_Go_Busy ) )
      {
         /* Wait until bit clears. */
      }

      /* Enable the SKS write. */
      nsp->pkh_secure_key_storage_control =
         (PK_SKS_Go_Busy | PK_SKS_From_PROM_Mask | ( i & PK_Cmd_SKS_Offset_Mask ));
	N8_NO_64_BURST; 
      /* Check for errors. */
      if ( ( nsp->pkh_secure_key_storage_control & PK_SKS_Access_Error ) |
           ( nsp->pkh_secure_key_storage_control & PK_SKS_PROM_Error ) )
      {

         DBG(("NSP2000: Error writing to SKS PROM. Control Register = %08x",
              nsp->pkh_secure_key_storage_control ));
         return 0;
      }

   }

   return 1;

} /* SKS_Prom2Cache */

/*****************************************************************************
 * SKS_Init
 *****************************************************************************/
/** @ingroup NSP2000 Driver
 * @brief This routine performs the initialization required for the SKS.
 *
 * @param void
 *
 * @par Externals:
 *    None
 *
 * @return
 *    N8_STATUS_OK on success
 *
 * @par Errors:
 *    N8_UNEXPECTED_ERROR if there was an error writing to the SKS cache
 *  
 * @par Locks:
 *
 * @par Assumptions: See Locks.
 *****************************************************************************/
int SKS_Init(NspInstance_t *NSPinstance_p)

{
   /* Handled in nsp.c */
   N8_AtomicLockInit(NSPinstance_p->SKSSem);                                                     
   memset(&NSPinstance_p->SKS_map, 0, N8_DEF_SKS_MEMSIZE);
   NSPinstance_p->SKS_size = N8_DEF_SKS_MEMSIZE;

   return(SKS_Prom2Cache((volatile NSP2000REGS_t *)NSPinstance_p->NSPregs_p));
}

/*****************************************************************************
 * n8_SKSInitialize
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Initialize an instance of the SKS management interface API.
 *
 * This function is called once by the Queue Manager at startup.  The purpose is
 * to read in the persistent key handle files and set up the shared SKS
 * management mapping structure on a per SKS basis.
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    void
 *
 *****************************************************************************/
void n8_SKSInitialize(n8_DaemonMsg_t *pParms)
{

   int i;
   NspInstance_t *NSPinstance_p;


   DBG(( "n8_SKSInitialize entering...\n"));
    
   /* the target SKS to initialize is the unit passed by the user
    * initialization app
    */
   i = pParms->unit;

   do
   {

      /* the SKS management structure is in shared memory accessible via each 
       * queue pointer.  there is also an initialized flag which is shared
       * across all processes.
       */

      /* do not call N8_preamble.  it is unnecessary since the current function
       * is called by the Queue Manager at startup.  Also calling it results in
       * deadlock as the preamble attempts to grab a system semaphore held by
       * the QMgr. */

      DBG(( "N8_sks: user app requested init of SKS unit %d\n",i));
        
 
      if ((i < 0) || (i >= NSPcount_g))
      {
         DBG(("Failed to get control structure: \n"));
         return;
      }
 
      /* assign the right control struct for the target HW */
      NSPinstance_p = &NSPDeviceTable_g[i];
      
      /* Entering critical section. */
      N8_AtomicLock(NSPinstance_p->SKSSem);

      /* copy descriptor table from user space */
      N8_FROM_USER(NSPinstance_p->SKS_map,
                   pParms->SKS_Descriptor_p,
                   sizeof(N8_Buffer_t)*
                   SKS_ALLOC_UNITS_PER_PROM);

/* Count the first consecutive number of units
 * allocated.
 */
#if N8DEBUG
{
    int count=0;
    int m;
    
    for (m=0; m<SKS_ALLOC_UNITS_PER_PROM; m++)
    {
       if (NSPinstance_p->SKS_map[m])
       {
          count++;
       }
    }
    DBG(("n8_SKSInitialize: first %d units allocated in SKS_Memory_p[]\n", count));
}
#endif

      N8_AtomicUnlock(NSPinstance_p->SKSSem);

      DBG(( "released semaphore...\n"));

   } while (FALSE);

   DBG(( "n8_SKSInitialize leaving...\n"));

   return;

} /* n8_SKSInitialize */


/*****************************************************************************
 * SKS_Remove
 *****************************************************************************/
/** @ingroup NSP2000 Driver
 * @brief This routine deletes the resources allocated for the SKS.
 *
 * @param void
 *
 * @par Externals:
 *    None
 *
 * @return
 *    N8_STATUS_OK
 *
 * @par Errors:
 *  
 * @par Locks:
 *
 * @par Assumptions: See Locks.
 *****************************************************************************/
int SKS_Remove(NspInstance_t *NSPinstance_p)

{
   N8_AtomicLockDel(NSPinstance_p->SKSSem);                                                     
   return N8_STATUS_OK;
}
