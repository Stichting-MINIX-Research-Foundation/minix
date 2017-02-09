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

static char const n8_id[] = "$Id: n8_sks.c,v 1.3 2014/03/25 16:19:14 christos Exp $";
/*****************************************************************************/
/** @file SKS_Management_Interface
 *  @brief Implementation for the SKS Management Interface.
 *
 * Allows for the initialization and subsequent key management of associated
 * SKS PROMs.
 *
 *****************************************************************************/


/*****************************************************************************
 * Revision history:
 *
 * 02/10/04 bac   Fixed N8_SKSReset bug (Bug 1006) by changing test logic.
 * 06/06/03 brr   Move n8_enums to public include as  n8_pub_enums.
 * 05/16/03 brr   Eliminate obsolete include file.
 * 04/04/02 bac   Reformat to conform to coding standards.
 * 04/01/02 spm   Moved deletion of key handle files from n8_SKSResetUnit
 *                ioctl to N8_SKSReset API call.
 * 03/04/02 spm   N8_GetSKSKeyHandle changed to set the value of the unit to
 *                N8_ANY_UNIT if N8_SKS_ROUND_ROBIN is defined. (Bug 645)
 *                Fixed N8_SKSGetKeyHandle so that it now copies the passed
 *                entry name into the entry_name field in the key handle.
 * 03/18/02 bac   Made all public entry points execute the preamble to ensure
 *                the API and chip are initialized.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/22/02 spm   Added include of n8_time.h.  Converted N8_KPRINT's to DBG's.
 * 02/20/02 brr   Removed references to the queue structure.
 * 02/14/02 brr   Reconcile 2.0 memory management modifications.
 * 02/05/02 spm   Removed #include of <linux/string.h> and
 *                restored #include of n8_OS_intf.h.
 * 01/28/02 spm   Changed while loops in SKS write to n8_usleeps.
 *                A test is made after the sleep has completed to
 *                insure that the go busy flag is cleared by the
 *                hardware.  Added break to N8_SKSGetKeyHandle for loop
 *                for the case that the file is found.
 * 01/22/02 spm   Moved n8_ComputeKeyLength to n8_sks_util.c.
 * 01/22/02 spm   Moved n8_SKSInitialize ot n8_sksInit.c.
 * 01/20/02 spm   Removed system calls.  Replaced file system handling
 *                with requests to the N8 daemon.
 * 01/17/02 bac   Bug #480 -- SKS write caused persistent N8_HARDWARE_ERROR.
 *                Cleared any existing error flags before the write.  Also
 *                expanded the critical section to include the subsequent wait
 *                for no busy.
 * 01/07/02 bac   Fixed semaphore usage to prevent deadlock.  Bug #450.
 * 12/20/01 bac   Removed global SKS_Descriptor management array and now use
 *                shared memory in the Queue Control structure.  Bug #436.
 * 12/11/01 mel   Fixed bug #414: Added checks for NULL
 * 12/07/01 bac   Fixed merging issues.  N8_SKSFree now, definitively, only
 *                takes a handle and not also an entry name.
 * 12/07/01 bac   Made N8_SKSReset more efficient by only calling SKSWrite once,
 *                free and reclaim entries if a new allocation has the same
 *                name, fixed abug in SKSWrite to force it to wait until
 *                finished before proceeding.
 * 12/03/01 bac   Rearrange order of write to SKS/write file in order to write
 *                the file first and return an error if it does not occur.
 *                Failures on write to the SKS cause the files to be removed.
 *                (BUG #391)
 * 11/29/01 bac   Added closedir() as needed to avoid leaking directory
 *                descriptors. (BUG #385).  Also allowed the use of N8_ANY_UNIT
 *                on calls to N8_SKSReset.  (BUG #386).
 * 12/03/01 mel   Fixed bug #395: in DBG string changed % to %% - to eliminate
 *                compilation error
 * 11/19/01 bac   Reworked the DSA key constraint testing.  Fixed
 *                N8_SKSGetKeyHandle to use the full path name.  Corrected the
 *                computation of the number of SKS words used by pre-existing
 *                entries so that they not be over-written.  Changed
 *                N8_SKSAllocate[RSA|DSA] to accept N8_ANY_KEY as unit
 *                specifiers. 
 *                Bugs #347, 352, 362, 355, 358, 361, 356, 359, 354, 357, 360.
 * 11/16/01 mel   Fixed bug #346: N8_SKSReset memory faults when invalid(-2)
 *                TargetSKS paramter is entered 
 * 11/16/01 mel   Fixed bug #349: N8_SKSDisplay formatted output could be 
 *                printed better.
 * 11/16/01 mel   Fixed bug #348: N8_SKSDisplay memory faults when NULL Pointer
 *                for 1st parameter 
 * 11/16/01 mel   Fixed bug #345: N8_SKSFree command returns 
 *                N8_UNEXPECTED_ERROR instead of N8_INVALID_KEY 
 * 11/16/01 mel   Fixed bug #344: N8_SKSFree with NULL pointer to 
 *                N8_SKSKeyHandle_t parameter causes a memory fault 
 * 11/15/01 mel   Fixed bug #332: N8_AllocateRSA returns N8_INVALID_OBJECT 
 *                instead of N8_INVALID_KEY_SIZE when p.length != q.length 
 * 11/15/01 bac   Changed all fprintf(stderrr,...) to DBG()
 * 11/15/01 mel   Fixed bug #330: N8_AllocateRSA seg faults when pointer to 
 *                privateKey value is NULL
 * 11/15/01 mel   Fixed bug #329: N8_SKSAllocateRSA memory faults when pointer
 *                to KeyMaterial is NULL
 * 11/15/01 mel   Fixed bug #331: N8_SKSAllocateRSA returns N8_HARDWARE_ERROR
 *                when p.lengthBtyes of q.lengthBytes= 0 
 * 11/14/01 spm   Changed prom-to-cache copy fn to work with new register
 *                access macros.
 * 11/13/01 mel   Fixed bug #297: register access macros in n8_sks.c will 
 *                only work on the behavioral model
 * 11/13/01 bac   Close all file descriptors when finished with them.
 * 11/10/01 spm   Added SKS prom-to-cache copy routine for use by
 *                QMgr setup.  Addressed bug #295 with a comment.
 * 11/08/01 mel   Fixed bug #302: No call to N8_preamble in the n8_sks.c API routines.
 * 11/04/01 bac   Propogate return codes rather than masking them with
 *                N8_UNEXPECTED_ERROR and  changed the mode on the call to 
 *                mkdir to allow group write access.
 * 10/30/01 dkm   Eliminate warning in VxWorks
 * 10/22/01 bac   Fixed problems calculating free blocks.
 * 10/19/01 hml   Fixed some compiler warnings.
 * 10/14/01 bac   Included string.h to silence compiler warning.
 * 10/11/01 bac   Changed parameters passed to Param_Byte_Length macros to
 *                reflect their new definitions.
 * 10/10/01 jdj   Multi-thread safe semaphores added to file and register 
 *                accesses.
 * 10/09/01 msz   Minor changes to calls to opendir and mkdir so they compile
 *                without warnings under vxworks.  VxWorks has different
 *                args for mkdir, and doesn't preserve const.
 * 06/04/01 jdj   Original version.
 ****************************************************************************/
