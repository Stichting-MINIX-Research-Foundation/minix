/*	$NetBSD: ntlm.c,v 1.1.1.3 2017/01/28 20:46:52 christos Exp $	*/

/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include <krb5/roken.h>

#include <krb5/wind.h>
#include <krb5/parse_units.h>
#include <krb5/krb5.h>


#define HC_DEPRECATED_CRYPTO

#include <krb5/krb5-types.h>
#include "crypto-headers.h"

#include <krb5/heimntlm.h>

/*! \mainpage Heimdal NTLM library
 *
 * \section intro Introduction
 *
 * Heimdal libheimntlm library is a implementation of the NTLM
 * protocol, both version 1 and 2. The GSS-API mech that uses this
 * library adds support for transport encryption and integrity
 * checking.
 *
 * NTLM is a protocol for mutual authentication, its still used in
 * many protocol where Kerberos is not support, one example is
 * EAP/X802.1x mechanism LEAP from Microsoft and Cisco.
 *
 * This is a support library for the core protocol, its used in
 * Heimdal to implement and GSS-API mechanism. There is also support
 * in the KDC to do remote digest authenticiation, this to allow
 * services to authenticate users w/o direct access to the users ntlm
 * hashes (same as Kerberos arcfour enctype keys).
 *
 * More information about the NTLM protocol can found here
 * http://davenport.sourceforge.net/ntlm.html .
 *
 * The Heimdal projects web page: http://www.h5l.org/
 *
 * @section ntlm_example NTLM Example
 *
 * Example to to use @ref test_ntlm.c .
 *
 * @example test_ntlm.c
 *
 * Example how to use the NTLM primitives.
 *
 */

/** @defgroup ntlm_core Heimdal NTLM library
 *
 * The NTLM core functions implement the string2key generation
 * function, message encode and decode function, and the hash function
 * functions.
 */

struct sec_buffer {
    uint16_t length;
    uint16_t allocated;
    uint32_t offset;
};

static const unsigned char ntlmsigature[8] = "NTLMSSP\x00";

time_t heim_ntlm_time_skew = 300;

/*
 *
 */

#define CHECK(f, e)							\
    do {								\
	ret = f;							\
	if (ret != (ssize_t)(e)) {					\
	    ret = HNTLM_ERR_DECODE;					\
	    goto out;							\
	}								\
    } while(/*CONSTCOND*/0)

#define CHECK_SIZE(f, e)						\
    do {								\
	ssize_t sret = f;						\
	if (sret != (ssize_t)(e)) {					\
	    ret = HNTLM_ERR_DECODE;					\
	    goto out;							\
	}								\
    } while(/*CONSTCOND*/0)

#define CHECK_OFFSET(f, e)						\
    do {								\
	off_t sret = f;							\
	if (sret != (e)) {						\
	    ret = HNTLM_ERR_DECODE;					\
	    goto out;							\
	}								\
    } while(/*CONSTCOND*/0)


static struct units ntlm_flag_units[] = {
#define ntlm_flag(x) { #x, NTLM_##x }
    ntlm_flag(ENC_56),
    ntlm_flag(NEG_KEYEX),
    ntlm_flag(ENC_128),
    ntlm_flag(MBZ1),
    ntlm_flag(MBZ2),
    ntlm_flag(MBZ3),
    ntlm_flag(NEG_VERSION),
    ntlm_flag(MBZ4),
    ntlm_flag(NEG_TARGET_INFO),
    ntlm_flag(NON_NT_SESSION_KEY),
    ntlm_flag(MBZ5),
    ntlm_flag(NEG_IDENTIFY),
    ntlm_flag(NEG_NTLM2),
    ntlm_flag(TARGET_SHARE),
    ntlm_flag(TARGET_SERVER),
    ntlm_flag(TARGET_DOMAIN),
    ntlm_flag(NEG_ALWAYS_SIGN),
    ntlm_flag(MBZ6),
    ntlm_flag(OEM_SUPPLIED_WORKSTATION),
    ntlm_flag(OEM_SUPPLIED_DOMAIN),
    ntlm_flag(NEG_ANONYMOUS),
    ntlm_flag(NEG_NT_ONLY),
    ntlm_flag(NEG_NTLM),
    ntlm_flag(MBZ8),
    ntlm_flag(NEG_LM_KEY),
    ntlm_flag(NEG_DATAGRAM),
    ntlm_flag(NEG_SEAL),
    ntlm_flag(NEG_SIGN),
    ntlm_flag(MBZ9),
    ntlm_flag(NEG_TARGET),
    ntlm_flag(NEG_OEM),
    ntlm_flag(NEG_UNICODE),
#undef ntlm_flag
    {NULL, 0}
};

size_t
heim_ntlm_unparse_flags(uint32_t flags, char *s, size_t len)
{
    return unparse_flags(flags, ntlm_flag_units, s, len);
}


/**
 * heim_ntlm_free_buf frees the ntlm buffer
 *
 * @param p buffer to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_buf(struct ntlm_buf *p)
{
    if (p->data)
	free(p->data);
    p->data = NULL;
    p->length = 0;
}


static int
ascii2ucs2le(const char *string, int up, struct ntlm_buf *buf)
{
    uint16_t *data;
    size_t len, n;
    uint8_t *p;
    int ret;

    ret = wind_utf8ucs2_length(string, &len);
    if (ret)
	return ret;
    if (len > UINT_MAX / sizeof(data[0]))
	return ERANGE;

    data = malloc(len * sizeof(data[0]));
    if (data == NULL)
	return ENOMEM;

    ret = wind_utf8ucs2(string, data, &len);
    if (ret) {
	free(data);
	return ret;
    }
    
    if (len == 0) {
	free(data);
	buf->data = NULL;
	buf->length = 0;
	return 0;
    }

    /* uppercase string, only handle ascii right now */
    if (up) {
	for (n = 0; n < len ; n++) {
	    if (data[n] < 128)
		data[n] = toupper((int)data[n]);
	}
    }

    buf->length = len * 2;
    p = buf->data = malloc(buf->length);
    if (buf->data == NULL && len != 0) {
	free(data);
	heim_ntlm_free_buf(buf);
	return ENOMEM;
    }

    for (n = 0; n < len ; n++) {
	p[(n * 2) + 0] = (data[n]     ) & 0xff;
	p[(n * 2) + 1] = (data[n] >> 8) & 0xff;
    }
    memset(data, 0, sizeof(data[0]) * len);
    free(data);

    return 0;
}

/*
 * Sizes in bytes
 */

#define SIZE_SEC_BUFFER		(2+2+4)
#define SIZE_OS_VERSION		(8)

/*
 *
 */

static krb5_error_code
ret_sec_buffer(krb5_storage *sp, struct sec_buffer *buf)
{
    krb5_error_code ret;
    CHECK(krb5_ret_uint16(sp, &buf->length), 0);
    CHECK(krb5_ret_uint16(sp, &buf->allocated), 0);
    CHECK(krb5_ret_uint32(sp, &buf->offset), 0);
out:
    return ret;
}

static krb5_error_code
store_sec_buffer(krb5_storage *sp, const struct sec_buffer *buf)
{
    krb5_error_code ret;
    CHECK(krb5_store_uint16(sp, buf->length), 0);
    CHECK(krb5_store_uint16(sp, buf->allocated), 0);
    CHECK(krb5_store_uint32(sp, buf->offset), 0);
out:
    return ret;
}

