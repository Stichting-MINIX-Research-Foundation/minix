/*	$NetBSD: error.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

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

struct heim_error {
    int error_code;
    heim_string_t msg;
    struct heim_error *next;
};

static void
error_dealloc(void *ptr)
{
    struct heim_error *p = ptr;
    heim_release(p->msg);
    heim_release(p->next);
}

static int
error_cmp(void *a, void *b)
{
    struct heim_error *ap = a, *bp = b;
    if (ap->error_code == ap->error_code)
	return ap->error_code - ap->error_code;
    return heim_cmp(ap->msg, bp->msg);
}

static unsigned long
error_hash(void *ptr)
{
    struct heim_error *p = ptr;
    return p->error_code;
}

struct heim_type_data _heim_error_object = {
    HEIM_TID_ERROR,
    "error-object",
    NULL,
    error_dealloc,
    NULL,
    error_cmp,
    error_hash,
    NULL
};

heim_error_t
heim_error_create_enomem(void)
{
    /* This is an immediate object; see heim_number_create() */
    return (heim_error_t)heim_number_create(ENOMEM);
}


void
heim_error_create_opt(heim_error_t *error, int error_code, const char *fmt, ...)
{
    if (error) {
	va_list ap;
	va_start(ap, fmt);
	*error = heim_error_createv(error_code, fmt, ap);
	va_end(ap);
    }
}

heim_error_t
heim_error_create(int error_code, const char *fmt, ...)
{
    heim_error_t e;
    va_list ap;

    va_start(ap, fmt);
    e = heim_error_createv(error_code, fmt, ap);
    va_end(ap);

    return e;
}

heim_error_t
heim_error_createv(int error_code, const char *fmt, va_list ap)
{
    heim_error_t e;
    char *str;
    int len;
    int save_errno = errno;

    str = malloc(1024);
    errno = save_errno;
    if (str == NULL)
        return heim_error_create_enomem();
    len = vsnprintf(str, 1024, fmt, ap);
    errno = save_errno;
    if (len < 0) {
        free(str);
	return NULL; /* XXX We should have a special heim_error_t for this */
    }

    e = _heim_alloc_object(&_heim_error_object, sizeof(struct heim_error));
    if (e) {
	e->msg = heim_string_create(str);
	e->error_code = error_code;
    }
    free(str);

    errno = save_errno;
    return e;
}

heim_string_t
heim_error_copy_string(heim_error_t error)
{
    if (heim_get_tid(error) != HEIM_TID_ERROR) {
	if (heim_get_tid(error) == heim_number_get_type_id())
	    return __heim_string_constant(strerror(heim_number_get_int((heim_number_t)error)));
	heim_abort("invalid heim_error_t");
    }
    /* XXX concat all strings */
    return heim_retain(error->msg);
}

int
heim_error_get_code(heim_error_t error)
{
    if (error == NULL)
	return -1;
    if (heim_get_tid(error) != HEIM_TID_ERROR) {
	if (heim_get_tid(error) == heim_number_get_type_id())
	    return heim_number_get_int((heim_number_t)error);
	heim_abort("invalid heim_error_t");
    }
    return error->error_code;
}

heim_error_t
heim_error_append(heim_error_t top, heim_error_t append)
{
    if (heim_get_tid(top) != HEIM_TID_ERROR) {
	if (heim_get_tid(top) == heim_number_get_type_id())
	    return top;
	heim_abort("invalid heim_error_t");
    }
    if (top->next)
	heim_release(top->next);
    top->next = heim_retain(append);
    return top;
}
