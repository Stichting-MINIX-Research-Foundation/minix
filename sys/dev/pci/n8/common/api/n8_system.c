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

static char const n8_id[] = "$Id: n8_system.c,v 1.2 2014/03/25 16:19:14 christos Exp $";
/*****************************************************************************/
/** @file n8_system.c
 *  @brief Implements the API call N8_GetSystemParameter, which allows users to
 *  query the system for characteristics as defined in the enumeration
 *  N8_Parameter_t.
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 *
 * 05/16/03 brr   Eliminate obsolete include file.
 * 03/10/03 brr   Added N8_INITIALIZE_INFO parameter to N8_GetSystemParameter.
 * 03/02/03 bac   Added support for N8_HARDWAREREVISION.
 * 07/08/02 brr   Added N8_FILEDESCRIPTOR parameter to N8_GetSystemParameter.
 * 04/05/02 brr   Added N8_SWVERSIONTEXT parameter to N8_GetSystemParameter.
 * 04/03/02 brr   Use version identification from n8_version.h. Also added 
 *                N8_PrintSoftwareVersion api call to print version info.
 * 02/25/02 brr   Updated for 2.1 release. Use a single call to obtain driver
 *                information & removed all QMgr references.
 * 10/30/01 hml   First working version.
 * 06/10/01 mel   Original version.
 ****************************************************************************/
/** @defgroup SystemInfo System Parameter retrieval
 */

#include "n8_util.h"
#include "n8_API_Initialize.h"
#include "n8_device_info.h"
#include "n8_version.h"

static N8_Status_t setCount(N8_Buffer_t *value_p, N8_Status_t type);
static N8_Status_t setType(N8_Buffer_t *value_p, N8_Status_t type);
static N8_Status_t setNumberOfChips(N8_Buffer_t *value_p);
static N8_Status_t setHWversion(N8_Buffer_t *value_p);
static N8_Status_t setHWrevision(N8_Buffer_t *value_p);
static N8_Status_t setSWversion(N8_Buffer_t *value_p);
static N8_Status_t setContextSize(N8_Buffer_t *value_p);
static N8_Status_t setSKSsize(N8_Buffer_t *value_p);
static N8_Status_t setSWversionText(N8_Buffer_t *value_p, size_t value_l);
static N8_Status_t setFD(N8_Buffer_t *value_p);
static N8_Status_t setInitInfo(N8_Buffer_t *value_p);

extern NSPdriverInfo_t  nspDriverInfo;

/*****************************************************************************
 * N8_GetSystemParameter
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Allows the caller to determine the value of various NSP2000 and API
 * system and configuration values.
 *
 * The configuration parameter desired is determined by the value specified in
 * Parameter. Note that the hash units are currently being treated the same
 * as the EA units since the NSP2000 does not have a separate hash core.
 *
 *  @param parameter   RO:  A constant naming the configuration value to 
 *                          return.
 *  @param value_p     WO:  A pointer to where to return the value(s) of the
 *                          requested system parameter. The format (type) of
 *                          what is returned depends on the value of
 *                          Parameter.
 *
 * @return
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 * @par Errors
 *      N8_INVALID_ENUM -     The value of Parameter is not one of the 
 *                            defined valid configuration enumerations. 
 *      N8_INVALID_OBJECT     The output parameter is NULL.
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_GetSystemParameter(N8_Parameter_t parameter, void *value_p,
    size_t value_l)
{
   N8_Status_t ret = N8_STATUS_OK;

   DBG(("N8_GetSystemParameter\n"));
   do
   {
       ret = N8_preamble();
       CHECK_RETURN(ret);

       /* verify value object */
       CHECK_OBJECT(value_p, ret);

       switch (parameter)
       {
           case N8_EACOUNT:
              ret = setCount(value_p, N8_EA);
              break;
           case N8_EATYPE:
              ret = setType(value_p, N8_EA);
              break;
           case N8_PKCOUNT:
              ret = setCount(value_p, N8_PKP);
              break;
           case N8_PKTYPE:
              ret = setType(value_p, N8_PKP);
              break;
           case N8_HPCOUNT:
              ret = setCount(value_p, N8_EA);
              break;
           case N8_HPTYPE:
              ret = setType(value_p, N8_EA);
              break;
           case N8_HARDWAREVERSION:
              ret = setHWversion(value_p);
              break;
           case N8_HARDWAREREVISION:
              ret = setHWrevision(value_p);
              break;
           case N8_SOFTWAREVERSION:
              ret = setSWversion(value_p);
              break;
           case N8_CONTEXTMEMSIZE:
              ret = setContextSize(value_p);
              break;
           case N8_SKSMEMSIZE:
              ret = setSKSsize(value_p);
              break;
           case N8_NUMBEROFCHIPS:
              ret = setNumberOfChips(value_p);
              break;
           case N8_SWVERSIONTEXT:
              ret = setSWversionText(value_p, value_l);
              break;
           case N8_INITIALIZE_INFO:
              ret = setInitInfo(value_p);
              break;
           case N8_FILEDESCRIPTOR:
              ret = setFD(value_p);
              break;
           default:
              /* invalid parameter */
              DBG(("Invalid parameter\n"));
              DBG(("N8_GetSystemParameter - return Error\n"));
              ret = N8_INVALID_ENUM;
              break;
       } /* switch */
   }while (FALSE);
   DBG(("N8_GetSystemParameter - OK\n"));
   return ret;
} /* N8_GetSystemParameter */