/*
 * Strings are either OEM or UNICODE. The later is encoded as ucs2 on
 * wire, but using utf8 in memory.
 */

static size_t
len_string(int ucs2, const char *s)
{
    if (ucs2) {
	size_t len;
	int ret;

	ret = wind_utf8ucs2_length(s, &len);
	if (ret == 0)
	    return len * 2;
	return strlen(s) * 5 * 2;
    } else {
	return strlen(s);
    }
}

/*
 *
 */

static krb5_error_code
ret_string(krb5_storage *sp, int ucs2, size_t len, char **s)
{
    krb5_error_code ret;
    uint16_t *data = NULL;

    *s = malloc(len + 1);
    if (*s == NULL)
	return ENOMEM;
    CHECK_SIZE(krb5_storage_read(sp, *s, len), len);

    (*s)[len] = '\0';

    if (ucs2) {
	unsigned int flags = WIND_RW_LE;
	size_t utf16len = len / 2;
	size_t utf8len;

	data = malloc(utf16len * sizeof(data[0])); 
	if (data == NULL) {
	    free(*s); *s = NULL;
	    ret = ENOMEM;
	    goto out;
	}

	ret = wind_ucs2read(*s, len, &flags, data, &utf16len);
	free(*s); *s = NULL;
	if (ret) {
	    goto out;
	}

	CHECK(wind_ucs2utf8_length(data, utf16len, &utf8len), 0);

	utf8len += 1;
	
	*s = malloc(utf8len);
	if (s == NULL) {
	    ret = ENOMEM;
	    goto out;
	}

	CHECK(wind_ucs2utf8(data, utf16len, *s, &utf8len), 0);
    }
    ret = 0;
 out:
    if (data)
	free(data);

    return ret;
}



static krb5_error_code
ret_sec_string(krb5_storage *sp, int ucs2, struct sec_buffer *desc, char **s)
{
    krb5_error_code ret = 0;
    CHECK_OFFSET(krb5_storage_seek(sp, desc->offset, SEEK_SET), desc->offset);
    CHECK(ret_string(sp, ucs2, desc->length, s), 0);
 out:
    return ret; 
}

static krb5_error_code
put_string(krb5_storage *sp, int ucs2, const char *s)
{
    krb5_error_code ret;
    struct ntlm_buf buf;

    if (ucs2) {
	ret = ascii2ucs2le(s, 0, &buf);
	if (ret)
	    return ret;
    } else {
	buf.data = rk_UNCONST(s);
	buf.length = strlen(s);
    }

    CHECK_SIZE(krb5_storage_write(sp, buf.data, buf.length), buf.length);
    if (ucs2)
	heim_ntlm_free_buf(&buf);
    ret = 0;
out:
    return ret;
}

/*
 *
 */

static krb5_error_code
ret_buf(krb5_storage *sp, struct sec_buffer *desc, struct ntlm_buf *buf)
{
    krb5_error_code ret;

    buf->data = malloc(desc->length);
    buf->length = desc->length;
    CHECK_OFFSET(krb5_storage_seek(sp, desc->offset, SEEK_SET), desc->offset);
    CHECK_SIZE(krb5_storage_read(sp, buf->data, buf->length), buf->length);
    ret = 0;
out:
    return ret;
}

static krb5_error_code
put_buf(krb5_storage *sp, const struct ntlm_buf *buf)
{
    krb5_error_code ret;
    CHECK_SIZE(krb5_storage_write(sp, buf->data, buf->length), buf->length);
    ret = 0;
out:
    return ret;
}

/**
 * Frees the ntlm_targetinfo message
 *
 * @param ti targetinfo to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_targetinfo(struct ntlm_targetinfo *ti)
{
    free(ti->servername);
    free(ti->domainname);
    free(ti->dnsdomainname);
    free(ti->dnsservername);
    free(ti->dnstreename);
    free(ti->targetname);
    heim_ntlm_free_buf(&ti->channel_bindings);
    memset(ti, 0, sizeof(*ti));
}

static int
encode_ti_string(krb5_storage *out, uint16_t type, int ucs2, char *s)
{
    krb5_error_code ret;
    CHECK(krb5_store_uint16(out, type), 0);
    CHECK(krb5_store_uint16(out, len_string(ucs2, s)), 0);
    CHECK(put_string(out, ucs2, s), 0);
out:
    return ret;
}

/**
 * Encodes a ntlm_targetinfo message.
 *
 * @param ti the ntlm_targetinfo message to encode.
 * @param ucs2 ignored
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_targetinfo(const struct ntlm_targetinfo *ti,
			    int ucs2,
			    struct ntlm_buf *data)
{
    krb5_error_code ret;
    krb5_storage *out;

    data->data = NULL;
    data->length = 0;

    out = krb5_storage_emem();
    if (out == NULL)
	return ENOMEM;

    krb5_storage_set_byteorder(out, KRB5_STORAGE_BYTEORDER_LE);

    if (ti->servername)
	CHECK(encode_ti_string(out, 1, ucs2, ti->servername), 0);
    if (ti->domainname)
	CHECK(encode_ti_string(out, 2, ucs2, ti->domainname), 0);
    if (ti->dnsservername)
	CHECK(encode_ti_string(out, 3, ucs2, ti->dnsservername), 0);
    if (ti->dnsdomainname)
	CHECK(encode_ti_string(out, 4, ucs2, ti->dnsdomainname), 0);
    if (ti->dnstreename)
	CHECK(encode_ti_string(out, 5, ucs2, ti->dnstreename), 0);
    if (ti->avflags) {
	CHECK(krb5_store_uint16(out, 6), 0);
	CHECK(krb5_store_uint16(out, 4), 0);
	CHECK(krb5_store_uint32(out, ti->avflags), 0);
    }
    if (ti->timestamp) {
	CHECK(krb5_store_uint16(out, 7), 0);
	CHECK(krb5_store_uint16(out, 8), 0);
	CHECK(krb5_store_uint32(out, ti->timestamp & 0xffffffff), 0);
	CHECK(krb5_store_uint32(out, (ti->timestamp >> 32) & 0xffffffff), 0);
    }	
    if (ti->targetname) {
	CHECK(encode_ti_string(out, 9, ucs2, ti->targetname), 0);
    }
    if (ti->channel_bindings.length) {
	CHECK(krb5_store_uint16(out, 10), 0);
	CHECK(krb5_store_uint16(out, ti->channel_bindings.length), 0);
	CHECK_SIZE(krb5_storage_write(out, ti->channel_bindings.data, ti->channel_bindings.length), ti->channel_bindings.length);
    }

    /* end tag */
    CHECK(krb5_store_int16(out, 0), 0);
    CHECK(krb5_store_int16(out, 0), 0);

    {
	krb5_data d;
	ret = krb5_storage_to_data(out, &d);
	data->data = d.data;
	data->length = d.length;
    }
out:
    krb5_storage_free(out);
    return ret;
}

