/*	$NetBSD: fuzzer.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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

#include "der_locl.h"
#include <krb5/com_err.h>

enum trigger_method { FOFF, FRANDOM, FLINEAR, FLINEAR_SIZE };

#ifdef ASN1_FUZZER
static enum trigger_method method = FOFF;

/* FLINEAR */
static unsigned long fnum, fcur, fsize;
#endif

int
asn1_fuzzer_method(const char *mode)
{
#ifdef ASN1_FUZZER
    if (mode == NULL || strcasecmp(mode, "off") == 0) {
	method = FOFF;
    } else if (strcasecmp(mode, "random") == 0) {
	method = FRANDOM;
    } else if (strcasecmp(mode, "linear") == 0) {
	method = FLINEAR;
    } else if (strcasecmp(mode, "linear-size") == 0) {
	method = FLINEAR_SIZE;
    } else
	return 1;
    return 0;
#else
    return 1;
#endif
}

void
asn1_fuzzer_reset(void)
{
#ifdef ASN1_FUZZER
    fcur = 0;
    fsize = 0;
    fnum = 0;
#endif
}

void
asn1_fuzzer_next(void)
{
#ifdef ASN1_FUZZER
    fcur = 0;
    fsize = 0;
    fnum++;
#endif
}

int
asn1_fuzzer_done(void)
{
#ifndef ASN1_FUZZER
    abort();
#else
    /* since code paths */
    return (fnum > 10000);
#endif
}

#ifdef ASN1_FUZZER

static int
fuzzer_trigger(unsigned int chance)
{
    switch(method) {
    case FOFF:
	return 0;
    case FRANDOM:
	if ((rk_random() % chance) != 1)
	    return 0;
	return 1;
    case FLINEAR:
	if (fnum == fcur++)
	    return 1;
	return 0;
    case FLINEAR_SIZE:
	return 0;
    }
    return 0;
}

static int
fuzzer_size_trigger(unsigned long *cur)
{
    if (method != FLINEAR_SIZE)
	return 0;
    if (fnum == (*cur)++)
	return 1;
    return 0;
}

static size_t
fuzzer_length_len (size_t len)
{
    if (fuzzer_size_trigger(&fsize)) {
	len = 0;
    } else if (fuzzer_size_trigger(&fsize)) {
	len = 129;
    } else if (fuzzer_size_trigger(&fsize)) {
	len = 0xffff;
    }

    if (len < 128)
	return 1;
    else {
	int ret = 0;
	do {
	    ++ret;
	    len /= 256;
	} while (len);
	return ret + 1;
    }
}

static int
fuzzer_put_length (unsigned char *p, size_t len, size_t val, size_t *size)
{
    if (len < 1)
	return ASN1_OVERFLOW;

    if (fuzzer_size_trigger(&fcur)) {
	val = 0;
    } else if (fuzzer_size_trigger(&fcur)) {
	val = 129;
    } else if (fuzzer_size_trigger(&fcur)) {
	val = 0xffff;
    }

    if (val < 128) {
	*p = val;
	*size = 1;
    } else {
	size_t l = 0;

	while(val > 0) {
	    if(len < 2)
		return ASN1_OVERFLOW;
	    *p-- = val % 256;
	    val /= 256;
	    len--;
	    l++;
	}
	*p = 0x80 | l;
	if(size)
	    *size = l + 1;
    }
    return 0;
}

static int
fuzzer_put_tag (unsigned char *p, size_t len, Der_class class, Der_type type,
		unsigned int tag, size_t *size)
{
    unsigned fcont = 0;

    if (tag <= 30) {
	if (len < 1)
	    return ASN1_OVERFLOW;
	if (fuzzer_trigger(100))
	    *p = MAKE_TAG(class, type, 0x1f);
	else
	    *p = MAKE_TAG(class, type, tag);
	*size = 1;
    } else {
	size_t ret = 0;
	unsigned int continuation = 0;

	do {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = tag % 128 | continuation;
	    len--;
	    ret++;
	    tag /= 128;
	    continuation = 0x80;
	} while(tag > 0);
	if (len < 1)
	    return ASN1_OVERFLOW;
	if (fuzzer_trigger(100))
	    *p-- = MAKE_TAG(class, type, 0);
	else
	    *p-- = MAKE_TAG(class, type, 0x1f);
	ret++;
	*size = ret;
    }
    return 0;
}

