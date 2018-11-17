/*	$NetBSD: json.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

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
#include <ctype.h>
#include <krb5/base64.h>

static heim_base_once_t heim_json_once = HEIM_BASE_ONCE_INIT;
static heim_string_t heim_tid_data_uuid_key = NULL;
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
json_init_once(void *arg)
{
    heim_tid_data_uuid_key = __heim_string_constant("heimdal-type-data-76d7fca2-d0da-4b20-a126-1a10f8a0eae6");
}

struct twojson {
    void *ctx;
    void (*out)(void *, const char *);
    size_t indent;
    heim_json_flags_t flags;
    int ret;
    int first;
};

struct heim_strbuf {
    char *str;
    size_t len;
    size_t alloced;
    int	enomem;
    heim_json_flags_t flags;
};

static int
base2json(heim_object_t, struct twojson *);

static void
indent(struct twojson *j)
{
    size_t i = j->indent;
    if (j->flags & HEIM_JSON_F_ONE_LINE)
	return;
    while (i--)
	j->out(j->ctx, "\t");
}

static void
array2json(heim_object_t value, void *ctx, int *stop)
{
    struct twojson *j = ctx;
    if (j->ret)
	return;
    if (j->first) {
	j->first = 0;
    } else {
	j->out(j->ctx, NULL); /* eat previous '\n' if possible */
	j->out(j->ctx, ",\n");
    }
    j->ret = base2json(value, j);
}

static void
dict2json(heim_object_t key, heim_object_t value, void *ctx)
{
    struct twojson *j = ctx;
    if (j->ret)
	return;
    if (j->first) {
	j->first = 0;
    } else {
	j->out(j->ctx, NULL); /* eat previous '\n' if possible */
	j->out(j->ctx, ",\n");
    }
    j->ret = base2json(key, j);
    if (j->ret)
	return;
    j->out(j->ctx, " : \n");
    j->indent++;
    j->ret = base2json(value, j);
    if (j->ret)
	return;
    j->indent--;
}

static int
base2json(heim_object_t obj, struct twojson *j)
{
    heim_tid_t type;
    int first = 0;

    if (obj == NULL) {
	if (j->flags & HEIM_JSON_F_CNULL2JSNULL) {
	    obj = heim_null_create();
	} else if (j->flags & HEIM_JSON_F_NO_C_NULL) {
	    return EINVAL;
	} else {
	    indent(j);
	    j->out(j->ctx, "<NULL>\n"); /* This is NOT valid JSON! */
	    return 0;
	}
    }

    type = heim_get_tid(obj);
    switch (type) {
    case HEIM_TID_ARRAY:
	indent(j);
	j->out(j->ctx, "[\n");
	j->indent++;
	first = j->first;
	j->first = 1;
	heim_array_iterate_f(obj, j, array2json);
	j->indent--;
	if (!j->first)
	    j->out(j->ctx, "\n");
	indent(j);
	j->out(j->ctx, "]\n");
	j->first = first;
	break;

    case HEIM_TID_DICT:
	indent(j);
	j->out(j->ctx, "{\n");
	j->indent++;
	first = j->first;
	j->first = 1;
	heim_dict_iterate_f(obj, j, dict2json);
	j->indent--;
	if (!j->first)
	    j->out(j->ctx, "\n");
	indent(j);
	j->out(j->ctx, "}\n");
	j->first = first;
	break;

    case HEIM_TID_STRING:
	indent(j);
	j->out(j->ctx, "\"");
	j->out(j->ctx, heim_string_get_utf8(obj));
	j->out(j->ctx, "\"");
	break;

    case HEIM_TID_DATA: {
	heim_dict_t d;
	heim_string_t v;
	const heim_octet_string *data;
	char *b64 = NULL;
	int ret;

	if (j->flags & HEIM_JSON_F_NO_DATA)
	    return EINVAL; /* JSON doesn't do binary */

	data = heim_data_get_data(obj);
	ret = rk_base64_encode(data->data, data->length, &b64);
	if (ret < 0 || b64 == NULL)
	    return ENOMEM;

	if (j->flags & HEIM_JSON_F_NO_DATA_DICT) {
	    indent(j);
	    j->out(j->ctx, "\"");
	    j->out(j->ctx, b64); /* base64-encode; hope there's no aliasing */
	    j->out(j->ctx, "\"");
	    free(b64);
	} else {
	    /*
	     * JSON has no way to represent binary data, therefore the
	     * following is a Heimdal-specific convention.
	     *
	     * We encode binary data as a dict with a single very magic
	     * key with a base64-encoded value.  The magic key includes
	     * a uuid, so we're not likely to alias accidentally.
	     */
	    d = heim_dict_create(2);
	    if (d == NULL) {
		free(b64);
		return ENOMEM;
	    }
	    v = heim_string_ref_create(b64, free);
	    if (v == NULL) {
		free(b64);
		heim_release(d);
		return ENOMEM;
	    }
	    ret = heim_dict_set_value(d, heim_tid_data_uuid_key, v);
	    heim_release(v);
	    if (ret) {
		heim_release(d);
		return ENOMEM;
	    }
	    ret = base2json(d, j);
	    heim_release(d);
	    if (ret)
		return ret;
	}
	break;
    }

    case HEIM_TID_NUMBER: {
	char num[32];
	indent(j);
	snprintf(num, sizeof (num), "%d", heim_number_get_int(obj));
	j->out(j->ctx, num);
	break;
    }
    case HEIM_TID_NULL:
	indent(j);
	j->out(j->ctx, "null");
	break;
    case HEIM_TID_BOOL:
	indent(j);
	j->out(j->ctx, heim_bool_val(obj) ? "true" : "false");
	break;
    default:
	return 1;
    }
    return 0;
}