/** @defgroup n8_sks SKS Management Interface
 */

#include "n8_sks.h"
#include "n8_pub_sks.h"
#include "n8_rsa.h"
#include "n8_dsa.h"
#include "n8_util.h"
#include "n8_pub_enums.h"
#include "n8_driver_api.h"
#include "n8_API_Initialize.h"
#include "n8_device_info.h"

#include "n8_OS_intf.h"
#include "n8_semaphore.h"
#include "n8_daemon_sks.h"
#include "n8_sks_util.h"
#include "n8_time.h"
#include "n8_SKSManager.h"

extern NSPdriverInfo_t  nspDriverInfo;

/* Local method prototypes */
#ifdef N8DEBUG
void n8_printSKSKeyHandle(const N8_SKSKeyHandle_t *keyHandle_p);
#endif

/* Local methods */

/*****************************************************************************
 * n8_getNumUnits
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Returns how many units (chips) there are in the system.
 *
 *  @param
 *    None
 *
 * @par Externals
 *    nspDriverInfo
 *
 * @return
 *    Number of units.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    Driver has been opened.
 *
 *****************************************************************************/

static int n8_getNumUnits(void)
{

   return nspDriverInfo.numChips;

} /* n8_getNumUnits */


/*****************************************************************************
 * n8_checkAndFreeEntry
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Check to see if a named entry exists and if so free it.  
 *
 * If a request is made to allocate an entry for a tuple <unit, name> and it
 * already exists, then we replace it.  First we look for the named entry and
 * delete it if it exists.  This allows the harvesting of resources before the
 * re-allocation to ensure no resources are lost.
 *
 *  @param name                RO:  name of entry
 *  @param unit                RO:  unit identifier
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/

static N8_Status_t n8_checkAndFreeEntry(const N8_Buffer_t *name, const N8_Unit_t unit)
{
   N8_Status_t ret;
   N8_SKSKeyHandle_t tempHandle;

   tempHandle.unitID = unit;
   ret = N8_SKSGetKeyHandle(name, &tempHandle);

   if (ret == N8_STATUS_OK)
   {
#if N8_SKS_ROUND_ROBIN
      /* If we are using round robin algorithm, then N8_SKS_GetKeyHandle will force
       * the key handle unit ID to N8_ANY_UNIT (-1).  We must restore the unit ID
       * to its original value in this case.
       */
      tempHandle.unitID = unit;
#endif
      /* an entry of this name exists.  free it and reclaim the resources. */
      ret = N8_SKSFree(&tempHandle);
   }
   else
   {
      ret = N8_STATUS_OK;
   }
   return ret;
}