static int
fuzzer_put_length_and_tag (unsigned char *p, size_t len, size_t len_val,
			   Der_class class, Der_type type,
			   unsigned int tag, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = fuzzer_put_length (p, len, len_val, &l);
    if(e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = fuzzer_put_tag (p, len, class, type, tag, &l);
    if(e)
	return e;

    ret += l;
    *size = ret;
    return 0;
}

static int
fuzzer_put_general_string (unsigned char *p, size_t len,
			   const heim_general_string *str, size_t *size)
{
    size_t slen = strlen(*str);

    if (len < slen)
	return ASN1_OVERFLOW;
    p -= slen;
    if (slen >= 2 && fuzzer_trigger(100)) {
	memcpy(p+1, *str, slen);
	memcpy(p+1, "%s", 2);
    } else if (slen >= 2 && fuzzer_trigger(100)) {
	memcpy(p+1, *str, slen);
	memcpy(p+1, "%n", 2);
    } else if (slen >= 4 && fuzzer_trigger(100)) {
	memcpy(p+1, *str, slen);
	memcpy(p+1, "%10n", 4);
    } else if (slen >= 10 && fuzzer_trigger(100)) {
	memcpy(p+1, *str, slen);
	memcpy(p+1, "%n%n%n%n%n", 10);
    } else if (slen >= 10 && fuzzer_trigger(100)) {
	memcpy(p+1, *str, slen);
	memcpy(p+1, "%n%p%s%d%x", 10);
    } else if (slen >= 7 && fuzzer_trigger(100)) {
	memcpy(p+1, *str, slen);
	memcpy(p+1, "%.1024d", 7);
    } else if (slen >= 7 && fuzzer_trigger(100)) {
	memcpy(p+1, *str, slen);
	memcpy(p+1, "%.2049d", 7);
    } else if (fuzzer_trigger(100)) {
	memset(p+1, 0, slen);
    } else if (fuzzer_trigger(100)) {
	memset(p+1, 0xff, slen);
    } else if (fuzzer_trigger(100)) {
	memset(p+1, 'A', slen);
    } else {
	memcpy(p+1, *str, slen);
    }
    *size = slen;
    return 0;
}


struct asn1_type_func fuzzerprim[A1T_NUM_ENTRY] = {
#define fuzel(name, type) {				\
	(asn1_type_encode)fuzzer_put_##name,		\
	(asn1_type_decode)der_get_##name,		\
	(asn1_type_length)der_length_##name,		\
	(asn1_type_copy)der_copy_##name,		\
	(asn1_type_release)der_free_##name,		\
	sizeof(type)					\
    }
#define el(name, type) {				\
	(asn1_type_encode)der_put_##name,		\
	(asn1_type_decode)der_get_##name,		\
	(asn1_type_length)der_length_##name,		\
	(asn1_type_copy)der_copy_##name,		\
	(asn1_type_release)der_free_##name,		\
	sizeof(type)					\
    }
