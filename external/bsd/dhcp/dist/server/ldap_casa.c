/*	$NetBSD: ldap_casa.c,v 1.1.1.3 2014/07/12 11:58:13 spz Exp $	*/
/* ldap_casa.c
   
   CASA routines for DHCPD... */

/* Copyright (c) 2006 Novell, Inc.

 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 * 1.Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer. 
 * 2.Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution. 
 * 3.Neither the name of ISC, ISC DHCP, nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software 
 *   without specific prior written permission. 

 * THIS SOFTWARE IS PROVIDED BY INTERNET SYSTEMS CONSORTIUM AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ISC OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.

 * This file was written by S Kalyanasundaram <skalyanasundaram@novell.com>
 */

/*
 * Copyright (c) 2004-2010 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: ldap_casa.c,v 1.1.1.3 2014/07/12 11:58:13 spz Exp $");

#if defined(LDAP_CASA_AUTH)
#include "ldap_casa.h"
#include "dhcpd.h"

int
load_casa (void)
{
       if( !(casaIDK = dlopen(MICASA_LIB,RTLD_LAZY)))
       	  return 0;
       p_miCASAGetCredential = (CASA_GetCredential_T) dlsym(casaIDK, "miCASAGetCredential");
       p_miCASASetCredential = (CASA_SetCredential_T) dlsym(casaIDK, "miCASASetCredential");
       p_miCASARemoveCredential = (CASA_RemoveCredential_T) dlsym(casaIDK, "miCASARemoveCredential");

       if((p_miCASAGetCredential == NULL) ||
         (p_miCASASetCredential == NULL) ||
         (p_miCASARemoveCredential == NULL))
       {
          if(casaIDK)
            dlclose(casaIDK);
          casaIDK = NULL;
          p_miCASAGetCredential = NULL;
          p_miCASASetCredential = NULL;
          p_miCASARemoveCredential = NULL;
          return 0;
       }
       else
          return 1;
}

static void
release_casa(void)
{
   if(casaIDK)
   {
      dlclose(casaIDK);
      casaIDK = NULL;
   }

   p_miCASAGetCredential = NULL;
   p_miCASASetCredential = NULL;
   p_miCASARemoveCredential = NULL;

}

int
load_uname_pwd_from_miCASA (char **ldap_username, char **ldap_password)
 {
   int                     result = 0;
   uint32_t                credentialtype = SSCS_CRED_TYPE_SERVER_F;
   SSCS_BASIC_CREDENTIAL   credential;
   SSCS_SECRET_ID_T        applicationSecretId;
   char                    *tempVar = NULL;

   const char applicationName[10] = "dhcp-ldap";

   if ( load_casa() )
   {
      memset(&credential, 0, sizeof(SSCS_BASIC_CREDENTIAL));
      memset(&applicationSecretId, 0, sizeof(SSCS_SECRET_ID_T));

      applicationSecretId.len = strlen(applicationName) + 1;
      memcpy (applicationSecretId.id, applicationName, applicationSecretId.len);

      credential.unFlags = USERNAME_TYPE_CN_F;

      result = p_miCASAGetCredential (0,
                 &applicationSecretId,NULL,&credentialtype,
                 &credential,NULL);

      if(credential.unLen)
      {
         tempVar = dmalloc (credential.unLen + 1, MDL);
         if (!tempVar)
             log_fatal ("no memory for ldap_username");
         memcpy(tempVar , credential.username, credential.unLen);
         *ldap_username = tempVar;

         tempVar = dmalloc (credential.pwordLen + 1, MDL);
         if (!tempVar)
             log_fatal ("no memory for ldap_password");
         memcpy(tempVar, credential.password, credential.pwordLen);
         *ldap_password = tempVar;

#if defined (DEBUG_LDAP)
         log_info ("Authentication credential taken from CASA");
#endif

         release_casa();
         return 1;

        }
        else
        {
            release_casa();
            return 0;
        }
      }
      else
          return 0; //casa libraries not loaded
 }

#endif /* LDAP_CASA_AUTH */