static int
heim_base2json(heim_object_t obj, void *ctx, heim_json_flags_t flags,
	       void (*out)(void *, const char *))
{
    struct twojson j;

    if (flags & HEIM_JSON_F_STRICT_STRINGS)
	return ENOTSUP; /* Sorry, not yet! */

    heim_base_once_f(&heim_json_once, NULL, json_init_once);

    j.indent = 0;
    j.ctx = ctx;
    j.out = out;
    j.flags = flags;
    j.ret = 0;
    j.first = 1;

    return base2json(obj, &j);
}


/*
 *
 */

struct parse_ctx {
    unsigned long lineno;
    const uint8_t *p;
    const uint8_t *pstart;
    const uint8_t *pend;
    heim_error_t error;
    size_t depth;
    heim_json_flags_t flags;
};


static heim_object_t
parse_value(struct parse_ctx *ctx);

/*
 * This function eats whitespace, but, critically, it also succeeds
 * only if there's anything left to parse.
 */
static int
white_spaces(struct parse_ctx *ctx)
{
    while (ctx->p < ctx->pend) {
	uint8_t c = *ctx->p;
	if (c == ' ' || c == '\t' || c == '\r') {

	} else if (c == '\n') {
	    ctx->lineno++;
	} else
	    return 0;
	(ctx->p)++;
    }
    return -1;
}

static int
is_number(uint8_t n)
{
    return ('0' <= n && n <= '9');
}

static heim_number_t
parse_number(struct parse_ctx *ctx)
{
    int number = 0, neg = 1;

    if (ctx->p >= ctx->pend)
	return NULL;

    if (*ctx->p == '-') {
	if (ctx->p + 1 >= ctx->pend)
	    return NULL;
	neg = -1;
	ctx->p += 1;
    }

    while (ctx->p < ctx->pend) {
	if (is_number(*ctx->p)) {
	    number = (number * 10) + (*ctx->p - '0');
	} else {
	    break;
	}
	ctx->p += 1;
    }

    return heim_number_create(number * neg);
}