#define elber(name, type) {				\
	(asn1_type_encode)der_put_##name,		\
	(asn1_type_decode)der_get_##name##_ber,		\
	(asn1_type_length)der_length_##name,		\
	(asn1_type_copy)der_copy_##name,		\
	(asn1_type_release)der_free_##name,		\
	sizeof(type)					\
    }
    el(integer, int),
    el(integer64, int64_t),
    el(heim_integer, heim_integer),
    el(integer, int),
    el(unsigned, unsigned),
    el(uninteger64, uint64_t),
    fuzel(general_string, heim_general_string),
    el(octet_string, heim_octet_string),
    elber(octet_string, heim_octet_string),
    el(ia5_string, heim_ia5_string),
    el(bmp_string, heim_bmp_string),
    el(universal_string, heim_universal_string),
    el(printable_string, heim_printable_string),
    el(visible_string, heim_visible_string),
    el(utf8string, heim_utf8_string),
    el(generalized_time, time_t),
    el(utctime, time_t),
    el(bit_string, heim_bit_string),
    { (asn1_type_encode)der_put_boolean, (asn1_type_decode)der_get_boolean,
      (asn1_type_length)der_length_boolean, (asn1_type_copy)der_copy_integer,
      (asn1_type_release)der_free_integer, sizeof(int)
    },
    el(oid, heim_oid),
    el(general_string, heim_general_string),
#undef fuzel
#undef el
#undef elber
};



