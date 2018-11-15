/*	$NetBSD: db_plugin.h,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

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
 */

/* Id */

#ifndef HEIMDAL_KRB5_DB_PLUGIN_H
#define HEIMDAL_KRB5_DB_PLUGIN_H 1

#define KRB5_PLUGIN_DB "krb5_db_plug"
#define KRB5_PLUGIN_DB_VERSION_0 0

/** @struct krb5plugin_db_ftable_desc
 *
 * @brief Description of the krb5 DB plugin facility.
 *
 * The krb5_aname_to_lname(3) function's DB rule is pluggable.  The
 * plugin is named KRB5_PLUGIN_DB ("krb5_db_plug"), with a single minor
 * version, KRB5_PLUGIN_DB_VERSION_0 (0).
 *
 * The plugin consists of a data symbol referencing a structure of type
 * krb5plugin_db_ftable_desc, with three fields:
 *
 * @param init          Plugin initialization function (see krb5-plugin(7))
 *
 * @param minor_version The plugin minor version number (0)
 *
 * @param fini          Plugin finalization function
 *
 * The init entry point is expected to call heim_db_register().  The
 * fini entry point is expected to do nothing.
 *
 * @ingroup krb5_support
 */
typedef struct krb5plugin_db_ftable_desc {
    int			minor_version;
    krb5_error_code	(KRB5_LIB_CALL *init)(krb5_context, void **);
    void		(KRB5_LIB_CALL *fini)(void *);
} krb5plugin_db_ftable;

#endif /* HEIMDAL_KRB5_DB_PLUGIN_H */