/**
 * Decodes an NTLM targetinfo message
 *
 * @param data input data buffer with the encode NTLM targetinfo message
 * @param ucs2 if the strings should be encoded with ucs2 (selected by flag in message).
 * @param ti the decoded target info, should be freed with heim_ntlm_free_targetinfo().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_decode_targetinfo(const struct ntlm_buf *data,
			    int ucs2,
			    struct ntlm_targetinfo *ti)
{
    uint16_t type, len;
    krb5_storage *in;
    int ret = 0, done = 0;

    memset(ti, 0, sizeof(*ti));

    if (data->length == 0)
	return 0;

    in = krb5_storage_from_readonly_mem(data->data, data->length);
    if (in == NULL)
	return ENOMEM;
    krb5_storage_set_byteorder(in, KRB5_STORAGE_BYTEORDER_LE);

    while (!done) {
	CHECK(krb5_ret_uint16(in, &type), 0);
	CHECK(krb5_ret_uint16(in, &len), 0);

	switch (type) {
	case 0:
	    done = 1;
	    break;
	case 1:
	    CHECK(ret_string(in, ucs2, len, &ti->servername), 0);
	    break;
	case 2:
	    CHECK(ret_string(in, ucs2, len, &ti->domainname), 0);
	    break;
	case 3:
	    CHECK(ret_string(in, ucs2, len, &ti->dnsservername), 0);
	    break;
	case 4:
	    CHECK(ret_string(in, ucs2, len, &ti->dnsdomainname), 0);
	    break;
	case 5:
	    CHECK(ret_string(in, ucs2, len, &ti->dnstreename), 0);
	    break;
	case 6:
	    CHECK(krb5_ret_uint32(in, &ti->avflags), 0);
	    break;
	case 7: {
	    uint32_t tmp;
	    CHECK(krb5_ret_uint32(in, &tmp), 0);
	    ti->timestamp = tmp;
	    CHECK(krb5_ret_uint32(in, &tmp), 0);
	    ti->timestamp |= ((uint64_t)tmp) << 32;
	    break;
	}	
	case 9:
	    CHECK(ret_string(in, 1, len, &ti->targetname), 0);
	    break;
	case 10:
	    ti->channel_bindings.data = malloc(len);
	    if (ti->channel_bindings.data == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    ti->channel_bindings.length = len;
	    CHECK_SIZE(krb5_storage_read(in, ti->channel_bindings.data, len), len);
	    break;
	default:
	    krb5_storage_seek(in, len, SEEK_CUR);
	    break;
	}
    }
 out:
    if (in)
	krb5_storage_free(in);
    return ret;
}

static krb5_error_code
encode_os_version(krb5_storage *out)
{
    krb5_error_code ret;
    CHECK(krb5_store_uint8(out, 0x06), 0);
    CHECK(krb5_store_uint8(out, 0x01), 0);
    CHECK(krb5_store_uint16(out, 0x1db0), 0);
    CHECK(krb5_store_uint8(out, 0x0f), 0); /* ntlm version 15 */
    CHECK(krb5_store_uint8(out, 0x00), 0);
    CHECK(krb5_store_uint8(out, 0x00), 0);
    CHECK(krb5_store_uint8(out, 0x00), 0);
 out:
    return ret;
}

/**
 * Frees the ntlm_type1 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type1(struct ntlm_type1 *data)
{
    if (data->domain)
	free(data->domain);
    if (data->hostname)
	free(data->hostname);
    memset(data, 0, sizeof(*data));
}

int
heim_ntlm_decode_type1(const struct ntlm_buf *buf, struct ntlm_type1 *data)
{
    krb5_error_code ret;
    unsigned char sig[8];
    uint32_t type;
    struct sec_buffer domain, hostname;
    krb5_storage *in;
    int ucs2;

    memset(data, 0, sizeof(*data));

    in = krb5_storage_from_readonly_mem(buf->data, buf->length);
    if (in == NULL) {
	ret = ENOMEM;
	goto out;
    }
    krb5_storage_set_byteorder(in, KRB5_STORAGE_BYTEORDER_LE);

    CHECK_SIZE(krb5_storage_read(in, sig, sizeof(sig)), sizeof(sig));
    CHECK(memcmp(ntlmsigature, sig, sizeof(ntlmsigature)), 0);
    CHECK(krb5_ret_uint32(in, &type), 0);
    CHECK(type, 1);
    CHECK(krb5_ret_uint32(in, &data->flags), 0);

    ucs2 = !!(data->flags & NTLM_NEG_UNICODE);

    /*
     * domain and hostname are unconditionally encoded regardless of
     * NTLMSSP_NEGOTIATE_OEM_{HOSTNAME,WORKSTATION}_SUPPLIED flag
     */
    CHECK(ret_sec_buffer(in, &domain), 0);
    CHECK(ret_sec_buffer(in, &hostname), 0);

    if (data->flags & NTLM_NEG_VERSION) {
	CHECK(krb5_ret_uint32(in, &data->os[0]), 0);
	CHECK(krb5_ret_uint32(in, &data->os[1]), 0);
    }

    if (data->flags & NTLM_OEM_SUPPLIED_DOMAIN)
	CHECK(ret_sec_string(in, ucs2, &domain, &data->domain), 0);
    if (data->flags & NTLM_OEM_SUPPLIED_WORKSTATION)
	CHECK(ret_sec_string(in, ucs2, &hostname, &data->hostname), 0);

out:
    if (in)
	krb5_storage_free(in);
    if (ret)
	heim_ntlm_free_type1(data);

    return ret;
}

/**
 * Encodes an ntlm_type1 message.
 *
 * @param type1 the ntlm_type1 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type1(const struct ntlm_type1 *type1, struct ntlm_buf *data)
{
    krb5_error_code ret;
    struct sec_buffer domain, hostname;
    krb5_storage *out;
    uint32_t base, flags;
    int ucs2 = 0;

    flags = type1->flags;
    base = 16;

    if (flags & NTLM_NEG_UNICODE)
	ucs2 = 1;

    if (type1->domain) {
	base += SIZE_SEC_BUFFER;
	flags |= NTLM_OEM_SUPPLIED_DOMAIN;
    }
    if (type1->hostname) {
	base += SIZE_SEC_BUFFER;
	flags |= NTLM_OEM_SUPPLIED_WORKSTATION;
    }
    if (flags & NTLM_NEG_VERSION)
	base += SIZE_OS_VERSION; /* os */

    if (type1->domain) {
	domain.offset = base;
	domain.length = len_string(ucs2, type1->domain);
	domain.allocated = domain.length;
    } else {
	domain.offset = 0;
	domain.length = 0;
	domain.allocated = 0;
    }

    if (type1->hostname) {
	hostname.offset = domain.allocated + domain.offset;
	hostname.length = len_string(ucs2, type1->hostname);
	hostname.allocated = hostname.length;
    } else {
	hostname.offset = 0;
	hostname.length = 0;
	hostname.allocated = 0;
    }

    out = krb5_storage_emem();
    if (out == NULL)
	return ENOMEM;

    krb5_storage_set_byteorder(out, KRB5_STORAGE_BYTEORDER_LE);
    CHECK_SIZE(krb5_storage_write(out, ntlmsigature, sizeof(ntlmsigature)),
	  sizeof(ntlmsigature));
    CHECK(krb5_store_uint32(out, 1), 0);
    CHECK(krb5_store_uint32(out, flags), 0);

    CHECK(store_sec_buffer(out, &domain), 0);
    CHECK(store_sec_buffer(out, &hostname), 0);

    if (flags & NTLM_NEG_VERSION) {
	CHECK(encode_os_version(out), 0);
    }
    if (type1->domain)
	CHECK(put_string(out, ucs2, type1->domain), 0);
    if (type1->hostname)
	CHECK(put_string(out, ucs2, type1->hostname), 0);

    {
	krb5_data d;
	ret = krb5_storage_to_data(out, &d);
	data->data = d.data;
	data->length = d.length;
    }