static heim_string_t
parse_string(struct parse_ctx *ctx)
{
    const uint8_t *start;
    int quote = 0;

    if (ctx->flags & HEIM_JSON_F_STRICT_STRINGS) {
	ctx->error = heim_error_create(EINVAL, "Strict JSON string encoding "
				       "not yet supported");
	return NULL;
    }

    if (*ctx->p != '"') {
	ctx->error = heim_error_create(EINVAL, "Expected a JSON string but "
				       "found something else at line %lu",
				       ctx->lineno);
	return NULL;
    }
    start = ++ctx->p;

    while (ctx->p < ctx->pend) {
	if (*ctx->p == '\n') {
	    ctx->lineno++;
	} else if (*ctx->p == '\\') {
	    if (ctx->p + 1 == ctx->pend)
		goto out;
	    ctx->p++;
	    quote = 1;
	} else if (*ctx->p == '"') {
	    heim_object_t o;

	    if (quote) {
		char *p0, *p;
		p = p0 = malloc(ctx->p - start);
		if (p == NULL)
		    goto out;
		while (start < ctx->p) {
		    if (*start == '\\') {
			start++;
			/* XXX validate quoted char */
		    }
		    *p++ = *start++;
		}
		o = heim_string_create_with_bytes(p0, p - p0);
		free(p0);
	    } else {
		o = heim_string_create_with_bytes(start, ctx->p - start);
		if (o == NULL) {
		    ctx->error = heim_error_create_enomem();
		    return NULL;
		}

		/* If we can decode as base64, then let's */
		if (ctx->flags & HEIM_JSON_F_TRY_DECODE_DATA) {
		    void *buf;
		    size_t len;
		    const char *s;

		    s = heim_string_get_utf8(o);
		    len = strlen(s);

		    if (len >= 4 && strspn(s, base64_chars) >= len - 2) {
			buf = malloc(len);
			if (buf == NULL) {
			    heim_release(o);
			    ctx->error = heim_error_create_enomem();
			    return NULL;
			}
			len = rk_base64_decode(s, buf);
			if (len == -1) {
			    free(buf);
			    return o;
			}
			heim_release(o);
			o = heim_data_ref_create(buf, len, free);
		    }
		}
	    }
	    ctx->p += 1;

	    return o;
	}
	ctx->p += 1;
    }
    out:
    ctx->error = heim_error_create(EINVAL, "ran out of string");
    return NULL;
}

static int
parse_pair(heim_dict_t dict, struct parse_ctx *ctx)
{
    heim_string_t key;
    heim_object_t value;

    if (white_spaces(ctx))
	return -1;

    if (*ctx->p == '}') {
	ctx->p++;
	return 0;
    }

    if (ctx->flags & HEIM_JSON_F_STRICT_DICT)
	/* JSON allows only string keys */
	key = parse_string(ctx);
    else
	/* heim_dict_t allows any heim_object_t as key */
	key = parse_value(ctx);
    if (key == NULL)
	/* Even heim_dict_t does not allow C NULLs as keys though! */
	return -1;

    if (white_spaces(ctx)) {
	heim_release(key);
	return -1;
    }

    if (*ctx->p != ':') {
	heim_release(key);
	return -1;
    }

    ctx->p += 1; /* safe because we call white_spaces() next */

    if (white_spaces(ctx)) {
	heim_release(key);
	return -1;
    }

    value = parse_value(ctx);
    if (value == NULL &&
	(ctx->error != NULL || (ctx->flags & HEIM_JSON_F_NO_C_NULL))) {
	if (ctx->error == NULL)
	    ctx->error = heim_error_create(EINVAL, "Invalid JSON encoding");
	heim_release(key);
	return -1;
    }
    heim_dict_set_value(dict, key, value);
    heim_release(key);
    heim_release(value);

    if (white_spaces(ctx))
	return -1;

    if (*ctx->p == '}') {
	/*
	 * Return 1 but don't consume the '}' so we can count the one
	 * pair in a one-pair dict
	 */
	return 1;
    } else if (*ctx->p == ',') {
	ctx->p++;
	return 1;
    }
    return -1;
}

