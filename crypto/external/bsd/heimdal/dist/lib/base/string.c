/*	$NetBSD: string.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include "baselocl.h"
#include <string.h>

static void
string_dealloc(void *ptr)
{
    heim_string_t s = ptr;
    heim_string_free_f_t *deallocp;
    heim_string_free_f_t dealloc;

    if (*(const char *)ptr != '\0')
	return;

    /* Possible string ref */
    deallocp = _heim_get_isaextra(s, 0);
    dealloc = *deallocp;
    if (dealloc != NULL) {
	char **strp = _heim_get_isaextra(s, 1);
	dealloc(*strp);
    }
}

static int
string_cmp(void *a, void *b)
{
    if (*(char *)a == '\0') {
	char **strp = _heim_get_isaextra(a, 1);

	if (*strp != NULL)
	    a = *strp; /* a is a string ref */
    }
    if (*(char *)b == '\0') {
	char **strp = _heim_get_isaextra(b, 1);

	if (*strp != NULL)
	    b = *strp; /* b is a string ref */
    }
    return strcmp(a, b);
}

static unsigned long
string_hash(void *ptr)
{
    const char *s = ptr;
    unsigned long n;

    for (n = 0; *s; ++s)
	n += *s;
    return n;
}

struct heim_type_data _heim_string_object = {
    HEIM_TID_STRING,
    "string-object",
    NULL,
    string_dealloc,
    NULL,
    string_cmp,
    string_hash,
    NULL
};

/**
 * Create a string object
 *
 * @param string the string to create, must be an utf8 string
 *
 * @return string object
 */

heim_string_t
heim_string_create(const char *string)
{
    return heim_string_create_with_bytes(string, strlen(string));
}

/**
 * Create a string object without copying the source.
 *
 * @param string the string to referenced, must be UTF-8
 * @param dealloc the function to use to release the referece to the string
 *
 * @return string object
 */

heim_string_t
heim_string_ref_create(const char *string, heim_string_free_f_t dealloc)
{
    heim_string_t s;
    heim_string_free_f_t *deallocp;

    s = _heim_alloc_object(&_heim_string_object, 1);
    if (s) {
	const char **strp;

	((char *)s)[0] = '\0';
	deallocp = _heim_get_isaextra(s, 0);
	*deallocp = dealloc;
	strp = _heim_get_isaextra(s, 1);
	*strp = string;
    }
    return s;
}

/**
 * Create a string object
 *
 * @param string the string to create, must be an utf8 string
 * @param len the length of the string
 *
 * @return string object
 */

heim_string_t
heim_string_create_with_bytes(const void *data, size_t len)
{
    heim_string_t s;

    s = _heim_alloc_object(&_heim_string_object, len + 1);
    if (s) {
	memcpy(s, data, len);
	((char *)s)[len] = '\0';
    }
    return s;
}

/**
 * Create a string object using a format string
 *
 * @param fmt format string
 * @param ...
 *
 * @return string object
 */

heim_string_t
heim_string_create_with_format(const char *fmt, ...)
{
    heim_string_t s;
    char *str = NULL;
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vasprintf(&str, fmt, ap);
    va_end(ap);
    if (ret < 0 || str == NULL)
	return NULL;

    s = heim_string_ref_create(str, string_dealloc);
    if (s == NULL)
	free(str);
    return s;
}

/**
 * Return the type ID of string objects
 *
 * @return type id of string objects
 */

heim_tid_t
heim_string_get_type_id(void)
{
    return HEIM_TID_STRING;
}

/**
 * Get the string value of the content.
 *
 * @param string the string object to get the value from
 *
 * @return a utf8 string
 */

const char *
heim_string_get_utf8(heim_string_t string)
{
    if (*(const char *)string == '\0') {
	const char **strp;

	/* String ref */
	strp = _heim_get_isaextra(string, 1);
	if (*strp != NULL)
	    return *strp;
    }
    return (const char *)string;
}

/*
 *
 */

static void
init_string(void *ptr)
{
    heim_dict_t *dict = ptr;
    *dict = heim_dict_create(101);
    heim_assert(*dict != NULL, "__heim_string_constant");
}

heim_string_t
__heim_string_constant(const char *_str)
{
    static HEIMDAL_MUTEX mutex = HEIMDAL_MUTEX_INITIALIZER;
    static heim_base_once_t once;
    static heim_dict_t dict = NULL;
    heim_string_t s, s2;

    heim_base_once_f(&once, &dict, init_string);
    s = heim_string_create(_str);

    HEIMDAL_MUTEX_lock(&mutex);
    s2 = heim_dict_get_value(dict, s);
    if (s2) {
	heim_release(s);
	s = s2;
    } else {
	_heim_make_permanent(s);
	heim_dict_set_value(dict, s, s);
    }
    HEIMDAL_MUTEX_unlock(&mutex);

    return s;
}