out:
    krb5_storage_free(out);

    return ret;
}

/**
 * Frees the ntlm_type2 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type2(struct ntlm_type2 *data)
{
    if (data->targetname)
	free(data->targetname);
    heim_ntlm_free_buf(&data->targetinfo);
    memset(data, 0, sizeof(*data));
}

int
heim_ntlm_decode_type2(const struct ntlm_buf *buf, struct ntlm_type2 *type2)
{
    krb5_error_code ret;
    unsigned char sig[8];
    uint32_t type, ctx[2];
    struct sec_buffer targetname, targetinfo;
    krb5_storage *in;
    int ucs2 = 0;

    memset(type2, 0, sizeof(*type2));

    in = krb5_storage_from_readonly_mem(buf->data, buf->length);
    if (in == NULL) {
	ret = ENOMEM;
	goto out;
    }
    krb5_storage_set_byteorder(in, KRB5_STORAGE_BYTEORDER_LE);

    CHECK_SIZE(krb5_storage_read(in, sig, sizeof(sig)), sizeof(sig));
    CHECK(memcmp(ntlmsigature, sig, sizeof(ntlmsigature)), 0);
    CHECK(krb5_ret_uint32(in, &type), 0);
    CHECK(type, 2);

    CHECK(ret_sec_buffer(in, &targetname), 0);
    CHECK(krb5_ret_uint32(in, &type2->flags), 0);
    if (type2->flags & NTLM_NEG_UNICODE)
	ucs2 = 1;
    CHECK_SIZE(krb5_storage_read(in, type2->challenge, sizeof(type2->challenge)),
	  sizeof(type2->challenge));
    CHECK(krb5_ret_uint32(in, &ctx[0]), 0); /* context */
    CHECK(krb5_ret_uint32(in, &ctx[1]), 0);
    CHECK(ret_sec_buffer(in, &targetinfo), 0);
    /* os version */
    if (type2->flags & NTLM_NEG_VERSION) {
	CHECK(krb5_ret_uint32(in, &type2->os[0]), 0);
	CHECK(krb5_ret_uint32(in, &type2->os[1]), 0);
    }

    CHECK(ret_sec_string(in, ucs2, &targetname, &type2->targetname), 0);
    CHECK(ret_buf(in, &targetinfo, &type2->targetinfo), 0);
    ret = 0;

out:
    if (in)
	krb5_storage_free(in);
    if (ret)
	heim_ntlm_free_type2(type2);

    return ret;
}

/**
 * Encodes an ntlm_type2 message.
 *
 * @param type2 the ntlm_type2 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type2(const struct ntlm_type2 *type2, struct ntlm_buf *data)
{
    struct sec_buffer targetname, targetinfo;
    krb5_error_code ret;
    krb5_storage *out = NULL;
    uint32_t base;
    int ucs2 = 0;

    base = 48;

    if (type2->flags & NTLM_NEG_VERSION)
	base += SIZE_OS_VERSION;

    if (type2->flags & NTLM_NEG_UNICODE)
	ucs2 = 1;

    targetname.offset = base;
    targetname.length = len_string(ucs2, type2->targetname);
    targetname.allocated = targetname.length;

    targetinfo.offset = targetname.allocated + targetname.offset;
    targetinfo.length = type2->targetinfo.length;
    targetinfo.allocated = type2->targetinfo.length;

    out = krb5_storage_emem();
    if (out == NULL)
	return ENOMEM;

    krb5_storage_set_byteorder(out, KRB5_STORAGE_BYTEORDER_LE);
    CHECK_SIZE(krb5_storage_write(out, ntlmsigature, sizeof(ntlmsigature)),
	  sizeof(ntlmsigature));
    CHECK(krb5_store_uint32(out, 2), 0);
    CHECK(store_sec_buffer(out, &targetname), 0);
    CHECK(krb5_store_uint32(out, type2->flags), 0);
    CHECK_SIZE(krb5_storage_write(out, type2->challenge, sizeof(type2->challenge)),
	  sizeof(type2->challenge));
    CHECK(krb5_store_uint32(out, 0), 0); /* context */
    CHECK(krb5_store_uint32(out, 0), 0);
    CHECK(store_sec_buffer(out, &targetinfo), 0);
    /* os version */
    if (type2->flags & NTLM_NEG_VERSION) {
	CHECK(encode_os_version(out), 0);
    }
    CHECK(put_string(out, ucs2, type2->targetname), 0);
    CHECK_SIZE(krb5_storage_write(out, type2->targetinfo.data,
			     type2->targetinfo.length),
	  type2->targetinfo.length);

    {
	krb5_data d;
	ret = krb5_storage_to_data(out, &d);
	data->data = d.data;
	data->length = d.length;
    }

out:
    krb5_storage_free(out);

    return ret;
}

/**
 * Frees the ntlm_type3 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type3(struct ntlm_type3 *data)
{
    heim_ntlm_free_buf(&data->lm);
    heim_ntlm_free_buf(&data->ntlm);
    if (data->targetname)
	free(data->targetname);
    if (data->username)
	free(data->username);
    if (data->ws)
	free(data->ws);
    heim_ntlm_free_buf(&data->sessionkey);
    memset(data, 0, sizeof(*data));
}

/*
 *
 */

int
heim_ntlm_decode_type3(const struct ntlm_buf *buf,
		       int ucs2,
		       struct ntlm_type3 *type3)
{
    krb5_error_code ret;
    unsigned char sig[8];
    uint32_t type;
    krb5_storage *in;
    struct sec_buffer lm, ntlm, target, username, sessionkey, ws;
    uint32_t min_offset = 0xffffffff;

    memset(type3, 0, sizeof(*type3));
    memset(&sessionkey, 0, sizeof(sessionkey));

    in = krb5_storage_from_readonly_mem(buf->data, buf->length);
    if (in == NULL) {
	ret = ENOMEM;
	goto out;
    }
    krb5_storage_set_byteorder(in, KRB5_STORAGE_BYTEORDER_LE);

    CHECK_SIZE(krb5_storage_read(in, sig, sizeof(sig)), sizeof(sig));
    CHECK(memcmp(ntlmsigature, sig, sizeof(ntlmsigature)), 0);
    CHECK(krb5_ret_uint32(in, &type), 0);
    CHECK(type, 3);
    CHECK(ret_sec_buffer(in, &lm), 0);
    if (lm.allocated)
	min_offset = min(min_offset, lm.offset);
    CHECK(ret_sec_buffer(in, &ntlm), 0);
    if (ntlm.allocated)
	min_offset = min(min_offset, ntlm.offset);
    CHECK(ret_sec_buffer(in, &target), 0);
    min_offset = min(min_offset, target.offset);
    CHECK(ret_sec_buffer(in, &username), 0);
    min_offset = min(min_offset, username.offset);
    CHECK(ret_sec_buffer(in, &ws), 0);
    if (ws.allocated)
	min_offset = min(min_offset, ws.offset);