static heim_dict_t
parse_dict(struct parse_ctx *ctx)
{
    heim_dict_t dict;
    size_t count = 0;
    int ret;

    heim_assert(*ctx->p == '{', "string doesn't start with {");

    dict = heim_dict_create(11);
    if (dict == NULL) {
	ctx->error = heim_error_create_enomem();
	return NULL;
    }

    ctx->p += 1; /* safe because parse_pair() calls white_spaces() first */

    while ((ret = parse_pair(dict, ctx)) > 0)
	count++;
    if (ret < 0) {
	heim_release(dict);
	return NULL;
    }
    if (count == 1 && !(ctx->flags & HEIM_JSON_F_NO_DATA_DICT)) {
	heim_object_t v = heim_dict_copy_value(dict, heim_tid_data_uuid_key);

	/*
	 * Binary data encoded as a dict with a single magic key with
	 * base64-encoded value?  Decode as heim_data_t.
	 */
	if (v != NULL && heim_get_tid(v) == HEIM_TID_STRING) {
	    void *buf;
	    size_t len;

	    buf = malloc(strlen(heim_string_get_utf8(v)));
	    if (buf == NULL) {
		heim_release(dict);
		heim_release(v);
		ctx->error = heim_error_create_enomem();
		return NULL;
	    }
	    len = rk_base64_decode(heim_string_get_utf8(v), buf);
	    heim_release(v);
	    if (len == -1) {
		free(buf);
		return dict; /* assume aliasing accident */
	    }
	    heim_release(dict);
	    return (heim_dict_t)heim_data_ref_create(buf, len, free);
	}
    }
    return dict;
}

static int
parse_item(heim_array_t array, struct parse_ctx *ctx)
{
    heim_object_t value;

    if (white_spaces(ctx))
	return -1;

    if (*ctx->p == ']') {
	ctx->p++; /* safe because parse_value() calls white_spaces() first */
	return 0;
    }

    value = parse_value(ctx);
    if (value == NULL &&
	(ctx->error || (ctx->flags & HEIM_JSON_F_NO_C_NULL)))
	return -1;

    heim_array_append_value(array, value);
    heim_release(value);

    if (white_spaces(ctx))
	return -1;

    if (*ctx->p == ']') {
	ctx->p++;
	return 0;
    } else if (*ctx->p == ',') {
	ctx->p++;
	return 1;
    }
    return -1;
}

static heim_array_t
parse_array(struct parse_ctx *ctx)
{
    heim_array_t array = heim_array_create();
    int ret;

    heim_assert(*ctx->p == '[', "array doesn't start with [");
    ctx->p += 1;

    while ((ret = parse_item(array, ctx)) > 0)
	;
    if (ret < 0) {
	heim_release(array);
	return NULL;
    }
    return array;
}

static heim_object_t
parse_value(struct parse_ctx *ctx)
{
    size_t len;
    heim_object_t o;

    if (white_spaces(ctx))
	return NULL;

    if (*ctx->p == '"') {
	return parse_string(ctx);
    } else if (*ctx->p == '{') {
	if (ctx->depth-- == 1) {
	    ctx->error = heim_error_create(EINVAL, "JSON object too deep");
	    return NULL;
	}
	o = parse_dict(ctx);
	ctx->depth++;
	return o;
    } else if (*ctx->p == '[') {
	if (ctx->depth-- == 1) {
	    ctx->error = heim_error_create(EINVAL, "JSON object too deep");
	    return NULL;
	}
	o = parse_array(ctx);
	ctx->depth++;
	return o;
    } else if (is_number(*ctx->p) || *ctx->p == '-') {
	return parse_number(ctx);
    }

    len = ctx->pend - ctx->p;

    if ((ctx->flags & HEIM_JSON_F_NO_C_NULL) == 0 &&
	len >= 6 && memcmp(ctx->p, "<NULL>", 6) == 0) {
	ctx->p += 6;
	return heim_null_create();
    } else if (len >= 4 && memcmp(ctx->p, "null", 4) == 0) {
	ctx->p += 4;
	return heim_null_create();
    } else if (len >= 4 && strncasecmp((char *)ctx->p, "true", 4) == 0) {
	ctx->p += 4;
	return heim_bool_create(1);
    } else if (len >= 5 && strncasecmp((char *)ctx->p, "false", 5) == 0) {
	ctx->p += 5;
	return heim_bool_create(0);
    }

    ctx->error = heim_error_create(EINVAL, "unknown char %c at %lu line %lu",
				   (char)*ctx->p, 
				   (unsigned long)(ctx->p - ctx->pstart),
				   ctx->lineno);
    return NULL;
}


