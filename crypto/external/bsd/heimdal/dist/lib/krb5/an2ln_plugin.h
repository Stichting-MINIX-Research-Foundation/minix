/*	$NetBSD: an2ln_plugin.h,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

/* Id */

#ifndef HEIMDAL_KRB5_AN2LN_PLUGIN_H
#define HEIMDAL_KRB5_AN2LN_PLUGIN_H 1

#define KRB5_PLUGIN_AN2LN "an2ln"
#define KRB5_PLUGIN_AN2LN_VERSION_0 0

typedef krb5_error_code (KRB5_LIB_CALL *set_result_f)(void *, const char *);

/** @struct krb5plugin_an2ln_ftable_desc
 *
 * @brief Description of the krb5_aname_to_lname(3) plugin facility.
 *
 * The krb5_aname_to_lname(3) function is pluggable.  The plugin is
 * named KRB5_PLUGIN_AN2LN ("an2ln"), with a single minor version,
 * KRB5_PLUGIN_AN2LN_VERSION_0 (0).
 *
 * The plugin for krb5_aname_to_lname(3) consists of a data symbol
 * referencing a structure of type krb5plugin_an2ln_ftable, with four
 * fields:
 *
 * @param init          Plugin initialization function (see krb5-plugin(7))
 *
 * @param minor_version The plugin minor version number (0)
 *
 * @param fini          Plugin finalization function
 *
 * @param an2ln         Plugin aname_to_lname function
 *
 * The an2ln field is the plugin entry point that performs the
 * traditional aname_to_lname operation however the plugin desires.  It
 * is invoked in no particular order relative to other an2ln plugins,
 * but it has a 'rule' argument that indicates which plugin is intended
 * to act on the rule.  The plugin an2ln function must return
 * KRB5_PLUGIN_NO_HANDLE if the rule is not applicable to it.
 *
 * The plugin an2ln function has the following arguments, in this order:
 *
 * -# plug_ctx, the context value output by the plugin's init function
 * -# context, a krb5_context
 * -# rule, the aname_to_lname rule being evaluated (from krb5.conf(5))
 * -# aname, the krb5_principal to be mapped to an lname
 * -# set_res_f, a function the plugin must call to set its result
 * -# set_res_ctx, the first argument to set_res_f (the second is the result lname string)
 *
 * @ingroup krb5_support
 */
typedef struct krb5plugin_an2ln_ftable_desc {
    int			minor_version;
    krb5_error_code	(KRB5_LIB_CALL *init)(krb5_context, void **);
    void		(KRB5_LIB_CALL *fini)(void *);
    krb5_error_code	(KRB5_LIB_CALL *an2ln)(void *, krb5_context, const char *,
	                         krb5_const_principal, set_result_f, void *);
} krb5plugin_an2ln_ftable;

#endif /* HEIMDAL_KRB5_AN2LN_PLUGIN_H */