    if (min_offset >= 52) {
	CHECK(ret_sec_buffer(in, &sessionkey), 0);
	min_offset = min(min_offset, sessionkey.offset);
	CHECK(krb5_ret_uint32(in, &type3->flags), 0);
    }
    if (min_offset >= 52 + SIZE_SEC_BUFFER + 4 + SIZE_OS_VERSION) {
	CHECK(krb5_ret_uint32(in, &type3->os[0]), 0);
	CHECK(krb5_ret_uint32(in, &type3->os[1]), 0);
    }
    if (min_offset >= 52 + SIZE_SEC_BUFFER + 4 + SIZE_OS_VERSION + 16) {
	type3->mic_offset = 52 + SIZE_SEC_BUFFER + 4 + SIZE_OS_VERSION;
	CHECK_SIZE(krb5_storage_read(in, type3->mic, sizeof(type3->mic)), sizeof(type3->mic));
    } else
	type3->mic_offset = 0;
    CHECK(ret_buf(in, &lm, &type3->lm), 0);
    CHECK(ret_buf(in, &ntlm, &type3->ntlm), 0);
    CHECK(ret_sec_string(in, ucs2, &target, &type3->targetname), 0);
    CHECK(ret_sec_string(in, ucs2, &username, &type3->username), 0);
    CHECK(ret_sec_string(in, ucs2, &ws, &type3->ws), 0);
    if (sessionkey.offset)
	CHECK(ret_buf(in, &sessionkey, &type3->sessionkey), 0);

out:
    if (in)
	krb5_storage_free(in);
    if (ret)
	heim_ntlm_free_type3(type3);

    return ret;
}

/**
 * Encodes an ntlm_type3 message.
 *
 * @param type3 the ntlm_type3 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * @param[out] mic_offset offset of message integrity code
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type3(const struct ntlm_type3 *type3, struct ntlm_buf *data, size_t *mic_offset)
{
    struct sec_buffer lm, ntlm, target, username, sessionkey, ws;
    krb5_error_code ret;
    krb5_storage *out = NULL;
    uint32_t base;
    int ucs2 = 0;

    memset(&lm, 0, sizeof(lm));
    memset(&ntlm, 0, sizeof(ntlm));
    memset(&target, 0, sizeof(target));
    memset(&username, 0, sizeof(username));
    memset(&ws, 0, sizeof(ws));
    memset(&sessionkey, 0, sizeof(sessionkey));

    base = 52;

    base += 8; /* sessionkey sec buf */
    base += 4; /* flags */
    if (type3->flags & NTLM_NEG_VERSION)
	base += SIZE_OS_VERSION; /* os flags */

    if (mic_offset) {
	*mic_offset = base;
	base += 16;
    }

    if (type3->flags & NTLM_NEG_UNICODE)
	ucs2 = 1;

    target.offset = base;
    target.length = len_string(ucs2, type3->targetname);
    target.allocated = target.length;

    username.offset = target.offset + target.allocated;
    username.length = len_string(ucs2, type3->username);
    username.allocated = username.length;

    ws.offset = username.offset + username.allocated;
    ws.length = len_string(ucs2, type3->ws);
    ws.allocated = ws.length;

    lm.offset = ws.offset + ws.allocated;
    lm.length = type3->lm.length;
    lm.allocated = type3->lm.length;

    ntlm.offset = lm.offset + lm.allocated;
    ntlm.length = type3->ntlm.length;
    ntlm.allocated = ntlm.length;

    sessionkey.offset = ntlm.offset + ntlm.allocated;
    sessionkey.length = type3->sessionkey.length;
    sessionkey.allocated = type3->sessionkey.length;

    out = krb5_storage_emem();
    if (out == NULL)
	return ENOMEM;

    krb5_storage_set_byteorder(out, KRB5_STORAGE_BYTEORDER_LE);
    CHECK_SIZE(krb5_storage_write(out, ntlmsigature, sizeof(ntlmsigature)),
	  sizeof(ntlmsigature));
    CHECK(krb5_store_uint32(out, 3), 0);

    CHECK(store_sec_buffer(out, &lm), 0);
    CHECK(store_sec_buffer(out, &ntlm), 0);
    CHECK(store_sec_buffer(out, &target), 0);
    CHECK(store_sec_buffer(out, &username), 0);
    CHECK(store_sec_buffer(out, &ws), 0);
    CHECK(store_sec_buffer(out, &sessionkey), 0);
    CHECK(krb5_store_uint32(out, type3->flags), 0);

    /* os version */
    if (type3->flags & NTLM_NEG_VERSION) {
	CHECK(encode_os_version(out), 0);
    }

    if (mic_offset) {
	static const uint8_t buf[16] = { 0 };
	CHECK_SIZE(krb5_storage_write(out, buf, sizeof(buf)), sizeof(buf));
    }

    CHECK(put_string(out, ucs2, type3->targetname), 0);
    CHECK(put_string(out, ucs2, type3->username), 0);
    CHECK(put_string(out, ucs2, type3->ws), 0);
    CHECK(put_buf(out, &type3->lm), 0);
    CHECK(put_buf(out, &type3->ntlm), 0);
    CHECK(put_buf(out, &type3->sessionkey), 0);

    {
	krb5_data d;
	ret = krb5_storage_to_data(out, &d);
	data->data = d.data;
	data->length = d.length;
    }

out:
    krb5_storage_free(out);

    return ret;
}


/*
 *
 */

static void
splitandenc(unsigned char *hash,
	    unsigned char *challenge,
	    unsigned char *answer)
{
    EVP_CIPHER_CTX ctx;
    unsigned char key[8];

    key[0] =  hash[0];
    key[1] = (hash[0] << 7) | (hash[1] >> 1);
    key[2] = (hash[1] << 6) | (hash[2] >> 2);
    key[3] = (hash[2] << 5) | (hash[3] >> 3);
    key[4] = (hash[3] << 4) | (hash[4] >> 4);
    key[5] = (hash[4] << 3) | (hash[5] >> 5);
    key[6] = (hash[5] << 2) | (hash[6] >> 6);
    key[7] = (hash[6] << 1);

    EVP_CIPHER_CTX_init(&ctx);

    EVP_CipherInit_ex(&ctx, EVP_des_cbc(), NULL, key, NULL, 1);
    EVP_Cipher(&ctx, answer, challenge, 8);
    EVP_CIPHER_CTX_cleanup(&ctx);
    memset(key, 0, sizeof(key));
}

