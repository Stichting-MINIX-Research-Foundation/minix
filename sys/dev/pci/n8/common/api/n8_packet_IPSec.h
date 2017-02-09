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
 * @(#) n8_packet_IPSec.h 1.14@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_packet_IPSec.h
 *  @brief Header for IPSec Packet functions.
 *
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 06/06/03 brr   Move useful defines to public include.
 * 04/19/02 bac   Corrected IPSEC_DATA_LENGTH_MIN and
 *                IPSEC_DECRYPTED_DATA_LENGTH_MIN values.  Created a new define
 *                IPSEC_DATA_BLOCK_SIZE.  Fixes BUG #716.
 * 10/12/01 dkm   Moved public portion to n8_pub_packet.h.
 * 08/27/01 mel   Deleted IPSEC_SET.. definisions - they are not in use.
 * 06/25/01 bac   Added IPSEC_AUTHENTICATION_DATA_LENGTH
 * 05/23/01 bac   Original version.
 ****************************************************************************/
#ifndef N8_PACKET_IPSEC_H
#define N8_PACKET_IPSEC_H

#include "n8_packet.h"
#include "n8_pub_packet.h"
#include "n8_pub_errors.h"

#define     IPSEC_DATA_BLOCK_SIZE 8
#define     IPSEC_DECRYPTED_DATA_LENGTH_MIN 36
#define     IPSEC_AUTHENTICATION_DATA_LENGTH 12

#define IPSEC_SPI_OFFSET                0
#define IPSEC_SEQUENCE_NIMBER_OFFSET    4

#define IPSEC_EXTRACT_SPI(PACKET_P)    ((uint32_t) BE_to_uint32(&PACKET_P[IPSEC_SPI_OFFSET]))
#define IPSEC_EXTRACT_SEQUENCE_NIMBER(PACKET_P)  ((uint32_t) BE_to_uint32(&PACKET_P[IPSEC_SEQUENCE_NIMBER_OFFSET]))

#endif

