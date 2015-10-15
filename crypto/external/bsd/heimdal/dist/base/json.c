/*	$NetBSD: json.c,v 1.1.1.2 2014/04/24 12:45:26 pettai Exp $	*/

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


int
heim_base2json(heim_object_t obj,
	       void (*out)(char *, void *), void *ctx)
{
    heim_tid_t type = heim_get_tid(obj);
    __block int fail = 0, needcomma = 0;

    switch (type) {
    case HEIM_TID_ARRAY:
	out("[ ", ctx);
	heim_array_iterate(obj, ^(heim_object_t sub) {
		if (needcomma)
		    out(", ", ctx);
		fail |= heim_base2json(sub, out, ctx);
		needcomma = 1;
	    });
	out("]", ctx);
	break;

    case HEIM_TID_DICT:
	out("{ ", ctx);
	heim_dict_iterate(obj, ^(heim_object_t key, heim_object_t value) {
		if (needcomma)
		    out(", ", ctx);
		fail |= heim_base2json(key, out, ctx);
		out(" = ", ctx);
		fail |= heim_base2json(value, out, ctx);
		needcomma = 1;
	    });
	out("}", ctx);
	break;

    case HEIM_TID_STRING:
	out("\"", ctx);
	out(heim_string_get_utf8(obj), ctx);
	out("\"", ctx);
	break;

    case HEIM_TID_NUMBER: {
	char num[16];
	snprintf(num, sizeof(num), "%d", heim_number_get_int(obj));
	out(num, ctx);
	break;
    }
    case HEIM_TID_NULL:
	out("null", ctx);
	break;
    case HEIM_TID_BOOL:
	out(heim_bool_val(obj) ? "true" : "false", ctx);
	break;
    default:
	return 1;
    }
    return fail;
}

static int
parse_dict(heim_dict_t dict, char * const *pp, size_t *len)
{
    const char *p = *pp;
    while (*len) {
	(*len)--;

	if (*p == '\n') {
	    p += 1;
	} else if (isspace(*p)) {
	    p += 1;
	} else if (*p == '}') {
	    *pp = p + 1;
	    return 0;
	} else {
	}
    }
    return ENOENT;
}


heim_object_t
heim_json2base(const void *data, size_t length)
{
    heim_array_t stack;
    heim_object_t o = NULL;
    const char *p = data;
    unsigned long lineno = 1;

    while (length) {
	length--;

	if (*p == '\n') {
	    lineno++;
	} else if (isspace((int)*p)) {
	    ;
	} else if (*p == '{') {
	    o = heim_dict_create();

	    if ((ret = parse_dict(&p, &length)) != 0)
		goto out;
	} else
	    abort();
    }

 out:
    if (ret && o) {
	heim_release(o);
	o = NULL;
    }

    return o;
}
