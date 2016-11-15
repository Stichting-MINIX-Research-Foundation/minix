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

/*****************************************************************************
 * @(#) n8_driver_api.h 1.11@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_driver_api.h
 *  @brief NSP2000 Driver API - Prototypes and Resources.
 *
 * This header contains prototypes for using the NSP2000 driver API, as well
 * as data structures and other resources. This header must be included
 * by any application wishing to make driver API calls.
 *****************************************************************************/

/*****************************************************************************
 * Revision history:  
 * 04/21/03 brr   Added support for multiple memory banks.
 * 04/01/03 brr   Reverted N8_WaitOnRequest to accept timeout parameter.
 * 03/19/03 brr   Modified prototype for N8_WaitOnRequest to accept an API
 *                request pointer.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 10/22/02 brr   Added openMode parameter to N8_OpenDevice.
 * 10/11/02 brr   Added timeout parameter to N8_WaitOnRequest.
 * 09/18/02 brr   Added prototypes for N8_DiagInfo & N8_WaitOnRequest.
 * 07/19/02 brr   Moved definitions of N8_VirtToPhys & N8_PhysToVirt from here
 *                to n8_OS_intf.h.
 * 07/17/02 brr   Update comments & eliminate unused prototypes.
 * 07/02/02 brr   Added prototype for N8_QueryMemStatistics & N8_QMgrDequeue.
 * 03/29/02 hml   Added proto for N8_ContextMemValidate. 
 * 03/27/02 hml   Changed N8_QueueReturnCodees_t to N8_Status_t.
 * 03/26/02 hml   Updated protos for N8_ContextMemFree and N8_ContextMemAlloc.
 * 03/22/02 hml   Removed sessionID from the N8_ContextMemAlloc proto.
 * 03/21/02 mmd   Implemented N8_QMgrQueryStatistics.
 * 03/18/02 brr   Added Context Memory API calls.
 * 03/01/02 brr   Do not include driver include files.
 * 02/25/02 brr   Reworked function prototypes for 2.1 changes & removed 
 *                obsolete IOCTL's.
 * 02/01/02 brr   Fixed include files.
 * 01/31/02 brr   Modified memory management functions to improve performance.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 10/22/01 mmd   Added new 3rd parm to N8_InitializeFPGA.
 * 10/15/01 mmd   Implemented N8_DriverDebug.
 * 10/12/01 dkm   Moved public portion to n8_pub_common.h.
 * 10/02/01 mmd   N8_InitializeFPGA now returns N8_EVENT_INCOMPLETE if the
 *                FPGA is currently being programmed by another entity.
 * 09/25/01 mmd   Implemented N8_GetConfigurationItem.
 * 09/05/01 bac   Renamed formal parameter FPGA in N8_IsFPGA to avoid
 *                compilation problems with the define of the same name.
 * 08/28/01 mmd   Revised API to new version.
 * 08/28/01 mmd   Added Key and Bitfield fields to N8_MemoryHandle_t, and now
 *                including memorycategories.h.
 * 08/16/01 mmd   Eliminated "simon", renamed, and revised to include the
 *                NSP2000 register layout for application use.
 * 07/25/01 mmd   Added N8_IsFPGA.
 * 06/28/01 jke   Fixed Makefile such that include of n8_errors.h not need 
 *                explicit path info
 * 06/05/01 mmd   Corrected N8_TestBuffer according to API spec.
 * 05/29/01 mmd   Incorporated suggestions from code review.
 * 05/17/01 mmd   Original version.
 ****************************************************************************/
/** @defgroup nsp2000drv NSP2000 Driver API - Prototypes and Resources.
 */

#ifndef N8_DRIVER_API_H
#define N8_DRIVER_API_H

#include "n8_pub_common.h"
#include "n8_pub_errors.h"
#include "n8_pub_rng.h"
#include "n8_malloc_common.h"
#include "n8_enqueue_common.h"
#include "n8_device_info.h"
#include "nsp_ioctl.h"