/**
 * Calculate the NTLM key, the password is assumed to be in UTF8.
 *
 * @param password password to calcute the key for.
 * @param key calcuted key, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_nt_key(const char *password, struct ntlm_buf *key)
{
    struct ntlm_buf buf;
    EVP_MD_CTX *m;
    int ret;

    key->data = malloc(MD4_DIGEST_LENGTH);
    if (key->data == NULL)
	return ENOMEM;
    key->length = MD4_DIGEST_LENGTH;

    ret = ascii2ucs2le(password, 0, &buf);
    if (ret) {
	heim_ntlm_free_buf(key);
	return ret;
    }

    m = EVP_MD_CTX_create();
    if (m == NULL) {
	heim_ntlm_free_buf(key);
	heim_ntlm_free_buf(&buf);
	return ENOMEM;
    }

    EVP_DigestInit_ex(m, EVP_md4(), NULL);
    EVP_DigestUpdate(m, buf.data, buf.length);
    EVP_DigestFinal_ex(m, key->data, NULL);
    EVP_MD_CTX_destroy(m);

    heim_ntlm_free_buf(&buf);
    return 0;
}

/**
 * Calculate NTLMv1 response hash
 *
 * @param key the ntlm v1 key
 * @param len length of key
 * @param challenge sent by the server
 * @param answer calculated answer, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm1(void *key, size_t len,
			  unsigned char challenge[8],
			  struct ntlm_buf *answer)
{
    unsigned char res[21];

    if (len != MD4_DIGEST_LENGTH)
	return HNTLM_ERR_INVALID_LENGTH;

    memcpy(res, key, len);
    memset(&res[MD4_DIGEST_LENGTH], 0, sizeof(res) - MD4_DIGEST_LENGTH);

    answer->data = malloc(24);
    if (answer->data == NULL)
	return ENOMEM;
    answer->length = 24;

    splitandenc(&res[0],  challenge, ((unsigned char *)answer->data) + 0);
    splitandenc(&res[7],  challenge, ((unsigned char *)answer->data) + 8);
    splitandenc(&res[14], challenge, ((unsigned char *)answer->data) + 16);

    return 0;
}

int
heim_ntlm_v1_base_session(void *key, size_t len,
			  struct ntlm_buf *session)
{
    EVP_MD_CTX *m;

    session->length = MD4_DIGEST_LENGTH;
    session->data = malloc(session->length);
    if (session->data == NULL) {
	session->length = 0;
	return ENOMEM;
    }
    
    m = EVP_MD_CTX_create();
    if (m == NULL) {
	heim_ntlm_free_buf(session);
	return ENOMEM;
    }
    EVP_DigestInit_ex(m, EVP_md4(), NULL);
    EVP_DigestUpdate(m, key, len);
    EVP_DigestFinal_ex(m, session->data, NULL);
    EVP_MD_CTX_destroy(m);

    return 0;
}

int
heim_ntlm_v2_base_session(void *key, size_t len,
			  struct ntlm_buf *ntlmResponse,
			  struct ntlm_buf *session)
{
    unsigned int hmaclen;
    HMAC_CTX c;

    if (ntlmResponse->length <= 16)
        return HNTLM_ERR_INVALID_LENGTH;

    session->data = malloc(16);
    if (session->data == NULL)
	return ENOMEM;
    session->length = 16;

    /* Note: key is the NTLMv2 key */
    HMAC_CTX_init(&c);
    HMAC_Init_ex(&c, key, len, EVP_md5(), NULL);
    HMAC_Update(&c, ntlmResponse->data, 16);
    HMAC_Final(&c, session->data, &hmaclen);
    HMAC_CTX_cleanup(&c);

    return 0;
}


int
heim_ntlm_keyex_wrap(struct ntlm_buf *base_session,
		     struct ntlm_buf *session,
		     struct ntlm_buf *encryptedSession)
{
    EVP_CIPHER_CTX c;
    int ret;

    if (base_session->length != MD4_DIGEST_LENGTH)
	return HNTLM_ERR_INVALID_LENGTH;

    session->length = MD4_DIGEST_LENGTH;
    session->data = malloc(session->length);
    if (session->data == NULL) {
	session->length = 0;
	return ENOMEM;
    }
    encryptedSession->length = MD4_DIGEST_LENGTH;
    encryptedSession->data = malloc(encryptedSession->length);
    if (encryptedSession->data == NULL) {
	heim_ntlm_free_buf(session);
	encryptedSession->length = 0;
	return ENOMEM;
    }

    EVP_CIPHER_CTX_init(&c);

    ret = EVP_CipherInit_ex(&c, EVP_rc4(), NULL, base_session->data, NULL, 1);
    if (ret != 1) {
	EVP_CIPHER_CTX_cleanup(&c);
	heim_ntlm_free_buf(encryptedSession);
	heim_ntlm_free_buf(session);
	return HNTLM_ERR_CRYPTO;
    }

    if (RAND_bytes(session->data, session->length) != 1) {
	EVP_CIPHER_CTX_cleanup(&c);
	heim_ntlm_free_buf(encryptedSession);
	heim_ntlm_free_buf(session);
	return HNTLM_ERR_RAND;
    }

    EVP_Cipher(&c, encryptedSession->data, session->data, encryptedSession->length);
    EVP_CIPHER_CTX_cleanup(&c);

    return 0;



}

/**
 * Generates an NTLMv1 session random with assosited session master key.
 *
 * @param key the ntlm v1 key
 * @param len length of key
 * @param session generated session nonce, should be freed with heim_ntlm_free_buf().
 * @param master calculated session master key, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_build_ntlm1_master(void *key, size_t len,
			     struct ntlm_buf *session,
			     struct ntlm_buf *master)
{
    struct ntlm_buf sess;
    int ret;

    ret = heim_ntlm_v1_base_session(key, len, &sess);
    if (ret)
	return ret;

    ret = heim_ntlm_keyex_wrap(&sess, session, master);
    heim_ntlm_free_buf(&sess);

    return ret;
}

/**
 * Generates an NTLMv2 session random with associated session master key.
 *
 * @param key the NTLMv2 key
 * @param len length of key
 * @param blob the NTLMv2 "blob"
 * @param session generated session nonce, should be freed with heim_ntlm_free_buf().
 * @param master calculated session master key, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */


int
heim_ntlm_build_ntlm2_master(void *key, size_t len,
			     struct ntlm_buf *blob,
			     struct ntlm_buf *session,
			     struct ntlm_buf *master)
{
    struct ntlm_buf sess;
    int ret;

    ret = heim_ntlm_v2_base_session(key, len, blob, &sess);
    if (ret)
	return ret;

    ret = heim_ntlm_keyex_wrap(&sess, session, master);
    heim_ntlm_free_buf(&sess);

    return ret;
}

