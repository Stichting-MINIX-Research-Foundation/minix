/*
 * Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: t_hashes.c,v 1.5 2010-10-04 22:27:41 marka Exp $ */

/*
 * -d1 or larger shows hash or HMAC result even if correct
 */

#include <config.h>

#include <stdlib.h>

#include <isc/hmacmd5.h>
#include <isc/hmacsha.h>
#include <isc/md5.h>
#include <isc/print.h>
#include <isc/sha1.h>
#include <isc/string.h>
#include <isc/util.h>

#include <tests/t_api.h>


static int	    nprobs;

typedef void(*HASH_INIT)(void *);
typedef void(*HMAC_INIT)(void *, const unsigned char *, unsigned int);
typedef void(*UPDATE)(void *, const unsigned char *, unsigned int);
typedef void(*FINAL)(void *, const unsigned char *);
typedef void(*SIGN)(void *, const unsigned char *, unsigned int);

typedef struct {
    const char *name;
    const unsigned char	*key;
    const unsigned int	key_len;
    const unsigned char	*str;
    const unsigned int	str_len;
} IN;
#define STR_INIT(s)	(const unsigned char *)(s), sizeof(s)-1


union {
    unsigned char b[1024];
    unsigned char md5[16];
    unsigned char sha1[ISC_SHA1_DIGESTLENGTH];
    unsigned char sha224[ISC_SHA224_DIGESTLENGTH];
    unsigned char sha256[ISC_SHA256_DIGESTLENGTH];
    unsigned char sha384[ISC_SHA384_DIGESTLENGTH];
    unsigned char sha512[ISC_SHA512_DIGESTLENGTH];
} dbuf;
#define DIGEST_FILL 0xdf

typedef struct {
    const char		*str;
    const unsigned int	digest_len;
} OUT;


/*
 * two ad hoc hash examples
 */
static IN abc = { "\"abc\"", NULL, 0, STR_INIT("abc")};
static OUT abc_sha1 = {
	"a9993e364706816aba3e25717850c26c9cd0d89d",
	ISC_SHA1_DIGESTLENGTH};
static OUT abc_sha224 = {
	"23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
	ISC_SHA224_DIGESTLENGTH};
static OUT abc_md5 = {
	"900150983cd24fb0d6963f7d28e17f72",
	16};

static IN abc_blah = { "\"abcdbc...\"", NULL, 0,
	STR_INIT("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")};
static OUT abc_blah_sha1 =  {
	"84983e441c3bd26ebaae4aa1f95129e5e54670f1",
	ISC_SHA1_DIGESTLENGTH};
static OUT abc_blah_sha224 = {
	"75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525",
	ISC_SHA224_DIGESTLENGTH};
static OUT abc_blah_md5 = {
	"8215ef0796a20bcaaae116d3876c664a",
	16};

/*
 * three HMAC-md5 examples from RFC 2104
 */
static const unsigned char rfc2104_1_key[16] = {
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b};
static IN rfc2104_1 = {"RFC 2104 #1", rfc2104_1_key, sizeof(rfc2104_1_key),
	STR_INIT("Hi There")};
static OUT rfc2104_1_hmac = {
	"9294727a3638bb1c13f48ef8158bfc9d",
	16};

static IN rfc2104_2 = {"RFC 2104 #2", STR_INIT("Jefe"),
	STR_INIT("what do ya want for nothing?")};
static OUT rfc2104_2_hmac = {
	"750c783e6ab0b503eaa86e310a5db738",
	16};