/*****************************************************************************
 * n8_verifyUnitID
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Given a unit ID, verify that it is valid.  If N8_ANY_UNIT is
 * specified, then have the QMgr select one to use.
 *
 *  @param unit                RO:  unit ID to be verified
 *  @param keyHandle_p         RW:  SKSKeyHandle to place the results
 *
 * @par Externals
 *      None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_INVALID_VALUE if the unit id is out of range.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
static N8_Status_t
n8_verifyUnitID(const N8_Unit_t unit, N8_SKSKeyHandle_t *keyHandle_p)
{
   int numSKS;
   static int sksSelection = 0;

   /* get the number of units */
   numSKS = n8_getNumUnits();

   /* TODO: We are selecting a unit number here.  It should be done in */
   /* QMgr.  However, it appeared that we might first need to select a */
   /* unit in this code, so we can check if the file exists, and the   */
   /* filename is based on the unit id.  So, for now, we will select a */
   /* valid unit ID here, and return it.  In the future it would be    */
   /* perhaps better to have the write return what unit was selected.  */
   if (unit == N8_ANY_UNIT)
   {
      keyHandle_p->unitID = sksSelection;
      sksSelection = sksSelection + 1;
      if (sksSelection == numSKS)
      {
         sksSelection = 0;
      }
   }
   else if ((unit < 0) || (unit >= numSKS))
   {
      return N8_INVALID_VALUE;
   }
   else
   {
      keyHandle_p->unitID = unit;
   }

   return N8_STATUS_OK;
}

#ifdef N8DEBUG
/*****************************************************************************
 * n8_printSKSKeyHandle
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Display a key handle for an SKS PROM.
 *
 * @param keyHandle_p RO: A pointer to the key handle to be printed.
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *
 * @par Assumptions:
 *    None.
 *****************************************************************************/
void n8_printSKSKeyHandle(const N8_SKSKeyHandle_t *keyHandle_p)
{

   DBG(("Key Handle:\n"));
   DBG(("Key Type   %08x\n", 
        keyHandle_p->key_type));
   DBG(("Key Length %08x\n\tSKS Offset %08x\n", 
        keyHandle_p->key_length,
        keyHandle_p->sks_offset));
   DBG(("Target SKS %08x\n", 
        keyHandle_p->unitID));
} /* n8_printSKSKeyHandle */
#endif

/* Public methods */

/*****************************************************************************
 * N8_SKSDisplay
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Display a description entity for an SKS PROM.
 *
 * Attempts to write a string into a given char string array.
 *
 * @param keyHandle_p       RW: A N8_SKSKeyHandle_t pointer. 
 * @param display_string    RO: A char array. 
 *
 * @par Externals:
 *    external_var1  RW: A Read Write external variable<BR>
 *    external_var2  RO: A Read Only external variable<BR>
 *    external_var3  WO: A Write Only external variable: No "break" on last one.
 *
 * @return 
 *    N8_STATUS_OK if no other errors. The display contents should now be
 *      within the provided char array.
 *
 * @par Assumptions:
 *    The given key handle pointer is valid.
 *****************************************************************************/
N8_Status_t N8_SKSDisplay(N8_SKSKeyHandle_t *keyHandle_p,
                          char              *display_string_p, size_t len) 
{
   if (keyHandle_p == NULL)
   {
      return N8_INVALID_KEY;
   }

   if (display_string_p == NULL)
   {
      return N8_INVALID_PARAMETER;
   }

   snprintf(display_string_p, len,
           "Key Handle:\n"
           "\tKey Type   %08x\n"
           "\tKey Length %08x\n"
           "\tSKS Offset %08x\n"
           "\tTarget SKS %08x\n",
           keyHandle_p->key_type, keyHandle_p->key_length,
           keyHandle_p->sks_offset, keyHandle_p->unitID);

   return N8_STATUS_OK;
} /* N8_SKSDisplay */


/*****************************************************************************
 * N8_SKSAllocateRSA
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Allocate and write a private RSA key entry to an SKS PROM.
 *
 * Attempts to allocate, then write the key into an SKS PROM.
 *
 * @param keyMaterial_p       RW: A N8_RSAKeyMaterial_t pointer.
 * @param keyEntryName_p      RW: A char pointer, the name of the sks entry.
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    N8_STATUS_OK indicates the key allocation and write successfully completed.
 *    N8_UNEXPECTED_ERROR indicates an error writing the key handle or that 
 *      the API was not or could not be initialized.
 *
 * @par Assumptions:
 *    That the RSA key material pointer is valid.
 *****************************************************************************/ 