heim_object_t
heim_json_create(const char *string, size_t max_depth, heim_json_flags_t flags,
		 heim_error_t *error)
{
    return heim_json_create_with_bytes(string, strlen(string), max_depth, flags,
				       error);
}

heim_object_t
heim_json_create_with_bytes(const void *data, size_t length, size_t max_depth,
			    heim_json_flags_t flags, heim_error_t *error)
{
    struct parse_ctx ctx;
    heim_object_t o;

    heim_base_once_f(&heim_json_once, NULL, json_init_once);

    ctx.lineno = 1;
    ctx.p = data;
    ctx.pstart = data;
    ctx.pend = ((uint8_t *)data) + length;
    ctx.error = NULL;
    ctx.flags = flags;
    ctx.depth = max_depth;

    o = parse_value(&ctx);

    if (o == NULL && error) {
	*error = ctx.error;
    } else if (ctx.error) {
	heim_release(ctx.error);
    }

    return o;
}


static void
show_printf(void *ctx, const char *str)
{
    if (str == NULL)
	return;
    fprintf(ctx, "%s", str);
}

/**
 * Dump a heimbase object to stderr (useful from the debugger!)
 *
 * @param obj object to dump using JSON or JSON-like format
 *
 * @addtogroup heimbase
 */
void
heim_show(heim_object_t obj)
{
    heim_base2json(obj, stderr, HEIM_JSON_F_NO_DATA_DICT, show_printf);
}

static void
strbuf_add(void *ctx, const char *str)
{
    struct heim_strbuf *strbuf = ctx;
    size_t len;

    if (strbuf->enomem)
	return;

    if (str == NULL) {
	/*
	 * Eat the last '\n'; this is used when formatting dict pairs
	 * and array items so that the ',' separating them is never
	 * preceded by a '\n'.
	 */
	if (strbuf->len > 0 && strbuf->str[strbuf->len - 1] == '\n')
	    strbuf->len--;
	return;
    }

    len = strlen(str);
    if ((len + 1) > (strbuf->alloced - strbuf->len)) {
	size_t new_len = strbuf->alloced + (strbuf->alloced >> 2) + len + 1;
	char *s;

	s = realloc(strbuf->str, new_len);
	if (s == NULL) {
	    strbuf->enomem = 1;
	    return;
	}
	strbuf->str = s;
	strbuf->alloced = new_len;
    }
    /* +1 so we copy the NUL */
    (void) memcpy(strbuf->str + strbuf->len, str, len + 1);
    strbuf->len += len;
    if (strbuf->str[strbuf->len - 1] == '\n' && 
	strbuf->flags & HEIM_JSON_F_ONE_LINE)
	strbuf->len--;
}

#define STRBUF_INIT_SZ 64

heim_string_t
heim_json_copy_serialize(heim_object_t obj, heim_json_flags_t flags, heim_error_t *error)
{
    heim_string_t str;
    struct heim_strbuf strbuf;
    int ret;

    if (error)
	*error = NULL;

    memset(&strbuf, 0, sizeof (strbuf));
    strbuf.str = malloc(STRBUF_INIT_SZ);
    if (strbuf.str == NULL) {
	if (error)
	    *error = heim_error_create_enomem();
	return NULL;
    }
    strbuf.len = 0;
    strbuf.alloced = STRBUF_INIT_SZ;
    strbuf.str[0] = '\0';
    strbuf.flags = flags;

    ret = heim_base2json(obj, &strbuf, flags, strbuf_add);
    if (ret || strbuf.enomem) {
	if (error) {
	    if (strbuf.enomem || ret == ENOMEM)
		*error = heim_error_create_enomem();
	    else
		*error = heim_error_create(1, "Impossible to JSON-encode "
					   "object");
	}
	free(strbuf.str);
	return NULL;
    }
    if (flags & HEIM_JSON_F_ONE_LINE) {
	strbuf.flags &= ~HEIM_JSON_F_ONE_LINE;
	strbuf_add(&strbuf, "\n");
    }
    str = heim_string_ref_create(strbuf.str, free);
    if (str == NULL) {
	if (error)
	    *error = heim_error_create_enomem();
	free(strbuf.str);
    }
    return str;
}