/*****************************************************************************
 * N8_OpenDevice
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Opens and initializes the NSP2000 driver.
 *
 * This routine opens the driver, and sets up all resources required to use
 * the NSP2000.
 *
 * @param driverInfo_p   RO: Pointer to structure that holds information
 *                           describing the driver's resources.
 * @param allocUserPool  RO: Flag indicating whether the process opening this
 *                           device needs memory resources mmap'ed to user space
 * @param openMode       RO: Flag indicating whether the process opening this
 *                           device intends to run diagnostics.
 *
 *
 * @par Externals:
 *    NSP_IOCTL_*   RO: #define - IOCTL codes for driver commands. <BR>
 *    N8_*          RO: #define - Return codes from NetOctave API.
 *
 * @return
 *    N8_STATUS_OK                Success.
 *    N8_INVALID_OBJECT           Failed - invalid devnode, or NULL nspregs
 *                                pointer, or driver not loaded, or invalid
 *                                hardware instance.
 *    N8_UNEXPECTED_ERROR         Failed - the MMAP call failed (*highly* unlikely).
 *    N8_INVALID_DRIVER_VERSION   Failed - the driver installed is incompatible.             
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern N8_Status_t N8_OpenDevice(NSPdriverInfo_t *driverInfo_p, 
                                 N8_Boolean_t     allocUserPool,
                                 N8_Open_t        openMode);



/*****************************************************************************
 * N8_AllocateBuffer
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Allocates a buffer and maps it between user and kernel space.
 *
 * This routine follows two steps. It first requests that the driver allocate
 * memory with characteristics dictated by the caller.
 *
 * For more information on this call, please refer to the NSP2000 Device
 * Driver Specification Document.
 *
 * The N8_MemoryHandle_t should be treated as read-only upon return from this
 * call, for subsequent calls to N8_TestBuffer and N8_FreeBuffer.
 *
 * @parm
 *         size           R0: Size of allocation request in bytes.
 *
 * @return MemoryStruct*  RW: Pointer to a struct that associates the necessary
 *                            parameters that completely identify an allocated
 *                            buffer. NULL if allocation fails
 *
 * @par Externals:
 *    NSP_IOCTL_*   RO: #define - IOCTL codes for driver commands.     <BR>
 *                                  
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern N8_MemoryHandle_t * N8_AllocateBuffer(const unsigned int size);
extern N8_MemoryHandle_t * N8_AllocateBufferPK(const unsigned int size);



/*****************************************************************************
 * N8_FreeBuffer
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Frees buffers allocated with N8_AllocateBuffer.
 *
 * It requests that the driver free the buffer.
 *
 * @param MemoryStruct   RO: Pointer to a struct that associates the necessary
 *                           parameters that completely identify an allocated
 *                           buffer.
 *
 * @par Externals:
 *    NSP_IOCTL_*   RO: #define - IOCTL codes for driver commands.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern void N8_FreeBuffer(N8_MemoryHandle_t *MemoryStruct);


/*****************************************************************************
 * N8_CloseDevice
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Closes the NSP2000 device driver.
 *
 * This routine closes the NSP2000 driver, and is intended to be called just
 * before an application terminates.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern void N8_CloseDevice(void);


/*****************************************************************************
 * N8_WaitOnInterrupt
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Block for an interrupt from the NSP2000 device driver.
 *
 * This routine blocks until the NSP2000 device driver reports receipt of an
 * interrupt from the specified execution core, or until the timeout value is
 * reached. Must use a real handle.
 *
 * @param chip        RO: Chip number of chip we are waiting on.
 * @param coretype    RO: Selects which execution core to monitor. Must be
 *                        set to N8_DAPI_PKE, N8_DAPI_RNG, or N8_DAPI_EA.
 * @param bitmask     RO: Bitmask to be applied to interrupt register, to   
 *                        select which interrupts will trigger a return.
 * @param timeout     RO: Timeout value, in seconds.
 *
 * @par Externals:
 *    NSP_IOCTL_*   RO: #define - IOCTL codes for driver commands.  <BR>
 *    N8_*          RO: #define - Return codes from NetOctave API.
 *
 * @return 
 *    N8_STATUS_OK            Received at least one of the specified IRQs.
 *    N8_INVALID_PARAMETER    Invalid coretype.
 *    N8_EVENT_INCOMPLETE     Request timed out - IRQ not received.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern N8_Status_t N8_WaitOnInterrupt( N8_Unit_t      chip,
                                       unsigned char  coretype,
                                       unsigned long  bitmask,
                                       unsigned long  timeout );



/*****************************************************************************
 * N8_QMgrQueryStatistics
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Query the QMgr for its current statistics.
 *
 * This routine queries the driver for Queue Manager performance
 * statistics.
 *
 * @param stats     RW: Returns assorted Queue Manager stat counters. Also
 *                      selects which chip's queue to query via the chip
 *                      field.
 *
 * @par Externals:
 *    NSP_IOCTL_*   RO: #define - IOCTL codes for driver commands.  <BR>
 *    N8_*          RO: #define - Return codes from NetOctave API.
 *
 * @return 
 *    N8_STATUS_OK            Success
 *    N8_INVALID_PARAMETER    Invalid chip selector.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern N8_Status_t N8_QMgrQueryStatistics( N8_QueueStatistics_t *stats );

/*****************************************************************************
 * N8_DriverDebug
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Enables/disables driver debugging messages.
 *
 * This routine enables/disables debug messages in the driver, and/or displays
 * various resource usage info.
 *
 * @param Selector RO: Selects the debug message family.
 *
 * @par Externals:
 *    NSP_IOCTL_*   RO: #define - IOCTL codes for driver commands. <BR>
 *    NSP_DBG_*     RO: #define - Message family selector.         <BR>
 *    N8_*          RO: #define - Return codes from NetOctave API.
 *
 * @return 
 *    N8_INVALID_PARAMETER   Used invalid message family selector.
 *    N8_STATUS_OK           Success.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

N8_Status_t N8_DriverDebug(unsigned char Selector);

/*****************************************************************************
 * N8_QueryMemStatistics
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Query the driver for its memory statistics.
 *
 * This routine queries the driver for its memory statistics.
 *
 * @param stats     RW: Returns statistics for the drivers memory pool.
 *
 * @par Externals:
 *    NSP_IOCTL_*   RO: #define - IOCTL codes for driver commands.  <BR>
 *    N8_*          RO: #define - Return codes from NetOctave API.
 *
 * @return
 *    N8_STATUS_OK            Success
 *    N8_INVALID_PARAMETER    Invalid chip selector.
 *    N8_INVALID_OBJECT       Failed - invalid devnode or missing driver.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern N8_Status_t N8_QueryMemStatistics( MemStats_t *stats );


/*****************************************************************************
 * N8_GetFD
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Return the file descriptor for the NSP2000.
 *
 * This routine returns the file descriptor for the NSP2000.
 *
 * @param
 *
 * @par Externals:
 *    nspDeviceHandle_g   RO: global file descriptor for the NSP2000.
 *
 * @return
 *   int - The NSP2000's file descriptor.
 *
 * @par Errors:
 *****************************************************************************/