N8_Status_t N8_SKSAllocateRSA(N8_RSAKeyMaterial_t *keyMaterial_p,
                              const N8_Buffer_t *keyEntryName_p)
{
   N8_RSAKeyObject_t key;
   N8_Buffer_t* param;

   uint32_t sksOffset = 0, targetSKS = 0, keyLength = 0;
   N8_Status_t ret = N8_STATUS_OK;
   N8_SKSKeyHandle_t *sks_key_p;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      CHECK_OBJECT(keyMaterial_p, ret);
      CHECK_OBJECT(keyMaterial_p->privateKey.value_p, ret);

      CHECK_OBJECT(keyEntryName_p, ret);
      if (strlen(keyEntryName_p) >= N8_SKS_ENTRY_NAME_MAX_LENGTH)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      if ((keyMaterial_p->p.lengthBytes == 0) ||
          (keyMaterial_p->q.lengthBytes == 0))
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      sks_key_p = &keyMaterial_p->SKSKeyHandle;

      ret = n8_verifyUnitID(keyMaterial_p->unitID, sks_key_p);
      CHECK_RETURN(ret);

      /* Check requirements for p and q lengths. */

      /* len(p) == len(q) */
      if (keyMaterial_p->q.lengthBytes != keyMaterial_p->p.lengthBytes)
      {
         DBG(("P and Q RSA key material parameters not of same length!\n"));
         ret = N8_INVALID_KEY_SIZE;
         break;
      }
      /* len(p) == len(q) == 1/2 len(private key).  note we don't perform the
       * division in the test as integer division would round down. */
      if ((keyMaterial_p->q.lengthBytes * 2) !=
          keyMaterial_p->privateKey.lengthBytes)
      {
         DBG(("P and Q RSA key material parameter length not half of "
              "private key length!\n")); 
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      /* public key length mod 32 == 0 or 17-31 */
      if (keyMaterial_p->privateKey.lengthBytes % 32 != 0 &&
          keyMaterial_p->privateKey.lengthBytes % 32 <= 16)
      {
         DBG(("Private key length %% 32 is not in the valid range of 0 or 17-31.\n"));
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      /* check to see if an entry of this name already exists and free
       * it if so. */
      ret = n8_checkAndFreeEntry(keyEntryName_p, keyMaterial_p->SKSKeyHandle.unitID);
      CHECK_RETURN(ret);

      ret = N8_RSAInitializeKey(&key, N8_PRIVATE_CRT, keyMaterial_p, NULL);
      CHECK_RETURN(ret);

      keyLength = keyMaterial_p->privateKey.lengthBytes;
      targetSKS = sks_key_p->unitID;
      sks_key_p->key_type = N8_RSA_VERSION_1_KEY;

      /* The key length field of the key handle is always in BNC digits. */
      sks_key_p->key_length = 
         BYTES_TO_PKDIGITS(keyLength);

      DBG(("Key Length in bytes:  %d\n", keyLength));
      DBG(("Key length in digits: %d\n", sks_key_p->key_length));

      ret = n8_SKSAllocate(sks_key_p);
      CHECK_RETURN(ret);

      /* attempt to write the key information to the mapping files */
      strcpy(sks_key_p->entry_name, keyEntryName_p);
        
      /* request N8 userspace daemon to write out a key handle file */
      ret = n8_daemon_sks_write(sks_key_p, keyEntryName_p);
        
      if (ret != N8_STATUS_OK)
      {
         /* the write to the key handle failed.
          * we need to unallocate the space
          * and return a failure. */

         DBG(("n8_daemon_sks_write returned error\n"));

         n8_SKSsetStatus(sks_key_p, SKS_FREE);
         break;
      }

      sksOffset = sks_key_p->sks_offset;

      /* Write the data into the SKS PROM. */
      DBG(("Writing key data into SKS.\n"));

      /* Write the p value into the SKS. */
      DBG(("Writing p param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_P_Param_Byte_Offset(&key);

      ret =  n8_SKSWrite(targetSKS,
                         (uint32_t*) param,
                         SKS_RSA_P_LENGTH(sks_key_p->key_length),
                         sksOffset +
                         SKS_RSA_P_OFFSET(sks_key_p->key_length),
                         FALSE);
      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);

      /* Write the p value into the SKS. */
      DBG(("Writing q param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_Q_Param_Byte_Offset(&key); 

      ret =  n8_SKSWrite(targetSKS,
                         (uint32_t*) param,
                         SKS_RSA_Q_LENGTH(sks_key_p->key_length), 
                         sksOffset +
                         SKS_RSA_Q_OFFSET(sks_key_p->key_length),
                         FALSE); 
      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);

      /* Write the dp value into the SKS. */
      DBG(("Writing dp param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_DP_Param_Byte_Offset(&key); 

      ret =  n8_SKSWrite(targetSKS,
                         (uint32_t*) param,
                         SKS_RSA_DP_LENGTH(sks_key_p->key_length),
                         sksOffset +
                         SKS_RSA_DP_OFFSET(sks_key_p->key_length),
                         FALSE);

      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);


      /* Write the dq value into the SKS. */
      DBG(("Writing dq param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_DQ_Param_Byte_Offset(&key); 

      ret =  n8_SKSWrite(targetSKS,
                         (uint32_t*) param,
                         SKS_RSA_DQ_LENGTH(sks_key_p->key_length),
                         sksOffset +
                         SKS_RSA_DQ_OFFSET(sks_key_p->key_length),
                         FALSE);
      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);

      /* Write the R mod p value into the SKS. */
      DBG(("Writing R mod p param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_R_MOD_P_Param_Byte_Offset(&key); 


      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_RSA_RMODP_LENGTH(sks_key_p->key_length),
                     sksOffset +
                     SKS_RSA_RMODP_OFFSET(sks_key_p->key_length),
                     FALSE);
      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);

      /* Write the R mod q value into the SKS. */
      DBG(("Writing R mod q param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_R_MOD_Q_Param_Byte_Offset(&key);

      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_RSA_RMODQ_LENGTH(sks_key_p->key_length),
                     sksOffset +
                     SKS_RSA_RMODQ_OFFSET(sks_key_p->key_length),
                     FALSE);
      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);

      /* Write the n value into the SKS. */
      DBG(("Writing n param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_N_Param_Byte_Offset(&key);

      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_RSA_N_LENGTH(sks_key_p->key_length),
                     sksOffset +
                     SKS_RSA_N_OFFSET(sks_key_p->key_length),
                     FALSE);

      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);

      /* Write the pInv value into the SKS. */
      DBG(("Writing pInv param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_U_Param_Byte_Offset(&key);


      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_RSA_PINV_LENGTH(sks_key_p->key_length),
                     sksOffset +
                     SKS_RSA_PINV_OFFSET(sks_key_p->key_length),
                     FALSE);

      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);

      /* Write the cp value into the SKS. */
      DBG(("Writing cp param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_CP_Param_Byte_Offset(&key);

      ret =  n8_SKSWrite(targetSKS,
                         (uint32_t*) param,
                         SKS_RSA_CP_LENGTH(sks_key_p->key_length),
                         sksOffset +
                         SKS_RSA_CP_OFFSET(sks_key_p->key_length),
                         FALSE);

      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
      CHECK_RETURN(ret);

      /* Write the cq value into the SKS. */
      DBG(("Writing cq param into SKS.\n"));

      param = (N8_Buffer_t*) key.kmem_p->VirtualAddress + 
         PK_RSA_CQ_Param_Byte_Offset(&key);

      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_RSA_CQ_LENGTH(sks_key_p->key_length),
                     sksOffset +
                     SKS_RSA_CQ_OFFSET(sks_key_p->key_length),
                     FALSE);
      DBG(("Return from write: %s\n", N8_Status_t_text(ret)));
   } while (FALSE);

   if (key.structureID == N8_RSA_STRUCT_ID)
   {
      N8_Status_t freeRet;
      freeRet = N8_RSAFreeKey(&key);
      /* if we terminated the processing loop with an error, let's report that
       * error to the calling function rather than have it masked by the return
       * from free key. */
      if (ret == N8_STATUS_OK)
      {
         ret = freeRet;
      }
   }
   return ret;

} /* N8_SKSAllocateRSA */


/*****************************************************************************
 * N8_SKSAllocateDSA
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Allocate and write a private DSA key entry to an SKS PROM.
 *
 * Attempts to allocate, then write the key into an SKS PROM.
 *
 * @param keyMaterial_p       RW: A N8_DSAKeyMaterial_t pointer.
 * @param keyEntryName_p      RW: A char pointer, the name of the sks entry.
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    N8_STATUS_OK indicates the key allocation and write successfully 
 *      completed.
 *    N8_UNEXPECTED_ERROR indicates an error writing the key handle or that 
 *      the API was not or could not be initialized.
 *
 * @par Assumptions:
 *    That the DSA key material pointer is valid.
 *****************************************************************************/  
N8_Status_t N8_SKSAllocateDSA(N8_DSAKeyMaterial_t *keyMaterial_p,
                              const N8_Buffer_t *keyEntryName_p)
{
   N8_DSAKeyObject_t key;
   N8_Buffer_t* param;
   unsigned int keyLength;
   uint32_t targetSKS = 0, sksOffset = 0;
   N8_Status_t ret;
   N8_SKSKeyHandle_t *sks_key_p;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      CHECK_OBJECT(keyMaterial_p, ret);
      CHECK_OBJECT(keyMaterial_p->privateKey.value_p, ret);
      CHECK_OBJECT(keyEntryName_p, ret);
      if (strlen(keyEntryName_p) >= N8_SKS_ENTRY_NAME_MAX_LENGTH)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }

      if ((keyMaterial_p->p.lengthBytes == 0) ||
          (keyMaterial_p->q.lengthBytes == 0))
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      ret = n8_DSAValidateKey(keyMaterial_p, N8_PRIVATE);
      CHECK_RETURN(ret);

      sks_key_p = &keyMaterial_p->SKSKeyHandle;

      ret = n8_verifyUnitID(keyMaterial_p->unitID, sks_key_p);
      CHECK_RETURN(ret);

      /* The key length field of the key handle is always in BNC digits. */
      sks_key_p->key_length = 
         BYTES_TO_PKDIGITS(keyMaterial_p->p.lengthBytes);

      /* check to see if an entry of this name already exists and free
       * it if so. */
      ret = n8_checkAndFreeEntry(keyEntryName_p, keyMaterial_p->SKSKeyHandle.unitID);
      CHECK_RETURN(ret);

      ret = N8_DSAInitializeKey(&key, N8_PRIVATE, keyMaterial_p, NULL);
      CHECK_RETURN(ret);

      keyLength = sks_key_p->key_length;
      targetSKS = sks_key_p->unitID;
      sks_key_p->key_type = N8_DSA_VERSION_1_KEY;


      /* allocate an sks. */
      ret = n8_SKSAllocate(sks_key_p);
      CHECK_RETURN(ret);

      /* attempt to write the key information to the mapping files */
      strcpy(sks_key_p->entry_name, keyEntryName_p);
        
      /* request N8 userspace daemon to write out a key handle file */
      ret = n8_daemon_sks_write(sks_key_p, keyEntryName_p);
      if (ret != N8_STATUS_OK)
      {
         /* the write to the key handle failed.
          * we need to unallocate the space
          * and return a failure. */
         n8_SKSsetStatus(sks_key_p, SKS_FREE);
         break;
      }
        
      sksOffset = sks_key_p->sks_offset;

      /* Write the data into the SKS PROM. */
      DBG(("Writing key data into SKS.\n"));

      /* The DSA parameter block to be stored the DSA has the following structure:
       * p            sks_offset                    key_length digits
       * g*R mod p    sks_offset + 4 * key_length   key_length digits
       * q            sks_offset + 8 * key_length   2 digits
       * x            sks_offset + 8*kl + 8         2 digits
       * p            sks_offset + 8*kl + 16        1 digit
       */

      /* Write the p value into the SKS. */
      DBG(("Writing p param into SKS.\n"));

      param = key.paramBlock + PK_DSA_P_Param_Offset; 

      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_DSA_P_LENGTH(sks_key_p->key_length),
                     sksOffset + SKS_DSA_P_OFFSET(sks_key_p->key_length),
                     FALSE);
      CHECK_RETURN(ret);

      /* Write the gR mod p value into the SKS. */
      DBG(("Writing gR mod p param into SKS.\n"));

      param = key.paramBlock +  PK_DSA_GR_MOD_P_Param_Offset(sks_key_p->key_length);

      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_DSA_GRMODP_LENGTH(sks_key_p->key_length),
                     sksOffset + SKS_DSA_GRMODP_OFFSET(sks_key_p->key_length),
                     FALSE);
      CHECK_RETURN(ret);

      /* Write the q value into the SKS. */
      DBG(("Writing q param into SKS.\n"));

      param = key.paramBlock +
         PK_DSA_Q_Param_Offset(sks_key_p->key_length);

      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_DSA_Q_LENGTH(sks_key_p->key_length),
                     sksOffset + SKS_DSA_Q_OFFSET(sks_key_p->key_length),
                     FALSE);
      CHECK_RETURN(ret);

      /* Write the x value (private key) into the SKS. */
      DBG(("Writing x (private key) param into SKS.\n"));

      param = key.paramBlock +
         PK_DSA_X_Param_Offset(sks_key_p->key_length);

      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_DSA_X_LENGTH(sks_key_p->key_length),
                     sksOffset +
                     SKS_DSA_X_OFFSET(sks_key_p->key_length),
                     FALSE); 
      CHECK_RETURN(ret);

      /* Write the cp value into the SKS. */
      DBG(("Writing cp into SKS.\n"));

      param = key.paramBlock + 
         PK_DSA_CP_Param_Offset(sks_key_p->key_length);

      ret =
         n8_SKSWrite(targetSKS,
                     (uint32_t*) param,
                     SKS_DSA_CP_LENGTH(sks_key_p->key_length),
                     sksOffset + SKS_DSA_CP_OFFSET(sks_key_p->key_length),
                     FALSE);
      CHECK_RETURN(ret);

   } while (FALSE);

   if (key.structureID == N8_DSA_STRUCT_ID)
   {
      N8_Status_t freeRet;
      freeRet = N8_DSAFreeKey(&key);
      /* if we terminated the processing loop with an error, let's report that
       * error to the calling function rather than have it masked by the return
       * from free key. */
      if (ret == N8_STATUS_OK)
      {
         ret = freeRet;
      }
   }

   return ret;

} /* N8_SKSAllocateDSA */

