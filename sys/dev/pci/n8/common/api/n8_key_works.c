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

static char const n8_id[] = "$Id: n8_key_works.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_key_works.c
 *  @brief Contains key operations
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 12/11/02 brr   Include n8_OS_intf to pick up needed types.
 * 09/20/01 bac   Changed Key_cblock to key_cblock_t to follow coding stds.
 * 07/02/01 mel   Fixed comments.
 * 06/24/01 bac   Changes to bring up to coding standards.
 * 05/29/01 mel   Original version.
 ****************************************************************************/
/** @defgroup keyworks Key Management Routines
 */

#include "n8_key_works.h"       /* definitions for functions  */
#include "n8_OS_intf.h" 


/* buffer with current parameters for RNG settings
 NB!
 We have to design the locking mechanism!
 */



/* this table is taken from des set_key.c 
 * 
 * parity table
 */
 static const unsigned char odd_parity[256]={
  1,  1,  2,  2,  4,  4,  7,  7,  8,  8, 11, 11, 13, 13, 14, 14,
 16, 16, 19, 19, 21, 21, 22, 22, 25, 25, 26, 26, 28, 28, 31, 31,
 32, 32, 35, 35, 37, 37, 38, 38, 41, 41, 42, 42, 44, 44, 47, 47,
 49, 49, 50, 50, 52, 52, 55, 55, 56, 56, 59, 59, 61, 61, 62, 62,
 64, 64, 67, 67, 69, 69, 70, 70, 73, 73, 74, 74, 76, 76, 79, 79,
 81, 81, 82, 82, 84, 84, 87, 87, 88, 88, 91, 91, 93, 93, 94, 94,
 97, 97, 98, 98,100,100,103,103,104,104,107,107,109,109,110,110,
112,112,115,115,117,117,118,118,121,121,122,122,124,124,127,127,
128,128,131,131,133,133,134,134,137,137,138,138,140,140,143,143,
145,145,146,146,148,148,151,151,152,152,155,155,157,157,158,158,
161,161,162,162,164,164,167,167,168,168,171,171,173,173,174,174,
176,176,179,179,181,181,182,182,185,185,186,186,188,188,191,191,
193,193,194,194,196,196,199,199,200,200,203,203,205,205,206,206,
208,208,211,211,213,213,214,214,217,217,218,218,220,220,223,223,
224,224,227,227,229,229,230,230,233,233,234,234,236,236,239,239,
241,241,242,242,244,244,247,247,248,248,251,251,253,253,254,254};

/* Weak and semi weak keys as taken from
 * %A D.W. Davies
 * %A W.L. Price
 * %T Security for Computer Networks
 * %I John Wiley & Sons
 * %D 1984
 * Many thanks to smb@ulysses.att.com (Steven Bellovin) for the reference
 * (and actual cblock values).
 */
static key_cblock_t  weak_keys[NUM_WEAK_KEY]={
        /* weak keys */
        {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01},
        {0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE},
        {0x1F,0x1F,0x1F,0x1F,0x0E,0x0E,0x0E,0x0E},
        {0xE0,0xE0,0xE0,0xE0,0xF1,0xF1,0xF1,0xF1},
        /* semi-weak keys */
        {0x01,0xFE,0x01,0xFE,0x01,0xFE,0x01,0xFE},
        {0xFE,0x01,0xFE,0x01,0xFE,0x01,0xFE,0x01},
        {0x1F,0xE0,0x1F,0xE0,0x0E,0xF1,0x0E,0xF1},
        {0xE0,0x1F,0xE0,0x1F,0xF1,0x0E,0xF1,0x0E},
        {0x01,0xE0,0x01,0xE0,0x01,0xF1,0x01,0xF1},
        {0xE0,0x01,0xE0,0x01,0xF1,0x01,0xF1,0x01},
        {0x1F,0xFE,0x1F,0xFE,0x0E,0xFE,0x0E,0xFE},
        {0xFE,0x1F,0xFE,0x1F,0xFE,0x0E,0xFE,0x0E},
        {0x01,0x1F,0x01,0x1F,0x01,0x0E,0x01,0x0E},
        {0x1F,0x01,0x1F,0x01,0x0E,0x01,0x0E,0x01},
        {0xE0,0xFE,0xE0,0xFE,0xF1,0xFE,0xF1,0xFE},
        {0xFE,0xE0,0xFE,0xE0,0xFE,0xF1,0xFE,0xF1}};

/*****************************************************************************
 * checkKeyForWeakness
 *****************************************************************************/
/** @ingroup keyworks
 * @brief Checks key for weakness.
 *
 * Compare key with values from the Weak Key Table
 *
 * @param key_p RO: A key for X9.17 algorithm
 *
 *
 * @return 
 *           N8_FALSE  -    key is not weak<BR>
 *           N8_TRUE   -    key is weak<BR>
 *
 * @par Errors:
 *    None.
 *****************************************************************************/
N8_Boolean_t checkKeyForWeakness (key_cblock_t *key_p)
{
   int i;
   N8_Boolean_t ret = N8_FALSE;

   DBG(("checkKeyForWeakness\n"));

   for (i=0; i < NUM_WEAK_KEY; i++)
   {
      if (memcmp(weak_keys[i], key_p, sizeof(key_p)) == 0)
      {
         ret = N8_TRUE;
         break;
      }
   }
   return ret;
} /* checkKeyForWeakness */


/*****************************************************************************
 * checkKeyParity
 *****************************************************************************/
/** @ingroup keyworks
 * @brief checks key for parity.
 *
 * Compare key with values from the Parity Table
 *
 * @param key_p RO: A key_p for X9.17 algorithm
 *
 *
 * @return 
 *           FALSE  -    key is not OK <BR>
 *           TRUE   -    key is OK<BR>
 *
 * @par Errors:
 *    None.
 *****************************************************************************/

N8_Boolean_t checkKeyParity(key_cblock_t *key_p)
{
   int i;
   N8_Boolean_t ret = N8_TRUE;
   DBG(("checkKeyParity\n"));

   for (i=0; i < DES_KEY_SIZE; i++)
   {
      if ((*key_p)[i] != odd_parity[(*key_p)[i]])
      {
         ret = N8_FALSE;
         break;
      }
   }
   return ret;
} /* checkKeyParity */


/*****************************************************************************
 * forceParity
 *****************************************************************************/
/** @ingroup keyworks
 * @brief Forces key parity.
 *
 * @param key_p RO: A key for X9.17 algorithm
 *
 *
 * @par Errors:
 *    None.
 *****************************************************************************/
void forceParity(key_cblock_t *key_p)
{
   int i;

   for (i=0; i<DES_KEY_SIZE; i++)
      (*key_p)[i]=odd_parity[(*key_p)[i]];
} /* forceParity */