/*****************************************************************************
 * setCount
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Get the count of units.
 *
 * Currently all of the hardware or emulation types have only one unit of any
 * type.  As soon as this ceases to be true, we will need to call the driver
 * function N8_GetConfigurationItem or depend on knowledge of the 
 * hardware type being stored in the QueueControl structure.
 *
 *
 * @param value_p WO: Pointer in which to store the number of units.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *    N8_UNEXPECTED_ERROR: The hardware type in one of the queues
 *                         was not recognized.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT and N8_UNEXPECTED_ERROR as described in the 
 *    return section.
 *   
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete.
 *****************************************************************************/

static N8_Status_t setCount(N8_Buffer_t *value_p, N8_Component_t type)
{
    N8_Status_t     ret = N8_STATUS_OK;
    int             nStructs;
    unsigned int    nDevices = 0;
    int             i;

    DBG(("setCount\n"));
    do
    {
       CHECK_OBJECT(value_p, ret);

       nStructs = nspDriverInfo.numChips;

       for (i = 0; i < nStructs; i++)
       {

          switch (nspDriverInfo.chipInfo[i].hardwareType)
          {
             /* Note that all of these devices have only one unit
                per device.  This may well change with future devices.
              */
             case N8_FPGA:
             case N8_BM:
             case N8_NSP2000_HW:
                nDevices ++;
                break;
             default:
                ret = N8_UNEXPECTED_ERROR;
          }
       }
       CHECK_RETURN(ret);

    }while (FALSE);

    DBG(("setCount - OK\n"));

    if (ret == N8_STATUS_OK)
    {
       memcpy(value_p, &nDevices, sizeof(int));
    }
    return ret;
} /* setCount */

/*****************************************************************************
 * setType
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Get types of units.
 *
 * Currently all of the hardware or emulation types have only one unit of any
 * type.  When this ceases to be true, we will need to revisit this call.
 *
 * @param value_p WO: Pointer in which to store unit types.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *    N8_UNEXPECTED_ERROR: The hardware type in one of the queues
 *                         was not recognized.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT and N8_UNEXPECTED_ERROR as described in the 
 *    return section.
 *   
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" array.
 *****************************************************************************/

static N8_Status_t setType(N8_Buffer_t *value_p, N8_Component_t type)
{
    N8_Status_t     ret = N8_STATUS_OK;
    int             nStructs;
    int             i;
    unsigned int   *vPtr_p;

    DBG(("setType\n"));
    do
    {
       CHECK_OBJECT(value_p, ret);

       nStructs = nspDriverInfo.numChips;

       vPtr_p = (unsigned int *)value_p;

       for (i = 0; i < nStructs; i++)
       {
          switch (nspDriverInfo.chipInfo[i].hardwareType)
          {
             /* Note that all of these devices have only one unit
                per device.  This may well change with future devices.
              */
             case N8_FPGA:
             case N8_BM:
                *vPtr_p = N8_NSP2000EMULATED;
                break;
             case N8_NSP2000_HW:
                *vPtr_p = N8_NSP2000;
                break;
             default:
                ret = N8_UNEXPECTED_ERROR;
          }
          vPtr_p ++;
       }
       CHECK_RETURN(ret);

    }while (FALSE);

    DBG(("setType - OK\n"));

    return ret;
} /* setType */
/*****************************************************************************
 * setHWversion
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Gets the hardware version or returns an emulation type for each
 * chip or emulation thereof in the current system.
 *
 * @param value_p WO: Pointer in which to store chip types.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *    N8_UNEXPECTED_ERROR: The hardware type in one of the queues
 *                         was not recognized.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT and N8_UNEXPECTED_ERROR as described in the 
 *    return section.
 *   
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" array.
 *****************************************************************************/
