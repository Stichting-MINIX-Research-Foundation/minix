/*	$NetBSD: kuserok_plugin.h,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 2011, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef HEIMDAL_KRB5_KUSEROK_PLUGIN_H
#define HEIMDAL_KRB5_KUSEROK_PLUGIN_H 1

#define KRB5_PLUGIN_KUSEROK "krb5_plugin_kuserok"
#define KRB5_PLUGIN_KUSEROK_VERSION_0 0

/** @struct krb5plugin_kuserok_ftable_desc
 *
 * @brief Description of the krb5_kuserok(3) plugin facility.
 *
 * The krb5_kuserok(3) function is pluggable.  The plugin is named
 * KRB5_PLUGIN_KUSEROK ("krb5_plugin_kuserok"), with a single minor
 * version, KRB5_PLUGIN_KUSEROK_VERSION_0 (0).
 *
 * The plugin for krb5_kuserok(3) consists of a data symbol referencing
 * a structure of type krb5plugin_kuserok_ftable, with four fields:
 *
 * @param init          Plugin initialization function (see krb5-plugin(7))
 *
 * @param minor_version The plugin minor version number (0)
 *
 * @param fini          Plugin finalization function
 *
 * @param kuserok       Plugin kuserok function
 *
 * The kuserok field is the plugin entry point that performs the
 * traditional kuserok operation however the plugin desires.  It is
 * invoked in no particular order relative to other kuserok plugins, but
 * it has a 'rule' argument that indicates which plugin is intended to
 * act on the rule.  The plugin kuserok function must return
 * KRB5_PLUGIN_NO_HANDLE if the rule is not applicable to it.
 *
 * The plugin kuserok function has the following arguments, in this
 * order:
 *
 * -# plug_ctx, the context value output by the plugin's init function
 * -# context, a krb5_context
 * -# rule, the kuserok rule being evaluated (from krb5.conf(5))
 * -# flags
 * -# k5login_dir, configured location of k5login per-user files if any
 * -# luser, name of the local user account to which principal is attempting to access.
 * -# principal, the krb5_principal trying to access the luser account
 * -# result, a krb5_boolean pointer where the plugin will output its result
 *
 * @ingroup krb5_support
 */
typedef struct krb5plugin_kuserok_ftable_desc {
    int			minor_version;
    krb5_error_code	(KRB5_LIB_CALL *init)(krb5_context, void **);
    void		(KRB5_LIB_CALL *fini)(void *);
    krb5_error_code	(KRB5_LIB_CALL *kuserok)(void *, krb5_context, const char *,
				   unsigned int, const char *, const char *,
				   krb5_const_principal,
				   krb5_boolean *);
} krb5plugin_kuserok_ftable;

#define KUSEROK_ANAME_TO_LNAME_OK        1
#define KUSEROK_K5LOGIN_IS_AUTHORITATIVE 2

#endif /* HEIMDAL_KRB5_KUSEROK_PLUGIN_H */