/**
 * Given a key and encrypted session, unwrap the session key
 *
 * @param baseKey the sessionBaseKey
 * @param encryptedSession encrypted session, type3.session field.
 * @param session generated session nonce, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_keyex_unwrap(struct ntlm_buf *baseKey,
		       struct ntlm_buf *encryptedSession,
		       struct ntlm_buf *session)
{
    EVP_CIPHER_CTX c;

    memset(session, 0, sizeof(*session));

    if (encryptedSession->length != MD4_DIGEST_LENGTH)
	return HNTLM_ERR_INVALID_LENGTH;
    if (baseKey->length != MD4_DIGEST_LENGTH)
	return HNTLM_ERR_INVALID_LENGTH;

    session->length = MD4_DIGEST_LENGTH;
    session->data = malloc(session->length);
    if (session->data == NULL) {
	session->length = 0;
	return ENOMEM;
    }
    EVP_CIPHER_CTX_init(&c);

    if (EVP_CipherInit_ex(&c, EVP_rc4(), NULL, baseKey->data, NULL, 0) != 1) {
	EVP_CIPHER_CTX_cleanup(&c);
	heim_ntlm_free_buf(session);
	return HNTLM_ERR_CRYPTO;
    }

    EVP_Cipher(&c, session->data, encryptedSession->data, session->length);
    EVP_CIPHER_CTX_cleanup(&c);

    return 0;
}


/**
 * Generates an NTLMv2 session key.
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param upper_case_target upper case the target, should not be used only for legacy systems
 * @param ntlmv2 the ntlmv2 session key
 *
 * @return 0 on success, or an error code on failure.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_ntlmv2_key(const void *key, size_t len,
		     const char *username,
		     const char *target,
		     int upper_case_target,
		     unsigned char ntlmv2[16])
{
    int ret;
    unsigned int hmaclen;
    HMAC_CTX c;

    HMAC_CTX_init(&c);
    HMAC_Init_ex(&c, key, len, EVP_md5(), NULL);
    {
	struct ntlm_buf buf;
	/* uppercase username and turn it into ucs2-le */
	ret = ascii2ucs2le(username, 1, &buf);
	if (ret)
	    goto out;
	HMAC_Update(&c, buf.data, buf.length);
	free(buf.data);
	/* turn target into ucs2-le */
	ret = ascii2ucs2le(target, upper_case_target, &buf);
	if (ret)
	    goto out;
	HMAC_Update(&c, buf.data, buf.length);
	free(buf.data);
    }
    HMAC_Final(&c, ntlmv2, &hmaclen);
 out:
    HMAC_CTX_cleanup(&c);
    memset(&c, 0, sizeof(c));

    return ret;
}

/*
 *
 */

#define NTTIME_EPOCH 0x019DB1DED53E8000LL

uint64_t
heim_ntlm_unix2ts_time(time_t unix_time)
{
    long long wt;
    wt = unix_time * (uint64_t)10000000 + (uint64_t)NTTIME_EPOCH;
    return wt;
}

time_t
heim_ntlm_ts2unixtime(uint64_t t)
{
    t = ((t - (uint64_t)NTTIME_EPOCH) / (uint64_t)10000000);
    if (t > (((uint64_t)(time_t)(~(uint64_t)0)) >> 1))
	return 0;
    return (time_t)t;
}

/**
 * Calculate LMv2 response
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param serverchallenge challenge as sent by the server in the type2 message.
 * @param ntlmv2 calculated session key
 * @param answer ntlm response answer, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_lm2(const void *key, size_t len,
			const char *username,
			const char *target,
			const unsigned char serverchallenge[8],
			unsigned char ntlmv2[16],
			struct ntlm_buf *answer)
{
    unsigned char clientchallenge[8];

    if (RAND_bytes(clientchallenge, sizeof(clientchallenge)) != 1)
	return HNTLM_ERR_RAND;

    /* calculate ntlmv2 key */

    heim_ntlm_ntlmv2_key(key, len, username, target, 0, ntlmv2);

    answer->data = malloc(24);
    if (answer->data == NULL)
        return ENOMEM;
    answer->length = 24;

    heim_ntlm_derive_ntlm2_sess(ntlmv2, clientchallenge, 8,
				serverchallenge, answer->data);

    memcpy(((unsigned char *)answer->data) + 16, clientchallenge, 8);

    return 0;
}


/**
 * Calculate NTLMv2 response
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param serverchallenge challenge as sent by the server in the type2 message.
 * @param infotarget infotarget as sent by the server in the type2 message.
 * @param ntlmv2 calculated session key
 * @param answer ntlm response answer, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm2(const void *key, size_t len,
			  const char *username,
			  const char *target,
			  const unsigned char serverchallenge[8],
			  const struct ntlm_buf *infotarget,
			  unsigned char ntlmv2[16],
			  struct ntlm_buf *answer)
{
    krb5_error_code ret;
    krb5_data data;
    unsigned char ntlmv2answer[16];
    krb5_storage *sp;
    unsigned char clientchallenge[8];
    uint64_t t;

    t = heim_ntlm_unix2ts_time(time(NULL));

    if (RAND_bytes(clientchallenge, sizeof(clientchallenge)) != 1)
	return HNTLM_ERR_RAND;

    /* calculate ntlmv2 key */

    heim_ntlm_ntlmv2_key(key, len, username, target, 0, ntlmv2);

    /* calculate and build ntlmv2 answer */

    sp = krb5_storage_emem();
    if (sp == NULL)
	return ENOMEM;
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(krb5_store_uint32(sp, 0x00000101), 0);
    CHECK(krb5_store_uint32(sp, 0), 0);
    /* timestamp le 64 bit ts */
    CHECK(krb5_store_uint32(sp, t & 0xffffffff), 0);
    CHECK(krb5_store_uint32(sp, t >> 32), 0);

    CHECK_SIZE(krb5_storage_write(sp, clientchallenge, 8), 8);

    CHECK(krb5_store_uint32(sp, 0), 0);  /* Z(4) */
    CHECK_SIZE(krb5_storage_write(sp, infotarget->data, infotarget->length),
	  infotarget->length);

    /*
     * These last 4 bytes(Z(4)) are not documented by MicroSoft and
     * SnowLeopard doesn't send them, Lion expected them to be there,
     * so we have to continue to send them. That is ok, since everyone
     * else (except Snow) seems to do that too.
     */
    CHECK(krb5_store_uint32(sp, 0), 0); /* Z(4) */

    CHECK(krb5_storage_to_data(sp, &data), 0);
    krb5_storage_free(sp);
    sp = NULL;

    heim_ntlm_derive_ntlm2_sess(ntlmv2, data.data, data.length, serverchallenge, ntlmv2answer);

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_data_free(&data);
	return ENOMEM;
    }

    CHECK_SIZE(krb5_storage_write(sp, ntlmv2answer, 16), 16);
    CHECK_SIZE(krb5_storage_write(sp, data.data, data.length), data.length);
    krb5_data_free(&data);

    CHECK(krb5_storage_to_data(sp, &data), 0);
    krb5_storage_free(sp);
    sp = NULL;

    answer->data = data.data;
    answer->length = data.length;

    return 0;
out:
    if (sp)
	krb5_storage_free(sp);
    return ret;
}

static const int authtimediff = 3600 * 2; /* 2 hours */