static N8_Status_t setHWversion(N8_Buffer_t *value_p)
{
   N8_Status_t     ret = N8_STATUS_OK;
   int             nStructs;
   int             i;
   unsigned int   *vPtr_p;
   
   DBG(("setHWversion\n"));
   do
   {
      CHECK_OBJECT(value_p, ret);
   
      nStructs = nspDriverInfo.numChips;

      vPtr_p = (unsigned int *) value_p;

      for (i = 0; i < nStructs; i++)
      {
         switch (nspDriverInfo.chipInfo[i].hardwareType)
         {
            /* Note that we only care to know whether or not there is real
               hardware for this queue.
             */
            case N8_FPGA:
            case N8_BM:
               *vPtr_p = N8_NSP2000EMULATED;
               break;
            case N8_NSP2000_HW:
               *vPtr_p = nspDriverInfo.chipInfo[i].HardwareVersion;
               break;
            default:
               ret = N8_UNEXPECTED_ERROR;
         }
         vPtr_p ++;
      }
      CHECK_RETURN(ret);

   }while (FALSE);

   DBG(("setHWversion - OK\n"));

   return ret;
} /* setHWversion */

/*****************************************************************************
 * setHWrevision
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Gets the hardware revision from the PCI interface.  This value is the
 * same as returned by 'lspci' under Linux.
 *
 * @param value_p WO: Pointer in which to store chip types.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *    N8_UNEXPECTED_ERROR: The hardware type in one of the queues
 *                         was not recognized.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT and N8_UNEXPECTED_ERROR as described in the 
 *    return section.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" array.
 *****************************************************************************/
static N8_Status_t setHWrevision(N8_Buffer_t *value_p)
{
   N8_Status_t     ret = N8_STATUS_OK;
   int             nStructs;
   int             i;
   unsigned int   *vPtr_p;
   
   DBG(("setHWrevision\n"));
   do
   {
      CHECK_OBJECT(value_p, ret);
   
      nStructs = nspDriverInfo.numChips;

      vPtr_p = (unsigned int *) value_p;

      for (i = 0; i < nStructs; i++)
      {
         *vPtr_p = (unsigned int) nspDriverInfo.chipInfo[i].RevisionID;
         vPtr_p ++;
      }

   } while (FALSE);

   DBG(("setHWrevision - OK\n"));

   return ret;
} /* setHWrevision */

/*****************************************************************************
 * setSWVersion
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Get the current version of the software.
 *
 * This function returns the current major and minor revision numbers of the
 * SDK as specified by the N8_MAJOR_REVISION and N8_MINOR_REVISION #defines
 * at the top of this file.  When the software revision is changed, change
 * these defines.
 *
 * @param value_p WO: Pointer in which to store revision info.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT as described in the return section.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" location.
 *****************************************************************************/
static N8_Status_t setSWversion(N8_Buffer_t *value_p)
{
    N8_Status_t  ret = N8_STATUS_OK;
    unsigned int version = 0;

    DBG(("setSWversion\n"));
    do
    {
       CHECK_OBJECT(value_p, ret);
       version = N8_VERSION;
       memcpy(value_p, &version, sizeof(unsigned int));
    }while (FALSE);
    DBG(("setSWversion - OK\n"));
    return ret;
} /* setSWversion */

/*****************************************************************************
 * setContextSize
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Get the sizes of the context memory for all EA units.
 *
 * Currently all of the hardware or emulation types have only one unit of any
 * type.  When this ceases to be true, we will need to revisit this call. 
 *
 * @param value_p WO: Pointer in which to store unit types.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT as described in the return section.
 *   
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" array.
 *****************************************************************************/
static N8_Status_t setContextSize(N8_Buffer_t *value_p)
{
    N8_Status_t     ret = N8_STATUS_OK;
    int             nStructs;
    int             i;
    unsigned long  *vPtr_p;

    DBG(("setContextSize\n"));
    do
    {
       CHECK_OBJECT(value_p, ret);

       nStructs = nspDriverInfo.numChips;

       vPtr_p = (unsigned long *) value_p;

       for (i = 0; i < nStructs; i++)
       {
          memcpy(vPtr_p, &nspDriverInfo.chipInfo[i].contextMemsize, sizeof(int));
          vPtr_p ++;
       }
       CHECK_RETURN(ret);
    }while (FALSE);

    DBG(("setContextSize - OK\n"));
    return ret;
} /* setContextSize */