static const unsigned char rfc2104_3_key[16] = {
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
static const unsigned char rfc2104_3_s[50] = {
	0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
	0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
	0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
	0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
	0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
	0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
	0xDD, 0xDD};
static IN rfc2104_3 = {"RFC 2104 #3", rfc2104_3_key, sizeof(rfc2104_3_key),
	rfc2104_3_s, sizeof(rfc2104_3_s)};
static OUT rfc2104_3_hmac = {
	"56be34521d144c88dbb8c733f0e8b3f6",
	16};

/*
 * four three HMAC-SHA tests cut-and-pasted from RFC 4634 starting on page 86
 */
static const unsigned char rfc4634_1_key[20] = {
      0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
      0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b
};
static IN rfc4634_1 = {"RFC 4634 #1", rfc4634_1_key, sizeof(rfc4634_1_key),
	STR_INIT("Hi There")};
static OUT rfc4634_1_sha1 = {
	"B617318655057264E28BC0B6FB378C8EF146BE00",
	ISC_SHA1_DIGESTLENGTH};
static OUT rfc4634_1_sha224 = {
	"896FB1128ABBDF196832107CD49DF33F47B4B1169912BA4F53684B22",
	ISC_SHA224_DIGESTLENGTH};
static OUT rfc4634_1_sha256 = {
	"B0344C61D8DB38535CA8AFCEAF0BF12B881DC200C9833DA726E9376C2E32"
	"CFF7",
	ISC_SHA256_DIGESTLENGTH};
static OUT rfc4634_1_sha384 = {
	"AFD03944D84895626B0825F4AB46907F15F9DADBE4101EC682AA034C7CEB"
	"C59CFAEA9EA9076EDE7F4AF152E8B2FA9CB6",
	ISC_SHA384_DIGESTLENGTH};
static OUT rfc4634_1_sha512 = {
	"87AA7CDEA5EF619D4FF0B4241A1D6CB02379F4E2CE4EC2787AD0B30545E1"
	"7CDEDAA833B7D6B8A702038B274EAEA3F4E4BE9D914EEB61F1702E696C20"
	"3A126854",
	ISC_SHA512_DIGESTLENGTH};

static IN rfc4634_2 = {"RFC 4634 #2", STR_INIT("Jefe"),
	STR_INIT("what do ya want for nothing?")};
static OUT rfc4634_2_sha1 = {
	"EFFCDF6AE5EB2FA2D27416D5F184DF9C259A7C79",
	ISC_SHA1_DIGESTLENGTH};
static OUT rfc4634_2_sha224 = {
	"A30E01098BC6DBBF45690F3A7E9E6D0F8BBEA2A39E6148008FD05E44",
	ISC_SHA224_DIGESTLENGTH};
static OUT rfc4634_2_sha256 = {
	"5BDCC146BF60754E6A042426089575C75A003F089D2739839DEC58B964EC"
	"3843",
	ISC_SHA256_DIGESTLENGTH};
static OUT rfc4634_2_sha384 = {
	"AF45D2E376484031617F78D2B58A6B1B9C7EF464F5A01B47E42EC3736322"
	"445E8E2240CA5E69E2C78B3239ECFAB21649",
	ISC_SHA384_DIGESTLENGTH};
static OUT rfc4634_2_sha512 = {
	"164B7A7BFCF819E2E395FBE73B56E0A387BD64222E831FD610270CD7EA25"
	"05549758BF75C05A994A6D034F65F8F0E6FDCAEAB1A34D4A6B4B636E070A"
	"38BCE737",
	ISC_SHA512_DIGESTLENGTH};

static const unsigned char rfc4634_3_key[20] = {
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa
};
static const unsigned char rfc4634_3_s[50] = {
	0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
	0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
	0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
	0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
	0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd
};
static IN rfc4634_3 = {"RFC 4634 #3", rfc4634_3_key, sizeof(rfc4634_3_key),
	rfc4634_3_s, sizeof(rfc4634_3_s)};
static OUT rfc4634_3_sha1 = {
	"125D7342B9AC11CD91A39AF48AA17B4F63F175D3",
	ISC_SHA1_DIGESTLENGTH};
static OUT rfc4634_3_sha224 = {
	"7FB3CB3588C6C1F6FFA9694D7D6AD2649365B0C1F65D69D1EC8333EA",
	ISC_SHA224_DIGESTLENGTH};
static OUT rfc4634_3_sha256 = {
	"773EA91E36800E46854DB8EBD09181A72959098B3EF8C122D9635514CED5"
	"65FE",
	ISC_SHA256_DIGESTLENGTH};
static OUT rfc4634_3_sha384 = {
	"88062608D3E6AD8A0AA2ACE014C8A86F0AA635D947AC9FEBE83EF4E55966"
	"144B2A5AB39DC13814B94E3AB6E101A34F27",
	ISC_SHA384_DIGESTLENGTH};
static OUT rfc4634_3_sha512 = {
	"FA73B0089D56A284EFB0F0756C890BE9B1B5DBDD8EE81A3655F83E33B227"
	"9D39BF3E848279A722C806B485A47E67C807B946A337BEE8942674278859"
	"E13292FB",
	ISC_SHA512_DIGESTLENGTH};

static const unsigned char rfc4634_4_key[25] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19
};
static const unsigned char rfc4634_4_s[50] = {
	0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
	0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
	0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
	0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
	0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd
};
static IN rfc4634_4 = {"RFC 4634 #3", rfc4634_4_key, sizeof(rfc4634_4_key),
	rfc4634_4_s, sizeof(rfc4634_4_s)};
