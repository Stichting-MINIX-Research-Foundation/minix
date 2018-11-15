/*	$NetBSD: gss_pname_to_uid.c,v 1.2.4.1 2017/09/11 04:58:44 snj Exp $	*/

/*
 * Copyright (c) 2011, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "mech_locl.h"

static OM_uint32
mech_localname(OM_uint32 *minor_status,
               struct _gss_mechanism_name *mn,
               gss_buffer_t localname)
{
    OM_uint32 major_status = GSS_S_UNAVAILABLE;

    *minor_status = 0;

    if (mn->gmn_mech->gm_localname == NULL)
        return GSS_S_UNAVAILABLE;

    major_status = mn->gmn_mech->gm_localname(minor_status,
                                              mn->gmn_name,
                                              mn->gmn_mech_oid,
                                              localname);
    if (GSS_ERROR(major_status))
        _gss_mg_error(mn->gmn_mech, major_status, *minor_status);

    return major_status;
}

static OM_uint32
attr_localname(OM_uint32 *minor_status,
               struct _gss_mechanism_name *mn,
               gss_buffer_t localname)
{
    OM_uint32 major_status = GSS_S_UNAVAILABLE;
    OM_uint32 tmpMinor;
    gss_buffer_desc value = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc display_value = GSS_C_EMPTY_BUFFER;
    int authenticated = 0, complete = 0;
    int more = -1;

    *minor_status = 0;

    localname->length = 0;
    localname->value = NULL;

    if (mn->gmn_mech->gm_get_name_attribute == NULL)
        return GSS_S_UNAVAILABLE;

    major_status = mn->gmn_mech->gm_get_name_attribute(minor_status,
                                                       mn->gmn_name,
                                                       GSS_C_ATTR_LOCAL_LOGIN_USER,
                                                       &authenticated,
                                                       &complete,
                                                       &value,
                                                       &display_value,
                                                       &more);
    if (GSS_ERROR(major_status)) {
        _gss_mg_error(mn->gmn_mech, major_status, *minor_status);
        return major_status;
    }

    if (authenticated) {
        *localname = value;
    } else {
        major_status = GSS_S_UNAVAILABLE;
        gss_release_buffer(&tmpMinor, &value);
    }

    gss_release_buffer(&tmpMinor, &display_value);

    return major_status;
}

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_localname(OM_uint32 *minor_status,
              gss_const_name_t pname,
              const gss_OID mech_type,
              gss_buffer_t localname)
{
    OM_uint32 major_status = GSS_S_UNAVAILABLE;
    struct _gss_name *name = (struct _gss_name *) pname;
    struct _gss_mechanism_name *mn = NULL;

    *minor_status = 0;

    if (mech_type != GSS_C_NO_OID) {
        major_status = _gss_find_mn(minor_status, name, mech_type, &mn);
        if (GSS_ERROR(major_status))
            return major_status;

        major_status = mech_localname(minor_status, mn, localname);
        if (major_status != GSS_S_COMPLETE)
            major_status = attr_localname(minor_status, mn, localname);
    } else {
        HEIM_SLIST_FOREACH(mn, &name->gn_mn, gmn_link) {
            major_status = mech_localname(minor_status, mn, localname);
            if (major_status != GSS_S_COMPLETE)
                major_status = attr_localname(minor_status, mn, localname);
            if (major_status != GSS_S_UNAVAILABLE)
                break;
        }
    }

    if (major_status != GSS_S_COMPLETE && mn != NULL)
        _gss_mg_error(mn->gmn_mech, major_status, *minor_status);

    return major_status;
}


GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_pname_to_uid(OM_uint32 *minor_status,
                 gss_const_name_t pname,
                 const gss_OID mech_type,
                 uid_t *uidp)
{
#ifdef NO_LOCALNAME
    return GSS_S_UNAVAILABLE;
#else
    OM_uint32 major, tmpMinor;
    gss_buffer_desc localname = GSS_C_EMPTY_BUFFER;
    char *szLocalname;
    char pwbuf[2048];
    struct passwd pw, *pwd;

    major = gss_localname(minor_status, pname, mech_type, &localname);
    if (GSS_ERROR(major))
        return major;

    szLocalname = malloc(localname.length + 1);
    if (szLocalname == NULL) {
        gss_release_buffer(&tmpMinor, &localname);
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    memcpy(szLocalname, localname.value, localname.length);
    szLocalname[localname.length] = '\0';

    if (rk_getpwnam_r(szLocalname, &pw, pwbuf, sizeof(pwbuf), &pwd) != 0)
        pwd = NULL;

    gss_release_buffer(&tmpMinor, &localname);
    free(szLocalname);

    *minor_status = 0;

    if (pwd != NULL) {
        *uidp = pwd->pw_uid;
        major = GSS_S_COMPLETE;
    } else {
        major = GSS_S_UNAVAILABLE;
    }

    return major;
#endif
}