/*****************************************************************************
 * N8_SKSFree
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief De-allocates and erases a private key entry to an SKS PROM.
 *
 * Attempts to de-allocate, then erase the key into an SKS PROM.
 *
 * @param keyHandle_p       RW: A N8_SKSKeyHandle_t pointer.
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    N8_STATUS_OK indicates the key de-allocation and erase successfully 
 *      completed.
 *    N8_UNEXPECTED_ERROR indicates an error erasing the SKS key entry or that 
 *      the API was not or could not be initialized.
 *
 * @par Assumptions:
 *    That the key handle pointer is valid.
 *****************************************************************************/
N8_Status_t N8_SKSFree(N8_SKSKeyHandle_t* keyHandle_p)
{
   int words_to_free;
   char* key_type;
   int i;
   uint32_t zero = 0x0;
   N8_Status_t ret;
   char fullFileName[1024];

   DBG(("SKS Free\n"));

   ret = N8_preamble();
   if (ret != N8_STATUS_OK)
   {
      return ret;
   }
   if (keyHandle_p == NULL)
   {
      ret = N8_INVALID_KEY;
      return ret;
   }

#ifdef N8DEBUG
   n8_printSKSKeyHandle(keyHandle_p);
#endif

   if ((keyHandle_p->key_type) == N8_RSA_VERSION_1_KEY)
   {
      words_to_free = SKS_RSA_DATA_LENGTH(keyHandle_p->key_length);
      key_type = "RSA";
   }
   else if (keyHandle_p->key_type == N8_DSA_VERSION_1_KEY)
   {
      words_to_free = SKS_DSA_DATA_LENGTH(keyHandle_p->key_length);
      key_type = "DSA";
   }
   else
   {
      DBG(("Unknown key type.\n"));
      return N8_INVALID_KEY;
   }

   DBG(("Zeroing out the key in the SKS PROM.\n"));

   /* Grab the offset and begin deleting data! */
   for (i = keyHandle_p->sks_offset; 
        (i < keyHandle_p->sks_offset+words_to_free) && (i < SKS_PROM_MAX_OFFSET); 
        i++)
   {
      ret = n8_SKSWrite(keyHandle_p->unitID, &zero, 1, i, FALSE);
      if (ret != N8_STATUS_OK)
      {
         DBG(("Error writing to SKS in N8_SKSFree.  (%s)\n",
              N8_Status_t_text(ret)));
         return ret;                 
      }
   }

   n8_SKSsetStatus(keyHandle_p, SKS_FREE);

   sprintf(fullFileName, "%s%d/%s",
           SKS_KEY_NODE_PATH, 
           keyHandle_p->unitID,
           keyHandle_p->entry_name);
    
   /* request N8 userspace daemon to delete the specfied file */
   n8_daemon_sks_delete(fullFileName);
    
   return N8_STATUS_OK;

} /* N8_SKSFree */


