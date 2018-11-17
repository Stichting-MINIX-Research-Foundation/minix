/*	$NetBSD: inquire_cred.c,v 1.2 2017/01/28 21:31:46 christos Exp $	*/

/*
 * Copyright (c) 1997, 2003 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gsskrb5_locl.h"

OM_uint32 GSSAPI_CALLCONV _gsskrb5_inquire_cred
(OM_uint32 * minor_status,
 gss_const_cred_id_t cred_handle,
 gss_name_t * output_name,
 OM_uint32 * lifetime,
 gss_cred_usage_t * cred_usage,
 gss_OID_set * mechanisms
    )
{
    krb5_context context;
    gss_cred_id_t aqcred_init = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t aqcred_accept = GSS_C_NO_CREDENTIAL;
    gsskrb5_cred cred = (gsskrb5_cred)cred_handle;
    gss_OID_set amechs = GSS_C_NO_OID_SET;
    gss_OID_set imechs = GSS_C_NO_OID_SET;
    OM_uint32 junk;
    OM_uint32 aminor;
    OM_uint32 ret;
    OM_uint32 aret;
    OM_uint32 alife = GSS_C_INDEFINITE;
    OM_uint32 ilife = GSS_C_INDEFINITE;

    /*
     * XXX This function is more complex than it has to be.  It should call
     * _gsskrb5_inquire_cred_by_mech() twice and merge the results in the
     * cred_handle == GSS_C_NO_CREDENTIAL case, but since
     * _gsskrb5_inquire_cred_by_mech() is implemented in terms of this
     * function, first we must fix _gsskrb5_inquire_cred_by_mech().
     */

    *minor_status = 0;

    if (output_name)
        *output_name = GSS_C_NO_NAME;
    if (cred_usage)
        *cred_usage = GSS_C_BOTH; /* There's no NONE */
    if (mechanisms)
        *mechanisms = GSS_C_NO_OID_SET;

    GSSAPI_KRB5_INIT (&context);

    if (cred_handle == GSS_C_NO_CREDENTIAL) {
        /*
         * From here to the end of this if we should refactor into a separate
         * function.
         */
        /* Get the info for the default ACCEPT credential */
        aret = _gsskrb5_acquire_cred(&aminor,
                                    GSS_C_NO_NAME,
                                    GSS_C_INDEFINITE,
                                    GSS_C_NO_OID_SET,
                                    GSS_C_ACCEPT,
                                    &aqcred_accept,
                                    NULL,
                                    NULL);
        if (aret == GSS_S_COMPLETE) {
            aret = _gsskrb5_inquire_cred(&aminor,
                                        aqcred_accept,
                                        output_name,
                                        &alife,
                                        NULL,
                                        &amechs);
            (void) _gsskrb5_release_cred(&junk, &aqcred_accept);
            if (aret == GSS_S_COMPLETE) {
                output_name = NULL; /* Can't merge names; output only one */
                if (cred_usage)
                    *cred_usage = GSS_C_ACCEPT;
                if (lifetime)
                    *lifetime = alife;
                if (mechanisms) {
                    *mechanisms = amechs;
                    amechs = GSS_C_NO_OID_SET;
                }
                (void) gss_release_oid_set(&junk, &amechs);
            } else if (aret != GSS_S_NO_CRED) {
                *minor_status = aminor;
                return aret;
            } else {
                alife = GSS_C_INDEFINITE;
            }
        }

        /* Get the info for the default INITIATE credential */
        ret = _gsskrb5_acquire_cred(minor_status,
                                    GSS_C_NO_NAME,
                                    GSS_C_INDEFINITE,
                                    GSS_C_NO_OID_SET,
                                    GSS_C_INITIATE,
                                    &aqcred_init,
                                    NULL,
                                    NULL);
        if (ret == GSS_S_COMPLETE) {
            ret = _gsskrb5_inquire_cred(minor_status,
                                        aqcred_init,
                                        output_name,
                                        &ilife,
                                        NULL,
                                        &imechs);
            (void) _gsskrb5_release_cred(&junk, &aqcred_init);
            if (ret == GSS_S_COMPLETE) {
                /*
                 * Merge results for INITIATE with ACCEPT if we had ACCEPT and
                 * for those outputs that are desired.
                 */
                if (cred_usage) {
                    *cred_usage = (*cred_usage == GSS_C_ACCEPT) ?
                        GSS_C_BOTH : GSS_C_INITIATE;
                }
                if (lifetime)
                    *lifetime = min(alife, ilife);
                if (mechanisms) {
                    /*
                     * This is just one mechanism (IAKERB and such would live
                     * elsewhere).  imechs will be equal to amechs, though not
                     * ==.
                     */
                    if (aret != GSS_S_COMPLETE) {
                        *mechanisms = imechs;
                        imechs = GSS_C_NO_OID_SET;
                    }
                }
                (void) gss_release_oid_set(&junk, &amechs);
            } else if (ret != GSS_S_NO_CRED) {
                *minor_status = aminor;
                return aret;
            }
        }

        if (aret != GSS_S_COMPLETE && ret != GSS_S_COMPLETE) {
            *minor_status = aminor;
            return aret;
        }
        *minor_status = 0; /* Even though 0 is not specified to be special */
        return GSS_S_COMPLETE;
    }

    HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);

    if (output_name != NULL) {
        if (cred->principal != NULL) {
            gss_name_t name = (gss_name_t)cred->principal;
            ret = _gsskrb5_duplicate_name(minor_status, name, output_name);
            if (ret)
                goto out;
        } else if (cred->usage == GSS_C_ACCEPT) {
            /*
             * Keytab case, princ may not be set (yet, ever, whatever).
             *
             * We used to unconditionally output the krb5_sname_to_principal()
             * of the host service for the hostname, but we didn't know if we
             * had keytab entries for it, so it was incorrect.  We can't be
             * breaking anything in tree by outputting GSS_C_NO_NAME, but we
             * might be breaking other callers.
             */
            *output_name = GSS_C_NO_NAME;
        } else {
            /* This shouldn't happen */
            *minor_status = KRB5_NOCREDS_SUPPLIED; /* XXX */
            ret = GSS_S_NO_CRED;
            goto out;
        }
    }
    if (lifetime != NULL) {
        ret = _gsskrb5_lifetime_left(minor_status,
                                     context,
                                     cred->endtime,
                                     lifetime);
        if (ret)
            goto out;
    }
    if (cred_usage != NULL)
        *cred_usage = cred->usage;
    if (mechanisms != NULL) {
        ret = gss_create_empty_oid_set(minor_status, mechanisms);
        if (ret)
            goto out;
        ret = gss_add_oid_set_member(minor_status,
                                     &cred->mechanisms->elements[0],
                                     mechanisms);
        if (ret)
            goto out;
    }
    ret = GSS_S_COMPLETE;

out:
    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
    return ret;
}
