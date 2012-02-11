/*	$NetBSD: prop_string.h,v 1.3 2008/04/28 20:22:51 martin Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PROPLIB_PROP_STRING_H_
#define	_PROPLIB_PROP_STRING_H_

#include <prop/prop_object.h>

typedef struct _prop_string *prop_string_t;

__BEGIN_DECLS
prop_string_t	prop_string_create(void);
prop_string_t	prop_string_create_cstring(const char *);
prop_string_t	prop_string_create_cstring_nocopy(const char *);

prop_string_t	prop_string_copy(prop_string_t);
prop_string_t	prop_string_copy_mutable(prop_string_t);

size_t		prop_string_size(prop_string_t);
bool		prop_string_mutable(prop_string_t);

char *		prop_string_cstring(prop_string_t);
const char *	prop_string_cstring_nocopy(prop_string_t);

bool		prop_string_append(prop_string_t, prop_string_t);
bool		prop_string_append_cstring(prop_string_t, const char *);

bool		prop_string_equals(prop_string_t, prop_string_t);
bool		prop_string_equals_cstring(prop_string_t, const char *);
__END_DECLS

#endif /* _PROPLIB_PROP_STRING_H_ */