extern int N8_GetFD(void);


/*****************************************************************************
 * N8_ContextMemAlloc
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Allocate an entry from the context memory.
 *
 * This routine allocates and entry from the context memory on an NPS2000.
 *
 * @param chip            RO: The chip.
 *        index           RW: Pointer to index of the allocated entry.
 *
 * @return
 *    Returns the index of the context memory allocation.
 *    -1 - The allocation has failed.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern N8_Status_t N8_ContextMemAlloc (N8_Unit_t *chip, unsigned int *index_p);

/*****************************************************************************
 * N8_ContextMemFree
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Free a context memory entry.
 *
 * This routine frees a context memory entry on an NPS2000.
 *
 * @param chip            RO: The chip.
 *        entry           R0: The index of the entry to be freed.
 *
 * @return
 *
 *****************************************************************************/

extern N8_Status_t N8_ContextMemFree  (N8_Unit_t chip, unsigned long entry);

/*****************************************************************************
 * N8_ContextMemValidate
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Validate a context memory entry.
 *
 * This routine validates a context memory entry on an NPS2000.
 *
 * @param chip            RO: The chip.
 *        index           R0: The index of the entry to be validated.
 *
 * @return
 *
 *****************************************************************************/

extern N8_Status_t N8_ContextMemValidate (N8_Unit_t chip, unsigned int index);

/*****************************************************************************
 * N8_QMgrQueue
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Queue an API request to the QMgr.
 *
 * This routine Queues an API request to the QMgr.
 *
 * @param API_req_p   RO: Pointer to the request to be queued.
 *
 * @par Externals:
 *    nspDeviceHandle_g      RO: File descriptor for the NSP2000
 *
 * @return
 *
 *****************************************************************************/

extern N8_Status_t N8_QMgrQueue( API_Request_t *API_req_p );


/*****************************************************************************
 * Queue_RN_request
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Queue a random number request.
 *
 * @param RN_req_p        RO: Pointer to the information structure for a
 *                            random number request.
 *
 * @par Externals:
 *    nspDeviceHandle_g      RO: File descriptor for the NSP2000
 *
 * @return
 *
 *****************************************************************************/

extern N8_Status_t Queue_RN_request( RN_Request_t *rn_req_p );

/*****************************************************************************
 * RN_SetParameters
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Set random number parameters
 *
 * @param parms_p         RO: Pointer to the information structure holding
 *                            parameters.
 *        chip            R0: The chip to set the parameters on.
 *
 * @par Externals:
 *    nspDeviceHandle_g   RO: File descriptor for the NSP2000
 *
 * @return
 *
 *****************************************************************************/

extern N8_Status_t RN_SetParameters(N8_RNG_Parameter_t *parms_p, int chip);

/*****************************************************************************
 * RN_GetParameters
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Get random number parameters
 *
 * @param parms_p         RO: Pointer to the information structure holding
 *                            parameters.
 *        chip            R0: The chip to get the parameters on.
 *
 * @par Externals:
 *    nspDeviceHandle_g   RO: File descriptor for the NSP2000
 *
 * @return
 *
 *****************************************************************************/

extern N8_Status_t RN_GetParameters(N8_RNG_Parameter_t *parms_p, int chip);

/*****************************************************************************
 * N8_QMgrDequeue
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Inform the driver a request has been processed
 *
 * This routine informs the driver a request has been processed
 *
 * @param
 *
 * @par Externals:
 *    nspDeviceHandle_g      RO: File descriptor for the NSP2000
 *
 * @return
 *
 *****************************************************************************/

extern N8_Status_t N8_QMgrDequeue(void);

extern N8_Status_t N8_DiagInfo(int chip, int *regAddr_p, int *eaQueAddr_p,
		               int *pkQueAddr_p, int *rnQueAddr_p);
extern N8_Status_t N8_WaitOnRequest(int timeout);

#endif   /* N8_DRIVER_API_H */


