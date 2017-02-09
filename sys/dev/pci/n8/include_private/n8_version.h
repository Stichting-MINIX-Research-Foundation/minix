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
 * @(#) n8_version.h 1.10@(#)
 *****************************************************************************/
/*****************************************************************************/
/** @file n8_version.h
 *  @brief This file contains version identification information for 
 *  NetOctaves NSP2000 SDK.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 03/14/03 brr   Bump Build number due to change in API_Request_t
 * 11/19/02 brr   Bump revision to 3.0.0.
 * 10/30/02 brr   Bump revision to 2.3.1.
 * 10/30/02 brr   Bump revision to 2.3.0.
 * 05/01/02 brr   Bump revision to 2.2.0.
 * 04/02/02 brr   Original version.
 ****************************************************************************/
#ifndef N8_VERSION_H
#define N8_VERSION_H


/* Change these with every release number update. */

#define N8_MAJOR_REVISION 3
#define N8_MINOR_REVISION 0
#define N8_MAINT_REVISION 0
#define N8_BUILD_NUMBER 26
#define N8_VERSION ((N8_MAJOR_REVISION << 24) | (N8_MINOR_REVISION << 16) | \
                    (N8_MAINT_REVISION <<  8) | (N8_BUILD_NUMBER))

#define N8_VERSION_STRING "NetOctave NSP2000 SDK version %d.%d.%d, build %d\n", \
                           N8_MAJOR_REVISION, N8_MINOR_REVISION,                 \
                           N8_MAINT_REVISION, N8_BUILD_NUMBER


#endif /* N8_VERSION_H */