static OUT rfc4634_4_sha1 = {
	"4C9007F4026250C6BC8414F9BF50C86C2D7235DA",
	ISC_SHA1_DIGESTLENGTH};
static OUT rfc4634_4_sha224 = {
	"6C11506874013CAC6A2ABC1BB382627CEC6A90D86EFC012DE7AFEC5A",
	ISC_SHA224_DIGESTLENGTH};
static OUT rfc4634_4_sha256 = {
	"82558A389A443C0EA4CC819899F2083A85F0FAA3E578F8077A2E3FF46729"
	"665B",
	ISC_SHA256_DIGESTLENGTH};
static OUT rfc4634_4_sha384 = {
	"3E8A69B7783C25851933AB6290AF6CA77A9981480850009CC5577C6E1F57"
	"3B4E6801DD23C4A7D679CCF8A386C674CFFB",
	ISC_SHA384_DIGESTLENGTH};
static OUT rfc4634_4_sha512 = {
	"B0BA465637458C6990E5A8C5F61D4AF7E576D97FF94B872DE76F8050361E"
	"E3DBA91CA5C11AA25EB4D679275CC5788063A5F19741120C4F2DE2ADEBEB"
	"10A298DD",
	ISC_SHA512_DIGESTLENGTH};



static const char *
d2str(char *buf, unsigned int buf_len,
      const unsigned char *d, unsigned int d_len)
{
	unsigned int i, l;

	l = 0;
	for (i = 0; i < d_len && l < buf_len-4; ++i) {
		l += snprintf(&buf[l], buf_len-l, "%02x", d[i]);
	}
	if (l >= buf_len-3) {
		REQUIRE(buf_len > sizeof("..."));
		strcpy(&buf[l-sizeof(" ...")], " ...");
	}
	return buf;
}



/*
 * Compare binary digest or HMAC to string of hex digits from an RFC
 */
static void
ck(const char *name, const IN *in, const OUT *out)
{
	char buf[sizeof(dbuf)*2+1];
	const char *str_name;
	unsigned int l;

	d2str(buf, sizeof(buf), dbuf.b, out->digest_len);
	str_name = in->name != NULL ? in->name : (const char *)in->str;

	if (T_debug != 0)
		t_info("%s(%s) = %s\n", name, str_name, buf);

	if (strcasecmp(buf, out->str)) {
		t_info("%s(%s)\n%9s %s\n%9s %s\n",
		       name, str_name,
		       "is", buf,
		       "should be", out->str);
		++nprobs;
		return;
	}

	/*
	 * check that the hash or HMAC is no longer than we think it is
	 */
	for (l = out->digest_len; l < sizeof(dbuf); ++l) {
		if (dbuf.b[l] != DIGEST_FILL) {
			t_info("byte #%d after end of %s(%s) changed to %02x\n",
			       l-out->digest_len, name, str_name, dbuf.b[l]);
			++nprobs;
			break;
		}
	}
}



static void
t_hash(const char *hname, HASH_INIT init, UPDATE update, FINAL final,
      IN *in, OUT *out)
{
	union {
	    unsigned char b[1024];
	    isc_sha1_t sha1;
	    isc_md5_t md5;
	} ctx;

	init(&ctx);
	update(&ctx, in->str, in->str_len);
	memset(dbuf.b, DIGEST_FILL, sizeof(dbuf));
	final(&ctx, dbuf.b);
	ck(hname, in, out);
}



/*
 * isc_sha224_final has a different calling sequence
 */
static void
t_sha224(IN *in, OUT *out)
{
	isc_sha224_t ctx;

	memset(dbuf.b, DIGEST_FILL, sizeof(dbuf));
	isc_sha224_init(&ctx);
	isc_sha224_update(&ctx, in->str, in->str_len);
	memset(dbuf.b, DIGEST_FILL, sizeof(dbuf));
	isc_sha224_final(dbuf.b, &ctx);
	ck("SHA224", in, out);
}



static void
t_hashes(IN *in, OUT *out_sha1, OUT *out_sha224, OUT *out_md5)
{
	t_hash("SHA1", (HASH_INIT)isc_sha1_init, (UPDATE)isc_sha1_update,
	       (FINAL)isc_sha1_final, in, out_sha1);
	t_sha224(in, out_sha224);
	t_hash("md5", (HASH_INIT)isc_md5_init, (UPDATE)isc_md5_update,
	       (FINAL)isc_md5_final, in, out_md5);
}