int
_asn1_encode_fuzzer(const struct asn1_template *t,
		    unsigned char *p, size_t len,
		    const void *data, size_t *size)
{
    size_t elements = A1_HEADER_LEN(t);
    int ret = 0;
    size_t oldlen = len;

    t += A1_HEADER_LEN(t);

    while (elements) {
	switch (t->tt & A1_OP_MASK) {
	case A1_OP_TYPE:
	case A1_OP_TYPE_EXTERN: {
	    size_t newsize;
	    const void *el = DPOC(data, t->offset);

	    if (t->tt & A1_FLAG_OPTIONAL) {
		void **pel = (void **)el;
		if (*pel == NULL)
		    break;
		el = *pel;
	    }

	    if ((t->tt & A1_OP_MASK) == A1_OP_TYPE) {
		ret = _asn1_encode_fuzzer(t->ptr, p, len, el, &newsize);
	    } else {
		const struct asn1_type_func *f = t->ptr;
		ret = (f->encode)(p, len, el, &newsize);
	    }

	    if (ret)
		return ret;
	    p -= newsize; len -= newsize;

	    break;
	}
	case A1_OP_TAG: {
	    const void *olddata = data;
	    size_t l, datalen;

	    data = DPOC(data, t->offset);

	    if (t->tt & A1_FLAG_OPTIONAL) {
		void **el = (void **)data;
		if (*el == NULL) {
		    data = olddata;
		    break;
		}
		data = *el;
	    }

	    ret = _asn1_encode_fuzzer(t->ptr, p, len, data, &datalen);
	    if (ret)
		return ret;

	    len -= datalen; p -= datalen;

	    ret = fuzzer_put_length_and_tag(p, len, datalen,
					    A1_TAG_CLASS(t->tt),
					    A1_TAG_TYPE(t->tt),
					    A1_TAG_TAG(t->tt), &l);
	    if (ret)
		return ret;

	    p -= l; len -= l;

	    data = olddata;

	    break;
	}
	case A1_OP_PARSE: {
	    unsigned int type = A1_PARSE_TYPE(t->tt);
	    size_t newsize;
	    const void *el = DPOC(data, t->offset);

	    if (type > sizeof(fuzzerprim)/sizeof(fuzzerprim[0])) {
		ABORT_ON_ERROR();
		return ASN1_PARSE_ERROR;
	    }

	    ret = (fuzzerprim[type].encode)(p, len, el, &newsize);
	    if (ret)
		return ret;
	    p -= newsize; len -= newsize;

	    break;
	}
	case A1_OP_SETOF: {
	    const struct template_of *el = DPOC(data, t->offset);
	    size_t ellen = _asn1_sizeofType(t->ptr);
	    heim_octet_string *val;
	    unsigned char *elptr = el->val;
	    size_t i, totallen;

	    if (el->len == 0)
		break;

	    if (el->len > UINT_MAX/sizeof(val[0]))
		return ERANGE;

	    val = malloc(sizeof(val[0]) * el->len);
	    if (val == NULL)
		return ENOMEM;

	    for(totallen = 0, i = 0; i < el->len; i++) {
		unsigned char *next;
		size_t l;

		val[i].length = _asn1_length(t->ptr, elptr);
		val[i].data = malloc(val[i].length);

		ret = _asn1_encode_fuzzer(t->ptr, DPO(val[i].data, val[i].length - 1),
					  val[i].length, elptr, &l);
		if (ret)
		    break;

		next = elptr + ellen;
		if (next < elptr) {
		    ret = ASN1_OVERFLOW;
		    break;
		}
		elptr = next;
		totallen += val[i].length;
	    }
	    if (ret == 0 && totallen > len)
		ret = ASN1_OVERFLOW;
	    if (ret) {
		do {
		    free(val[i].data);
		} while(i-- > 0);
		free(val);
		return ret;
	    }

	    len -= totallen;

	    qsort(val, el->len, sizeof(val[0]), _heim_der_set_sort);

	    i = el->len - 1;
	    do {
		p -= val[i].length;
		memcpy(p + 1, val[i].data, val[i].length);
		free(val[i].data);
	    } while(i-- > 0);
	    free(val);

	    break;

	}
	case A1_OP_SEQOF: {
	    struct template_of *el = DPO(data, t->offset);
	    size_t ellen = _asn1_sizeofType(t->ptr);
	    size_t newsize;
	    unsigned int i;
	    unsigned char *elptr = el->val;

	    if (el->len == 0)
		break;

	    elptr += ellen * (el->len - 1);
	   
	    for (i = 0; i < el->len; i++) {
		ret = _asn1_encode_fuzzer(t->ptr, p, len,
				   elptr,
				   &newsize);
		if (ret)
		    return ret;
		p -= newsize; len -= newsize;
		elptr -= ellen;
	    }

	    break;
	}
	case A1_OP_BMEMBER: {
	    const struct asn1_template *bmember = t->ptr;
	    size_t size = bmember->offset;
	    size_t elements = A1_HEADER_LEN(bmember);
	    size_t pos;
	    unsigned char c = 0;
	    unsigned int bitset = 0;
	    int rfc1510 = (bmember->tt & A1_HBF_RFC1510);

	    bmember += elements;

	    if (rfc1510)
		pos = 31;
	    else
		pos = bmember->offset;

	    while (elements && len) {
		while (bmember->offset / 8 < pos / 8) {
		    if (rfc1510 || bitset || c) {
			if (len < 1)
			    return ASN1_OVERFLOW;
			*p-- = c; len--;
		    }
		    c = 0;
		    pos -= 8;
		}
		_asn1_bmember_put_bit(&c, data, bmember->offset, size, &bitset);
		elements--; bmember--;
	    }
	    if (rfc1510 || bitset) {
		if (len < 1)
		    return ASN1_OVERFLOW;
		*p-- = c; len--;
	    }
	   
	    if (len < 1)
		return ASN1_OVERFLOW;
	    if (rfc1510 || bitset == 0)
		*p-- = 0;
	    else
		*p-- = bitset - 1;

	    len--;

	    break;
	}
	case A1_OP_CHOICE: {
	    const struct asn1_template *choice = t->ptr;
	    const unsigned int *element = DPOC(data, choice->offset);
	    size_t datalen;
	    const void *el;

	    if (*element > A1_HEADER_LEN(choice)) {
		printf("element: %d\n", *element);
		return ASN1_PARSE_ERROR;
	    }

	    if (*element == 0) {
		ret += der_put_octet_string(p, len,
					    DPOC(data, choice->tt), &datalen);
	    } else {
		choice += *element;
		el = DPOC(data, choice->offset);
		ret = _asn1_encode_fuzzer(choice->ptr, p, len, el, &datalen);
		if (ret)
		    return ret;
	    }	   
	    len -= datalen; p -= datalen;

	    break;
	}
	default:
	    ABORT_ON_ERROR();
	}
	t--;
	elements--;
    }

    if (fuzzer_trigger(1000)) {
	memset(p + 1, 0, oldlen - len);
    } else if (fuzzer_trigger(1000)) {
	memset(p + 1, 0x41, oldlen - len);
    } else if (fuzzer_trigger(1000)) {
	memset(p + 1, 0xff, oldlen - len);
    }

    if (size)
	*size = oldlen - len;

    return 0;
}