/*****************************************************************************
 * N8_SKSGetKeyHandle
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Reads a key handle data from a named key entry for an SKS PROM.
 *
 * @param systemKeyNode     RW: A char pointer, the named key entry.
 * @param keyHandle_p       WO: A N8_SKSKeyHandle_t pointer.
 *
 * @par Externals:
 *    SKS_initialized_g     RW: A boolean value that indicates whether the SKS
 *                              admin interface API has been initialized.
 * @return 
 *    N8_STATUS_OK indicates the key read successfully completed.
 *    N8_UNEXPECTED_ERROR indicates an error reading the SKS key entry or that 
 *      the API was not or could not be initialized.
 *
 *****************************************************************************/
N8_Status_t N8_SKSGetKeyHandle(const N8_Buffer_t* keyEntryName,
                               N8_SKSKeyHandle_t* keyHandle_p)
{

   N8_Status_t ret = N8_STATUS_OK;
   char fullFileName[1024];
   int numberSKS;
   int i;
   int found;

   DBG(("Get KeyHandle : \n"));

   ret = N8_preamble();
   if (ret != N8_STATUS_OK)
   {
      return ret;
   }
    
   if ((keyEntryName == NULL) || (keyHandle_p == NULL))
   {
      return N8_INVALID_OBJECT;
   }

   /* get the number of units */
   numberSKS = n8_getNumUnits();

   /* check to see if we can have a buffer overrun.  the +4 is for the trailing
    * '/' and for the size of the unitID -- assuming the number of units is no
    * more than 999. */

   if ((strlen(SKS_KEY_NODE_PATH) + strlen(keyEntryName) + 4) >=
       sizeof(fullFileName))
   {
      return N8_UNEXPECTED_ERROR;
   }
   found = -1;
   for (i = 0; i < numberSKS; i++)
   {
      sprintf(fullFileName, "%s%d/%s", SKS_KEY_NODE_PATH, i, keyEntryName);
      /* request N8 userspace daemon to read from the specfied key
       * handle file
       */
      ret = n8_daemon_sks_read(keyHandle_p, fullFileName);
      if (ret == N8_STATUS_OK)
      {
         found = i;
         /* n8_daemon_sks_read does not set the entry name */
         strcpy(keyHandle_p->entry_name, keyEntryName);
#if N8_SKS_ROUND_ROBIN
         keyHandle_p->unitID = N8_ANY_UNIT;
#endif /* N8_SKS_ROUND_ROBIN */
         break;
      }
   }
   if (found == -1)
   {
      ret = N8_INVALID_KEY;
   }

   return ret;
} /* N8_SKSGetKeyHandle */