static int
verify_ntlm2(const void *key, size_t len,
	     const char *username,
	     const char *target,
	     int upper_case_target,
	     time_t now,
	     const unsigned char serverchallenge[8],
	     const struct ntlm_buf *answer,
	     struct ntlm_buf *infotarget,
	     unsigned char ntlmv2[16])
{
    krb5_error_code ret;
    unsigned char clientanswer[16];
    unsigned char clientnonce[8];
    unsigned char serveranswer[16];
    krb5_storage *sp;
    uint64_t t;
    time_t authtime;
    uint32_t temp;

    infotarget->length = 0;
    infotarget->data = NULL;

    if (answer->length < 16)
	return HNTLM_ERR_INVALID_LENGTH;

    if (now == 0)
	now = time(NULL);

    /* calculate ntlmv2 key */

    heim_ntlm_ntlmv2_key(key, len, username, target, upper_case_target, ntlmv2);

    /* calculate and build ntlmv2 answer */

    sp = krb5_storage_from_readonly_mem(answer->data, answer->length);
    if (sp == NULL)
	return ENOMEM;
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    CHECK_SIZE(krb5_storage_read(sp, clientanswer, 16), 16);

    CHECK(krb5_ret_uint32(sp, &temp), 0);
    CHECK(temp, 0x00000101);
    CHECK(krb5_ret_uint32(sp, &temp), 0);
    CHECK(temp, 0);
    /* timestamp le 64 bit ts */
    CHECK(krb5_ret_uint32(sp, &temp), 0);
    t = temp;
    CHECK(krb5_ret_uint32(sp, &temp), 0);
    t |= ((uint64_t)temp)<< 32;

    authtime = heim_ntlm_ts2unixtime(t);

    if (labs((int)(authtime - now)) > authtimediff) {
	ret = HNTLM_ERR_TIME_SKEW;
	goto out;
    }

    /* client challenge */
    CHECK_SIZE(krb5_storage_read(sp, clientnonce, 8), 8);

    CHECK(krb5_ret_uint32(sp, &temp), 0); /* Z(4) */

    /* let pick up targetinfo */
    infotarget->length = answer->length - (size_t)krb5_storage_seek(sp, 0, SEEK_CUR);
    if (infotarget->length < 4) {
	ret = HNTLM_ERR_INVALID_LENGTH;
	goto out;
    }
    infotarget->data = malloc(infotarget->length);
    if (infotarget->data == NULL) {
	ret = ENOMEM;
	goto out;
    }
    CHECK_SIZE(krb5_storage_read(sp, infotarget->data, infotarget->length),
	  infotarget->length);

    krb5_storage_free(sp);
    sp = NULL;

    if (answer->length < 16) {
	ret = HNTLM_ERR_INVALID_LENGTH;
	goto out;
    }

    heim_ntlm_derive_ntlm2_sess(ntlmv2,
				((unsigned char *)answer->data) + 16, answer->length - 16,
				serverchallenge,
				serveranswer);

    if (memcmp(serveranswer, clientanswer, 16) != 0) {
	heim_ntlm_free_buf(infotarget);
	return HNTLM_ERR_AUTH;
    }

    return 0;
out:
    heim_ntlm_free_buf(infotarget);
    if (sp)
	krb5_storage_free(sp);
    return ret;
}

/**
 * Verify NTLMv2 response.
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param now the time now (0 if the library should pick it up itself)
 * @param serverchallenge challenge as sent by the server in the type2 message.
 * @param answer ntlm response answer, should be freed with heim_ntlm_free_buf().
 * @param infotarget infotarget as sent by the server in the type2 message.
 * @param ntlmv2 calculated session key
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_verify_ntlm2(const void *key, size_t len,
		       const char *username,
		       const char *target,
		       time_t now,
		       const unsigned char serverchallenge[8],
		       const struct ntlm_buf *answer,
		       struct ntlm_buf *infotarget,
		       unsigned char ntlmv2[16])
{
    int ret;
    
    /**
     * First check with the domain as the client passed it to the function.
     */

    ret = verify_ntlm2(key, len, username, target, 0, now,
		       serverchallenge, answer, infotarget, ntlmv2);

    /**
     * Second check with domain uppercased.
     */

    if (ret)
	ret = verify_ntlm2(key, len, username, target, 1, now,
			   serverchallenge, answer, infotarget, ntlmv2);

    /**
     * Third check with empty domain.
     */
    if (ret)
	ret = verify_ntlm2(key, len, username, "", 0, now,
			   serverchallenge, answer, infotarget, ntlmv2);
    return ret;
}

/*
 * Calculate the NTLM2 Session Response
 *
 * @param clnt_nonce client nonce
 * @param svr_chal server challage
 * @param ntlm2_hash ntlm hash
 * @param lm The LM response, should be freed with heim_ntlm_free_buf().
 * @param ntlm The NTLM response, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm2_sess(const unsigned char clnt_nonce[8],
			       const unsigned char svr_chal[8],
			       const unsigned char ntlm_hash[16],
			       struct ntlm_buf *lm,
			       struct ntlm_buf *ntlm)
{
    unsigned char ntlm2_sess_hash[8];
    unsigned char res[21], *resp;
    int code;

    code = heim_ntlm_calculate_ntlm2_sess_hash(clnt_nonce, svr_chal,
					       ntlm2_sess_hash);
    if (code) {
	return code;
    }

    lm->data = malloc(24);
    if (lm->data == NULL) {
	return ENOMEM;
    }
    lm->length = 24;

    ntlm->data = malloc(24);
    if (ntlm->data == NULL) {
	free(lm->data);
	lm->data = NULL;
	return ENOMEM;
    }
    ntlm->length = 24;

    /* first setup the lm resp */
    memset(lm->data, 0, 24);
    memcpy(lm->data, clnt_nonce, 8);

    memset(res, 0, sizeof(res));
    memcpy(res, ntlm_hash, 16);

    resp = ntlm->data;
    splitandenc(&res[0], ntlm2_sess_hash, resp + 0);
    splitandenc(&res[7], ntlm2_sess_hash, resp + 8);
    splitandenc(&res[14], ntlm2_sess_hash, resp + 16);

    return 0;
}


/*
 * Calculate the NTLM2 Session "Verifier"
 *
 * @param clnt_nonce client nonce
 * @param svr_chal server challage
 * @param hash The NTLM session verifier
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm2_sess_hash(const unsigned char clnt_nonce[8],
				    const unsigned char svr_chal[8],
				    unsigned char verifier[8])
{
    unsigned char ntlm2_sess_hash[MD5_DIGEST_LENGTH];
    EVP_MD_CTX *m;

    m = EVP_MD_CTX_create();
    if (m == NULL)
	return ENOMEM;

    EVP_DigestInit_ex(m, EVP_md5(), NULL);
    EVP_DigestUpdate(m, svr_chal, 8); /* session nonce part 1 */
    EVP_DigestUpdate(m, clnt_nonce, 8); /* session nonce part 2 */
    EVP_DigestFinal_ex(m, ntlm2_sess_hash, NULL); /* will only use first 8 bytes */
    EVP_MD_CTX_destroy(m);

    memcpy(verifier, ntlm2_sess_hash, 8);

    return 0;
}


/*
 * Derive a NTLM2 session key
 *
 * @param sessionkey session key from domain controller
 * @param clnt_nonce client nonce
 * @param svr_chal server challenge
 * @param derivedkey salted session key
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_derive_ntlm2_sess(const unsigned char sessionkey[16],
			    const unsigned char *clnt_nonce, size_t clnt_nonce_length,
			    const unsigned char svr_chal[8],
			    unsigned char derivedkey[16])
{
    unsigned int hmaclen;
    HMAC_CTX c;

    /* HMAC(Ksession, serverchallenge || clientchallenge) */
    HMAC_CTX_init(&c);
    HMAC_Init_ex(&c, sessionkey, 16, EVP_md5(), NULL);
    HMAC_Update(&c, svr_chal, 8);
    HMAC_Update(&c, clnt_nonce, clnt_nonce_length);
    HMAC_Final(&c, derivedkey, &hmaclen);
    HMAC_CTX_cleanup(&c);
    memset(&c, 0, sizeof(c));
}