/*
 * isc_hmacmd5_sign has a different calling sequence
 */
static void
t_md5hmac(IN *in, OUT *out)
{
	isc_hmacmd5_t ctx;

	isc_hmacmd5_init(&ctx, in->key, in->key_len);
	isc_hmacmd5_update(&ctx, in->str, in->str_len);
	memset(dbuf.b, DIGEST_FILL, sizeof(dbuf));
	isc_hmacmd5_sign(&ctx, dbuf.b);
	ck("HMAC-md5", in, out);
}



static void
t_hmac(const char *hname, HMAC_INIT init, UPDATE update, SIGN sign,
      IN *in, OUT *out)
{
	union {
	    unsigned char b[1024];
	    isc_hmacmd5_t hmacmd5;
	    isc_hmacsha1_t hmacsha1;
	    isc_hmacsha224_t hmacsha224;
	    isc_hmacsha256_t hmacsha256;
	    isc_hmacsha384_t hmacsha384;
	    isc_hmacsha512_t hmacsha512;
	} ctx;

	init(&ctx, in->key, in->key_len);
	update(&ctx, in->str, in->str_len);
	memset(dbuf.b, DIGEST_FILL, sizeof(dbuf));
	sign(&ctx, dbuf.b, out->digest_len);
	ck(hname, in, out);
}



static void
t_hmacs(IN *in, OUT *out_sha1, OUT *out_sha224, OUT *out_sha256,
	OUT *out_sha384, OUT *out_sha512)
{
	t_hmac("HMAC-SHA1", (HMAC_INIT)isc_hmacsha1_init,
	       (UPDATE)isc_hmacsha1_update, (SIGN)isc_hmacsha1_sign,
	       in, out_sha1);
	t_hmac("HMAC-SHA224", (HMAC_INIT)isc_hmacsha224_init,
	       (UPDATE)isc_hmacsha224_update, (SIGN)isc_hmacsha224_sign,
	       in, out_sha224);
	t_hmac("HMAC-SHA256", (HMAC_INIT)isc_hmacsha256_init,
	       (UPDATE)isc_hmacsha256_update, (SIGN)isc_hmacsha256_sign,
	       in, out_sha256);
	t_hmac("HMAC-SHA384", (HMAC_INIT)isc_hmacsha384_init,
	       (UPDATE)isc_hmacsha384_update, (SIGN)isc_hmacsha384_sign,
	       in, out_sha384);
	t_hmac("HMAC-SHA512", (HMAC_INIT)isc_hmacsha512_init,
	       (UPDATE)isc_hmacsha512_update, (SIGN)isc_hmacsha512_sign,
	       in, out_sha512);
}



/*
 * This will almost never fail, and so there is no need for the extra noise
 * that would come from breaking it into several tests.
 */
static void
t1(void)
{
	/*
	 * two ad hoc hash examples
	 */
	t_hashes(&abc, &abc_sha1, &abc_sha224, &abc_md5);
	t_hashes(&abc_blah, &abc_blah_sha1, &abc_blah_sha224, &abc_blah_md5);

	/*
	 * three HMAC-md5 examples from RFC 2104
	 */
	t_md5hmac(&rfc2104_1, &rfc2104_1_hmac);
	t_md5hmac(&rfc2104_2, &rfc2104_2_hmac);
	t_md5hmac(&rfc2104_3, &rfc2104_3_hmac);

	/*
	 * four HMAC-SHA tests from RFC 4634 starting on page 86
	 */
	t_hmacs(&rfc4634_1, &rfc4634_1_sha1, &rfc4634_1_sha224,
		&rfc4634_1_sha256, &rfc4634_1_sha384, &rfc4634_1_sha512);
	t_hmacs(&rfc4634_2, &rfc4634_2_sha1, &rfc4634_2_sha224,
		&rfc4634_2_sha256, &rfc4634_2_sha384, &rfc4634_2_sha512);
	t_hmacs(&rfc4634_3, &rfc4634_3_sha1, &rfc4634_3_sha224,
		&rfc4634_3_sha256, &rfc4634_3_sha384, &rfc4634_3_sha512);
	t_hmacs(&rfc4634_4, &rfc4634_4_sha1, &rfc4634_4_sha224,
		&rfc4634_4_sha256, &rfc4634_4_sha384, &rfc4634_4_sha512);

	if (nprobs != 0)
		t_result(T_FAIL);
	else
		t_result(T_PASS);
}


testspec_t	T_testlist[] = {
	{	t1,		"hashes"		},
	{	NULL,		NULL			}
};
