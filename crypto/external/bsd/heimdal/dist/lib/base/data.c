/*	$NetBSD: data.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
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

#include "baselocl.h"
#include <string.h>

static void
data_dealloc(void *ptr)
{
    heim_data_t d = ptr;
    heim_octet_string *os = (heim_octet_string *)d;
    heim_data_free_f_t *deallocp;
    heim_data_free_f_t dealloc;

    if (os->data == NULL)
	return;

    /* Possible string ref */
    deallocp = _heim_get_isaextra(os, 0);
    dealloc = *deallocp;
    if (dealloc != NULL)
	dealloc(os->data);
}

static int
data_cmp(void *a, void *b)
{
    heim_octet_string *osa = a, *osb = b;
    if (osa->length != osb->length)
	return osa->length - osb->length;
    return memcmp(osa->data, osb->data, osa->length);
}

static unsigned long
data_hash(void *ptr)
{
    heim_octet_string *os = ptr;
    const unsigned char *s = os->data;

    if (os->length < 4)
	return os->length;
    return s[0] | (s[1] << 8) |
	(s[os->length - 2] << 16) | (s[os->length - 1] << 24);
}

struct heim_type_data _heim_data_object = {
    HEIM_TID_DATA,
    "data-object",
    NULL,
    data_dealloc,
    NULL,
    data_cmp,
    data_hash,
    NULL
};

/**
 * Create a data object
 *
 * @param string the string to create, must be an utf8 string
 *
 * @return string object
 */

heim_data_t
heim_data_create(const void *data, size_t length)
{
    heim_octet_string *os;

    os = _heim_alloc_object(&_heim_data_object, sizeof(*os) + length);
    if (os) {
	os->data = (uint8_t *)os + sizeof(*os);
	os->length = length;
	memcpy(os->data, data, length);
    }
    return (heim_data_t)os;
}

heim_data_t
heim_data_ref_create(const void *data, size_t length,
		     heim_data_free_f_t dealloc)
{
    heim_octet_string *os;
    heim_data_free_f_t *deallocp;

    os = _heim_alloc_object(&_heim_data_object, sizeof(*os) + length);
    if (os) {
	os->data = (void *)data;
	os->length = length;
	deallocp = _heim_get_isaextra(os, 0);
	*deallocp = dealloc;
    }
    return (heim_data_t)os;
}


/**
 * Return the type ID of data objects
 *
 * @return type id of data objects
 */

heim_tid_t
heim_data_get_type_id(void)
{
    return HEIM_TID_DATA;
}

/**
 * Get the data value of the content.
 *
 * @param data the data object to get the value from
 *
 * @return a heim_octet_string
 */

const heim_octet_string *
heim_data_get_data(heim_data_t data)
{
    /* Note that this works for data and data_ref objects */
    return (const heim_octet_string *)data;
}

const void *
heim_data_get_ptr(heim_data_t data)
{
    /* Note that this works for data and data_ref objects */
    return ((const heim_octet_string *)data)->data;
}

size_t	heim_data_get_length(heim_data_t data)
{
    /* Note that this works for data and data_ref objects */
    return ((const heim_octet_string *)data)->length;
}