/*****************************************************************************
 * setSKSsize
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Get the sizes of the SKS memory for all PKP units.
 *
 * Currently all of the hardware or emulation types have only one unit of any
 * type.  When this ceases to be true, we will need to revisit this call. 
 *
 * @param value_p WO: Pointer in which to store unit types.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT as described in the return section.
 *   
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" array.
 *****************************************************************************/
static N8_Status_t setSKSsize(N8_Buffer_t *value_p)
{
    N8_Status_t     ret = N8_STATUS_OK;
    int             nStructs;
    int             i;
    unsigned long    *vPtr_p;

    DBG(("setSKSSize\n"));
    do
    {
       CHECK_OBJECT(value_p, ret);

       nStructs = nspDriverInfo.numChips;

       vPtr_p = (unsigned long *)value_p;

       for (i = 0; i < nStructs; i++)
       {
          memcpy(vPtr_p, &nspDriverInfo.chipInfo[i].SKS_size, sizeof(unsigned long));
          vPtr_p ++;
       }
       CHECK_RETURN(ret);
    }while (FALSE);

    DBG(("setSKSSize - OK\n"));
    return ret;
} /* setSKSsize */

/*****************************************************************************
 * setNumberOfChips
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Get the number of chips on the system.
 *
 * Because we also want to include the number of "emulated" chips on the system,
 * we can simply return the number of control structures on the system.
 *
 * @param value_p WO: Pointer in which to store unit types.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT as described in the return section.
 *   
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" memory location.
 *****************************************************************************/
static N8_Status_t setNumberOfChips(N8_Buffer_t *value_p)
{
    N8_Status_t   ret = N8_STATUS_OK;
    int           nStructs;

    DBG(("setNumberOfChips\n"));

    nStructs = nspDriverInfo.numChips;

    DBG(("setNumberOfChips - OK\n"));
    memcpy(value_p, &nStructs, sizeof(int));

    return ret;
} /* setNumberOfChips */

/*****************************************************************************
 * setSWversionText
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Returns a text string that describes this version of the SDK library. 
 *
 *  @param NONE
 *
 * @return
 *    ret - always returns N8_STATUS_OK.
 *
 * @par Errors
 *
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t setSWversionText(N8_Buffer_t *value_p, size_t value_l)
{
   snprintf(value_p, value_l, N8_VERSION_STRING);
   return N8_STATUS_OK;
} /* setSWversionText */

/*****************************************************************************
 * setFD
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Get the file descriptor for the NSP2000
 *
 * This function returns the file descriptor for the NSP2000 device.
 *
 * @param value_p WO: Pointer in which to store revision info.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT as described in the return section.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" location.
 *****************************************************************************/
static N8_Status_t setFD(N8_Buffer_t *value_p)
{
    N8_Status_t  ret = N8_STATUS_OK;
    int fd = 0;

    DBG(("setFD\n"));
    do
    {
       CHECK_OBJECT(value_p, ret);
       fd = N8_GetFD();
       memcpy(value_p, &fd, sizeof(unsigned int));
    }while (FALSE);
    DBG(("setFD - OK\n"));
    return ret;
} /* setFD */
/*****************************************************************************
 * setInitInfo
 *****************************************************************************/
/** @ingroup SystemInfo
 * @brief Get the configuration parameters used to initialize the API.
 *
 * This function returns the configuration parameters that were used to 
 * initialize the API.
 *
 * @param value_p WO: Pointer in which to store revision info.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function worked correctly.
 *    N8_INVALID_OBJECT: The output pointer is NULL.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT as described in the return section.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    This function can not be called until the initialization of the API
 *    is complete. The value_p pointer points to a "reasonable" location.
 *****************************************************************************/
static N8_Status_t setInitInfo(N8_Buffer_t *value_p)
{
    N8_Status_t  ret = N8_STATUS_OK;

    DBG(("setInitInfo\n"));
    do
    {
       CHECK_OBJECT(value_p, ret);
       n8_getConfigInfo((N8_ConfigAPI_t *)value_p);
    }while (FALSE);
    DBG(("setInitInfo - OK\n"));
    return ret;
}