/*****************************************************************************
 * N8_SKSReset
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Obliterates all data within an SKS PROM.
 *
 * Attempts to de-allocate, then erase the all key entries in an SKS PROM.
 * THIS SHOULD NEVER BE INVOKED EXCEPT TO CLEAR ALL KEYS IN A GIVEN PROM.
 * THIS IS AN IRREVERSIBLE ACTION.
 *
 * @param targetSKS       RW: A int, the target SKS PROM. 
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    N8_STATUS_OK indicates the key de-allocation and erase successfully 
 *      completed.
 *    N8_UNEXPECTED_ERROR indicates an error erasing the SKS key entry or that 
 *      the API was not or could not be initialized.
 *
 *****************************************************************************/
N8_Status_t N8_SKSReset(N8_Unit_t targetSKS)
{
   int i;
   N8_Status_t ret = N8_STATUS_OK;
   N8_Unit_t firstSKS, lastSKS;
   int numberSKS;

   DBG(("N8_SKSReset: entering...\n"));

   ret = N8_preamble();
   if (ret != N8_STATUS_OK)
   {
      return ret;
   }

   /* get the number of units */
   numberSKS = n8_getNumUnits();

   if (targetSKS == N8_ANY_UNIT)
   {
      firstSKS = 0;
      lastSKS = numberSKS-1;
   }
   else if (targetSKS >= 0 &&
            targetSKS < numberSKS)
   {
      firstSKS = lastSKS = targetSKS;
   }
   else
   {
      ret = N8_INVALID_VALUE;
      return ret;
   }
   /* loop over the SKS units to be reset.  it will either be all or just the
    * targetSKS. */
   for (i = firstSKS; i <= lastSKS; i++)
   {

      ret = n8_SKSResetUnit(i);
      if (ret != N8_STATUS_OK)
      {
         return ret;
      }
      /* request N8 userspace daemon to remove
       * all the key handle files on the host
       * file system that are under the specified
       * execution unit
       */
      ret = n8_daemon_sks_reset(i);
      if (ret != N8_STATUS_OK)
      {
         DBG(("Error resetting SKS files: %d\n", ret));
         return N8_FILE_ERROR;
      }
   }
   DBG(("N8_SKSReset: leaving...\n"));


   return ret;
} /* N8_SKSReset */

