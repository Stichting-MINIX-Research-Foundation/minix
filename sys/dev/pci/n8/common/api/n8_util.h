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
 * @(#) n8_util.h 1.65@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_util.h
 *  @brief Header file for utility functions
 *
 * Utility function prototypes
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/18/03 brr   Eliminate unused parameter passage.
 * 06/06/03 brr   Move n8_enums to public include as  n8_pub_enums.
 * 05/19/03 brr   Remove obsolete prototypes, conver freeRequest to a macro.
 * 03/10/03 brr   Added support for API callbacks.
 * 07/14/02 bac   Corrected chip type to be the standard N8_Unit_t.
 * 06/10/02 hml   Added proto for initializeEARequestBuffer.
 * 05/07/02 msz   Mark synchronous requests so QMgr will pend until they are
 *                done.
 * 05/07/02 bac   Changed output of RESULT_HANDLER_WARNING macro to print the
 *                error code in hex.
 * 03/28/02 hml   Updated QUEUE_AND_CHECK, since N8_QMgrQueue now returns
 *                an N8_Status_t. 
 * 03/26/02 hml   Added proto for n8_validateUnit.
 * 03/26/02 brr   Allocate the data buffer as part of the API request. 
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/25/02 brr   Updated RESULT_HANDLER_WARNING to use N8_PRINT.
 * 11/27/01 bac   Added prototype for n8_handleEvent.
 * 11/13/01 dkm   Removed unused DELETE_OBJECT and CREATE_KOBJECT macros.
 * 11/14/01 bac   Added error message to QUEUE_AND_CHECK.
 * 11/11/01 bac   Added n8_sizedBufferCmp prototype.
 * 11/06/01 hml   Added the CHECK_STRUCTURE macro and cleaned up a little.
 * 10/30/01 bac   Added functions n8_decrLength64 and n8_incrLength64.
 * 10/11/01 brr   Modified QUEUE_AND_CHECK macros to pass back different error
 *                code. This fixed a memory leak when queue operation failed.
 * 10/11/01 hml   Added n8_strdup prototype.
 * 10/08/01 hml   Added printHWError
 * 10/02/01 bac   Added support for n8_enums.  Created RESULT_HANDLER_WARNING.
 * 09/20/01 bac   Tightened up some signatures to have unsigned ints and const
 *                modifiers. 
 * 09/18/01 bac   Added CEIL macro, changed signature to createEARequestBuffer
 *                to add numCmds.
 * 09/14/01 bac   Moved debug command block printing macros and prototypes to
 *                this common location.
 * 08/30/01 msz   Include "QMQueue.h" for QMgrQueue to prevent a warning.
 * 08/16/01 mmd   Now including n8_driver_api.h instead of simon.h.
 * 08/20/01 msz   Fixed REQUEST_SUCCESSFULLY_ENQUEUEDs to be N8_QUEUE_SUCCESS,
 *                as these were equivalent,
 *                and ENQUEUE_REQUEST_SUCCESSFULLY_ENQUEUED was removed.
 *                Also go directly to QM rather than EA or PK.
 * 07/30/01 bac   Fixed HANDLE_EVENT to check return code in the event.
 * 07/20/01 bac   Added chip id to calls to create[PK|EA]RequestBuffer.
 * 07/05/01 hml   Copy unit type from request to event in HANDLE_EVENT
 * 06/25/01 bac   Added CREATE_UOBJECTSET macro
 * 06/19/01 bac   Added CREATE_UOBJECT macro
 * 06/19/01 bac   Added prototype for addMemoryStructToFree and updated
 *                freeRequest to handle N8_MemoryHandle_t entries
 * 06/17/01 bac   Added removeBlockFromFreelist prototype.
 * 05/18/01 bac   Removed unnecessary check for null in DELETE_OBJECT and
 *                redundant memset in CREATE_OBJECT.
 * 05/04/01 bac   Added macro CHECK_RETURN
 * 04/26/01 bac   Added prototypes for create[PK|EA]RequestBuffer
 * 04/24/01 bac   Original version.
 ****************************************************************************/

#ifndef N8_UTIL_H
#define N8_UTIL_H

#include "n8_common.h"
#include "n8_pub_errors.h"
#include "n8_enqueue_common.h"
#include "n8_malloc_common.h"
#include "n8_driver_api.h"
#include "n8_pub_enums.h"

/* macros */
/* round up to the next word boundary */
#define NEXT_WORD_SIZE(_D) ((((_D)+3)>>2)<<2)

#define CEIL(__A,__B) (((__A)+(__B)-1)/(__B))

#define HANDLE_EVENT(__EV_P, __REQ_P, __RTN)                      \
   (__RTN) = n8_handleEvent((__EV_P), (void *) (__REQ_P),         \
                            (__REQ_P)->qr.unit,                   \
                            (__REQ_P)->qr.requestStatus);


#define N8_SET_EVENT_FINISHED(__EP, __UNIT)     \
   (__EP)->unit = (__UNIT);                     \
   (__EP)->state = NULL;                        \
   (__EP)->status = N8_QUEUE_REQUEST_FINISHED;


#define CHECK_RETURN(RET)                                         \
   if ((RET) != N8_STATUS_OK)                                     \
   {                                                              \
       break;                                                     \
   }                                   

#define CHECK_OBJECT(OBJ, RETRN)                \
      if ((OBJ) == NULL)                        \
      {                                         \
         (RETRN) = N8_INVALID_OBJECT;           \
         break;                                 \
      }

#define CHECK_STRUCTURE(STRUCT, ID, RETRN)      \
      if ((STRUCT) != (ID))                     \
      {                                         \
         (RETRN) = N8_INVALID_OBJECT;           \
         break;                                 \
      }

#define CREATE_UOBJECT(OBJ, _TYPE, RETRN)        \
      OBJ = (_TYPE *) N8_UMALLOC(sizeof(_TYPE)); \
      if (OBJ == NULL)                           \
      {                                          \
         RETRN = N8_MALLOC_FAILED;               \
         break;                                  \
      }

#define CREATE_UOBJECTSET(OBJ, _TYPE, _NUM, RETRN)        \
      OBJ = (_TYPE *) N8_UMALLOC(sizeof(_TYPE) * (_NUM)); \
      if (OBJ == NULL)                                    \
      {                                                   \
         RETRN = N8_MALLOC_FAILED;                        \
         break;                                           \
      }

#define CREATE_UOBJECT_SIZE(OBJ, SIZE, RETRN)   \
      OBJ = N8_UMALLOC(SIZE);                   \
      if (OBJ == NULL)                          \
      {                                         \
         RETRN = N8_MALLOC_FAILED;              \
         break;                                 \
      }

/* Note RNG requests currently do not use QUEUE_AND_CHECK */
#ifdef SUPPORT_CALLBACKS
#define QUEUE_AND_CHECK(__EV_P,__CMD_P, _RTN)                              \
 {                                                                         \
   if (__EV_P == NULL)                                                     \
   {                                                                       \
       __CMD_P->qr.synchronous = N8_TRUE;                                  \
   }                                                                       \
   else                                                                    \
   {                                                                       \
       __CMD_P->usrCallback = __EV_P->usrCallback;                         \
       __CMD_P->usrData     = __EV_P->usrData;                             \
   }                                                                       \
   _RTN = N8_QMgrQueue((__CMD_P));                                         \
   if (_RTN != N8_STATUS_OK)                                               \
   {                                                                       \
      break;                                                               \
   }                                                                       \
 }
#else
#define QUEUE_AND_CHECK(__EV_P,__CMD_P, _RTN)                              \
 {                                                                         \
   if (__EV_P == NULL)                                                     \
   {                                                                       \
       __CMD_P->qr.synchronous = N8_TRUE;                                  \
   }                                                                       \
   _RTN = N8_QMgrQueue((__CMD_P));                                         \
   if (_RTN != N8_STATUS_OK)                                               \
   {                                                                       \
      break;                                                               \
   }                                                                       \
 }
#endif

#define freeRequest(_REQ_P)                                                \
   if (_REQ_P != NULL && ((API_Request_t *)_REQ_P)->userRequest != N8_TRUE)                   \
   {                                                                       \
      N8_KFREE((N8_MemoryHandle_t *)((int)_REQ_P-N8_BUFFER_HEADER_SIZE));  \
   }                                                                       \


#define RESULT_HANDLER_WARNING(__TITLE__, __REQ_P__)                       \
      N8_PRINT("%s called with error.\n"                                   \
             "simon error %x on command %d of %d\n",                       \
             (__TITLE__),                                                  \
             (__REQ_P__)->err_rpt_bfr.errorReturnedFromSimon,              \
             (__REQ_P__)->err_rpt_bfr.indexOfCommandThatCausedError,       \
             (__REQ_P__)->numNewCmds);                                     \
      printHWError((__REQ_P__)->err_rpt_bfr.errorReturnedFromSimon,        \
                   (__REQ_P__)->qr.unit);       

/* debugging macros */
#ifdef N8DEBUG
#define DBG_PRINT_PK_CMD_BLOCKS(A,B,C) \
   printPKCommandBlocks((A),(B),(C))

#define DBG_PRINT_EA_CMD_BLOCKS(A,B,C) \
   printEACommandBlocks((A),(B),(C))
#else
#define DBG_PRINT_PK_CMD_BLOCKS(A,B,C)
#define DBG_PRINT_EA_CMD_BLOCKS(A,B,C)
#endif

/* Prototypes */
void printN8Buffer(N8_Buffer_t *p, const unsigned int size);
void n8_displayBuffer(N8_Buffer_t *buf_p, const uint32_t length,
                      const char *name);
void initializeEARequestBuffer(API_Request_t      *req_p,
                               N8_MemoryHandle_t  *kmem_p,
                               const N8_Unit_t     chip,
                               const unsigned int  numCmds,
                               const void         *callbackFcn,
                               N8_Boolean_t        userRequest);

N8_Status_t createEARequestBuffer(API_Request_t **req_pp,
                                  const N8_Unit_t chip,
                                  const unsigned int numCmds,
                                  const void *callbackFcn,
                                  const unsigned int dataBytes);

N8_Status_t createPKRequestBuffer(API_Request_t **req_pp,
                                  const N8_Unit_t chip,
                                  const unsigned int numCmds,
                                  const void *callbackFcn,
                                  const unsigned int dataBytes);

void printCommandBlock(uint32_t *u, unsigned int size);
void printPKCommandBlocks(const char *title, PK_CMD_BLOCK_t *block_p, uint32_t num_blocks);
void printEACommandBlocks(const char *title, EA_CMD_BLOCK_t *block_p, uint32_t num_blocks);
void printHWError(uint32_t errCode, N8_Component_t unit);
char *n8_strdup(const char *str_p);
void n8_decrLength64(uint32_t *hi_word_p,
                     uint32_t *lo_word_p,
                     const uint32_t length);
void n8_incrLength64(uint32_t *hi_word_p,
                     uint32_t *lo_word_p,
                     const uint32_t  length);

int n8_sizedBufferCmp(const N8_SizedBuffer_t *a_p,
                      const N8_SizedBuffer_t *b_p);
void resultHandlerGeneric(API_Request_t* req_p);
N8_Status_t n8_handleEvent(N8_Event_t *e_p, void *req_p,
                           N8_Unit_t unit,
                           N8_QueueStatusCodes_t requestStatus);
N8_Boolean_t n8_validateUnit(N8_Unit_t chip);

N8_Status_t queueEvent(N8_Event_t *event_p);
N8_Status_t callbackInit(uint32_t numEvents, uint32_t timeout);
N8_Status_t callbackShutdown(void);
N8_Status_t n8_getConfigInfo(N8_ConfigAPI_t *config_p);

#endif /* N8_UTIL_H */
