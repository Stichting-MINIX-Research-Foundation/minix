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
 * @(#) n8_pub_errors.h 1.17@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_errors.h
 *  @brief Error conditions/return codes.
 *
 * Return codes for public API defined here.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 03/10/03 brr   Added error codes for API callbacks.
 * 10/22/02 brr   Added N8_INCOMPATIBLE_OPEN.
 * 10/10/02 brr   Added N8_TIMEOUT.
 * 04/30/02 brr   Added N8_API_QUEUE_FULL.
 * 04/12/02 hml   Added N8_HARDWARE_UNAVAILABLE.
 * 04/03/02 brr   Added N8_INVALID_DRIVER_VERSION.
 * 03/28/01 hml   Added N8_QUEUE_FULL.
 * 10/30/01 dkm   Added N8_INVALID_UNIT.
 * 10/16/01 hml   Added N8_QUEUE_ERROR.
 * 10/12/01 dkm   Moved to include_public directory.
 * 09/06/01 hml   Added the N8_STATUS_ATTACH_OK value.
 * 06/30/01 arh   Added documentation to N8_Status_t enumeration values and
 *                assigned specific values to each. 
 *                Added N8_ALREADY_INITIALIZED.
 * 06/14/01 bac   Added N8_INVALID_VERSION.
 * 05/30/01 mlando Added N8_VERIFICATION_FAILED, because IPSec interface changed
 * 05/30/01 bac   Removed N8_VERIFICATION_FAILED
 * 05/11/01 bac   Added N8_VERIFICATION_FAILED
 * 04/10/01 bac   Standardized by converting enums to uppercase.
 * 03/10/01 bac   Original version.
 ****************************************************************************/

#ifndef N8_PUB_ERRORS_H
#define N8_PUB_ERRORS_H

#if 0
#ifdef __cplusplus
extern "C"
{
#endif
#endif

/* These enumeration constants are the API error return values.
   All user-visible API routines return a value of this type.
   Note the following:
      1. NO CODE, INTERNAL (i.e., written by NetOctave) OR
         EXTERNAL, SHOULD ASSUME THAT N8_STATUS_OK = 0.
      2. Each enumeration value should have its value
         explicitly set, so the values can be determined
         easily from this file.
      3. Once defined, values should never be changed, for
         reasons of backwards compatibility etc.
      4. This also means if an enumeration constant is 
         ever deleted, its value should be left unused
         and not re-assigned to a new constant.              */
         
typedef enum
{
   N8_STATUS_OK                 =   0, /* The call completed successfully.*/
   N8_EVENT_COMPLETE            =   1, /* The asynchronous event has 
                                          completed.*/
   N8_EVENT_INCOMPLETE          =   2, /* The asynchronous event has not 
                                          completed. */
   N8_INVALID_INPUT_SIZE        =   3, /* An input size or length parameter is 
                                          outside its legal value range. */
   N8_INVALID_OUTPUT_SIZE       =   4, /* An output size or length parameter is 
                                          outside its required value range.  */
   N8_INVALID_ENUM              =   5, /* An input enumerated constant is not a 
                                          recognized value for that enumerated 
                                          type. */
   N8_INVALID_PARAMETER         =   6, /* An input parameter is of the wrong 
                                          type. */
   N8_INVALID_OBJECT            =   7, /* An input object is not of the right    
                                          type, not correctly initialized, or  
                                          contains bad or invalid values. */
   N8_INVALID_KEY               =   8, /* The type or value of a key is illegal 
                                          for the required key.   */
   N8_INVALID_KEY_SIZE          =   9, /* The size or length of a key is 
                                          outside its legal value range.   */
   N8_INVALID_PROTOCOL          =  10, /* The specified communication protocol 
                                          is not a recognized value.  */
   N8_INVALID_CIPHER            =  11, /* The specified cipher/encryption 
                                          algorithm is not a recognized 
                                          value.    */
   N8_INVALID_HASH              =  12, /* The specified hash algorithm is not 
                                          a recognized/supported value. */
   N8_INVALID_VALUE             =  13, /* A specified parameter has a value
                                          that is not legal. */
   N8_INVALID_VERSION           =  14, /* The version number in an SSL or
                                          TLS version is not a supported
                                          version.  */
   N8_INCONSISTENT              =  15, /* An input argument's internal state 
                                          is inconsistent, or the values of two 
                                          or more input parameters are 
                                          inconsistent with one another.  */
   N8_NOT_INITIALIZED           =  16, /* A required resource or input object 
                                          has not been properly initialized. */
   N8_UNALLOCATED_CONTEXT       =  17, /* The specified context entry is not 
                                          allocated and cannot be used in this 
                                          call.        */
   N8_NO_MORE_RESOURCE          =  18, /* The call failed due to exhaustion of 
                                          a required resource.  */
   N8_HARDWARE_ERROR            =  19, /* The hardware has detected an error.
                                          It may be an inconsistency with the
                                          operands (e.g. they are not relatively
                                          prime), a timing issue, or other
                                          problem.  It DOES NOT indicate that
                                          the hardware is actually faulty. */
   N8_UNEXPECTED_ERROR          =  20, /* Some other unexpected, unrecognized
                                          error has occurred.   */
   N8_UNIMPLEMENTED_FUNCTION    =  21, /* The called interface is not
                                          implemented in this release. */
   N8_MALLOC_FAILED             =  22, /* An internal memory allocation
                                          operation failed. */
   N8_WEAK_KEY                  =  23, /* A cryptographically unsecure key 
                                          was provided.  */
   N8_VERIFICATION_FAILED       =  24, /* The MAC computed from a received SSL, 
                                          TLS, or IPSec packet did not equal 
                                          the MAC contained in the packet.  */
   N8_ALREADY_INITIALIZED       =  25, /* An attempt has been made to 
                                          initialize an already initialized
                                          resource. */
   N8_FILE_ERROR                =  26, /* A file I/O error occurred trying
                                          to open or read a file. */
   N8_STATUS_ATTACH_OK          =  27, /* A shared resource was successfully
                                          attached to (not created) */
   N8_QUEUE_ERROR               =  28, /* A queueing error occurred when
                                          posting an operation to the QMgr */
   N8_INVALID_UNIT              =  29, /* The specified unit is out of range 
                                          of valid or available unit values.*/
   N8_UNALIGNED_ADDRESS         =  30, /* The passed address is not 32-bit 
                                          aligned */
   N8_QUEUE_FULL                =  31, /* Posting an operation to the QMgr
                                          failed because the cmd queue is full. */
   N8_INVALID_DRIVER_VERSION    =  32, /* The SDK library is not compatible
                                          with the installed driver.        */
   N8_HARDWARE_UNAVAILABLE      =  33, /* No NetOctave Hardware is available 
                                          or the driver is not loaded or the
                                          driver has malfunctioned. */
   N8_API_QUEUE_FULL            =  34, /* Posting an operation to the QMgr
                                          failed because the API queue is full. */
   N8_RNG_QUEUE_EMPTY           =  35, /* The RNG queue did not have a sufficient
                                          number of bytes available to honor the
                                          RNG request. */
   N8_TIMEOUT                   =  36, /* The operation has timed out. */
   N8_INCOMPATIBLE_OPEN         =  37, /* The driver has been opened to */
                                       /* perform a different operation */
                                       /* & must be restarted.          */
   N8_EVENT_ALLOC_FAILED        =  38, /* The allocation of the event array */
                                       /* failed.                           */
   N8_THREAD_INIT_FAILED        =  39, /* The callback thread could not be  */
                                       /* intiialized.                      */
   N8_EVENT_QUEUE_FULL          =  40  /* The event array is full.          */

} N8_Status_t;

#if 0
#ifdef __cplusplus
}
#endif
#endif

#endif /* N8_PUB_ERRORS_H */