/*****************************************************************************
 * N8_SKSVerifyRSA
 *****************************************************************************/
N8_Status_t N8_SKSVerifyRSA(N8_SKSKeyHandle_t* keyHandle_p,
                            N8_Buffer_t* input_p, 
                            N8_Buffer_t* result_p)
{
   N8_RSAKeyObject_t privateKey;
   N8_Buffer_t* decryptBuffer_p;
   N8_Status_t ret;

   DBG(("Verify RSA Key.\n"));

   ret = N8_preamble();
   if (ret != N8_STATUS_OK)
   {
      return ret;
   }
   /* Set the material to NULL as we are using the SKS. */
   ret = N8_RSAInitializeKey(&privateKey, N8_PRIVATE_SKS, NULL, NULL);
   if (ret != N8_STATUS_OK)
   {
      DBG(("Could not initialize RSA private key from SKS. (%s)\n",
           N8_Status_t_text(ret)));
      return ret;
   }

   decryptBuffer_p = (N8_Buffer_t *) N8_UMALLOC(privateKey.privateKeyLength);
   if (decryptBuffer_p == NULL)
   {
      DBG(("Could not allocate %i bytes for decrypt buffer.\n", 
           privateKey.privateKeyLength));
      return N8_MALLOC_FAILED;
   }

   ret = N8_RSADecrypt(&privateKey, input_p,
                       privateKey.privateKeyLength,
                       decryptBuffer_p, NULL);
   if (ret != N8_STATUS_OK)
   {
      DBG(("Could not complete RSA decrypt using SKS private key. (%s)\n",
           N8_Status_t_text(ret)));
      return ret;
   }

   /* Compare the buffers. If they are different, then indicate this in the return code. */
   if (memcmp(result_p, decryptBuffer_p, privateKey.privateKeyLength) != 0)
   {
      DBG(("Message buffers are not the same. "
           "RSA Decrypt failed or SKS private key material is not valid.\n"));
      return N8_VERIFICATION_FAILED;
   }

   DBG(("RSA Decrypt good: SKS private key is valid.\n"));

   return N8_STATUS_OK;
} /* N8_SKSVerifyRSA */

/*****************************************************************************
 * N8_SKSVerifyDSA
 *****************************************************************************/

N8_Status_t N8_SKSVerifyDSA(N8_SKSKeyHandle_t* keyHandle_p, 
                            N8_Buffer_t* inputHash_p, 
                            N8_Buffer_t* resultRValue_p,
                            N8_Buffer_t* resultSValue_p)
{
   N8_DSAKeyObject_t privateKey;
   N8_Buffer_t* signRValueBuffer_p, *signSValueBuffer_p;
   N8_Status_t ret;
   DBG(("Verify DSA Key.\n"));

   ret = N8_preamble();
   if (ret != N8_STATUS_OK)
   {
      return ret;
   }

   /* Set the material to NULL as we are using the SKS. */
   ret = N8_DSAInitializeKey(&privateKey, N8_PRIVATE_SKS, NULL, NULL);
   if (ret != N8_STATUS_OK)
   {
      DBG(("Could not initialize DSA private key from SKS. (%s)\n",
           N8_Status_t_text(ret)));
      return ret;
   }

   /* !!!!!! Is there a #define for the S and R value byte lengths?!?!?!! */
   if ((signRValueBuffer_p = (N8_Buffer_t *) N8_UMALLOC(20)) != 0)
   {
      DBG(("Could not allocate %i bytes for DSA sign R value buffer.\n", 
           privateKey.modulusLength));
      return N8_MALLOC_FAILED;
   }

   if ((signSValueBuffer_p = (N8_Buffer_t *) N8_UMALLOC(20)) != 0)
   {
      DBG(("Could not allocate %i bytes for DSA sign S value buffer.\n", 
           privateKey.modulusLength));
      return N8_MALLOC_FAILED;
   }

   ret = N8_DSASign(&privateKey, inputHash_p, signRValueBuffer_p,
                    signSValueBuffer_p, NULL);
   if (ret != N8_STATUS_OK)
   {
      DBG(("Could not complete DSA sign using SKS private key. (%s)\n",
           N8_Status_t_text(ret)));
      return ret;
   }

   /* Compare the buffers. If they are different, then indicate this in the return code. */
   if (memcmp(resultRValue_p, signRValueBuffer_p, 20) != 0)
   {
      DBG(("Result R value buffers are not the same. "
           "DSA Sign failed or SKS private key material is not valid.\n"));
      return N8_VERIFICATION_FAILED;
   }

   if (memcmp(resultSValue_p, signSValueBuffer_p, 20) != 0)
   {
      DBG(("Result S value buffers are not the same.  "
           "DSA Sign failed or SKS private key material is not valid.\n"));
      return N8_VERIFICATION_FAILED;
   }

   DBG(("DSA Sign good: SKS private key is valid.\n"));

   return N8_STATUS_OK;

} /* N8_SKSVerifyDSA */