size_t
_asn1_length_fuzzer(const struct asn1_template *t, const void *data)
{
    size_t elements = A1_HEADER_LEN(t);
    size_t ret = 0;

    t += A1_HEADER_LEN(t);

    while (elements) {
	switch (t->tt & A1_OP_MASK) {
	case A1_OP_TYPE:
	case A1_OP_TYPE_EXTERN: {
	    const void *el = DPOC(data, t->offset);

	    if (t->tt & A1_FLAG_OPTIONAL) {
		void **pel = (void **)el;
		if (*pel == NULL)
		    break;
		el = *pel;
	    }

	    if ((t->tt & A1_OP_MASK) == A1_OP_TYPE) {
		ret += _asn1_length(t->ptr, el);
	    } else {
		const struct asn1_type_func *f = t->ptr;
		ret += (f->length)(el);
	    }
	    break;
	}
	case A1_OP_TAG: {
	    size_t datalen;
	    const void *olddata = data;

	    data = DPO(data, t->offset);

	    if (t->tt & A1_FLAG_OPTIONAL) {
		void **el = (void **)data;
		if (*el == NULL) {
		    data = olddata;
		    break;
		}
		data = *el;
	    }
	    datalen = _asn1_length(t->ptr, data);
	    ret += der_length_tag(A1_TAG_TAG(t->tt)) + fuzzer_length_len(datalen);
	    ret += datalen;
	    data = olddata;
	    break;
	}
	case A1_OP_PARSE: {
	    unsigned int type = A1_PARSE_TYPE(t->tt);
	    const void *el = DPOC(data, t->offset);

	    if (type >= sizeof(asn1_template_prim)/sizeof(asn1_template_prim[0])) {
		ABORT_ON_ERROR();
		break;
	    }
	    ret += (asn1_template_prim[type].length)(el);
	    break;
	}
	case A1_OP_SETOF:
	case A1_OP_SEQOF: {
	    const struct template_of *el = DPOC(data, t->offset);
	    size_t ellen = _asn1_sizeofType(t->ptr);
	    const unsigned char *element = el->val;
	    unsigned int i;

	    for (i = 0; i < el->len; i++) {
		ret += _asn1_length(t->ptr, element);
		element += ellen;
	    }

	    break;
	}
	case A1_OP_BMEMBER: {
	    const struct asn1_template *bmember = t->ptr;
	    size_t size = bmember->offset;
	    size_t elements = A1_HEADER_LEN(bmember);
	    int rfc1510 = (bmember->tt & A1_HBF_RFC1510);

	    if (rfc1510) {
		ret += 5;
	    } else {

		ret += 1;

		bmember += elements;

		while (elements) {
		    if (_asn1_bmember_isset_bit(data, bmember->offset, size)) {
			ret += (bmember->offset / 8) + 1;
			break;
		    }
		    elements--; bmember--;
		}
	    }
	    break;
	}
	case A1_OP_CHOICE: {
	    const struct asn1_template *choice = t->ptr;
	    const unsigned int *element = DPOC(data, choice->offset);

	    if (*element > A1_HEADER_LEN(choice))
		break;

	    if (*element == 0) {
		ret += der_length_octet_string(DPOC(data, choice->tt));
	    } else {
		choice += *element;
		ret += _asn1_length(choice->ptr, DPOC(data, choice->offset));
	    }
	    break;
	}
	default:
	    ABORT_ON_ERROR();
	    break;
	}
	elements--;
	t--;
    }
    return ret;
}

#endif /* ASN1_FUZZER */
