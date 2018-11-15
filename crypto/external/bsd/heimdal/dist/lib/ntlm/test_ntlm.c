/*	$NetBSD: test_ntlm.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdio.h>
#include <err.h>
#include <krb5/roken.h>
#include <krb5/getarg.h>

#include <krb5/krb5-types.h> /* or <inttypes.h> */
#include <krb5/heimntlm.h>

static int dumpdata_flag;

static int
test_parse(void)
{
    const char *user = "foo",
	*domain = "mydomain",
	*hostname = "myhostname",
	*password = "digestpassword",
	*target = "DOMAIN";
    struct ntlm_type1 type1;
    struct ntlm_type2 type2;
    struct ntlm_type3 type3;
    struct ntlm_buf data;
    int ret, flags;

    memset(&type1, 0, sizeof(type1));

    type1.flags = NTLM_NEG_UNICODE|NTLM_NEG_TARGET|NTLM_NEG_NTLM|NTLM_NEG_VERSION;
    type1.domain = rk_UNCONST(domain);
    type1.hostname = rk_UNCONST(hostname);
    type1.os[0] = 0;
    type1.os[1] = 0;

    ret = heim_ntlm_encode_type1(&type1, &data);
    if (ret)
	errx(1, "heim_ntlm_encode_type1");

    memset(&type1, 0, sizeof(type1));

    if (dumpdata_flag)
	rk_dumpdata("ntlm-type1", data.data, data.length);

    ret = heim_ntlm_decode_type1(&data, &type1);
    free(data.data);
    if (ret)
	errx(1, "heim_ntlm_encode_type1");

    if (strcmp(type1.domain, domain) != 0)
	errx(1, "parser got domain wrong: %s", type1.domain);

    if (strcmp(type1.hostname, hostname) != 0)
	errx(1, "parser got hostname wrong: %s", type1.hostname);

    heim_ntlm_free_type1(&type1);

    /*
     *
     */

    memset(&type2, 0, sizeof(type2));

    flags = NTLM_NEG_UNICODE | NTLM_NEG_NTLM | NTLM_TARGET_DOMAIN;
    type2.flags = flags;

    memset(type2.challenge, 0x7f, sizeof(type2.challenge));
    type2.targetname = rk_UNCONST(target);
    type2.targetinfo.data = NULL;
    type2.targetinfo.length = 0;

    ret = heim_ntlm_encode_type2(&type2, &data);
    if (ret)
	errx(1, "heim_ntlm_encode_type2");

    memset(&type2, 0, sizeof(type2));

    if (dumpdata_flag)
	rk_dumpdata("ntlm-type2", data.data, data.length);

    ret = heim_ntlm_decode_type2(&data, &type2);
    free(data.data);
    if (ret)
	errx(1, "heim_ntlm_decode_type2");

    heim_ntlm_free_type2(&type2);

    /*
     *
     */

    memset(&type3, 0, sizeof(type3));

    type3.flags = flags;
    type3.username = rk_UNCONST(user);
    type3.targetname = rk_UNCONST(target);
    type3.ws = rk_UNCONST("workstation");

    {
	struct ntlm_buf key;
	heim_ntlm_nt_key(password, &key);

	heim_ntlm_calculate_ntlm1(key.data, key.length,
				  type2.challenge,
				  &type3.ntlm);
	free(key.data);
    }

    ret = heim_ntlm_encode_type3(&type3, &data, NULL);
    if (ret)
	errx(1, "heim_ntlm_encode_type3");

    free(type3.ntlm.data);

    memset(&type3, 0, sizeof(type3));

    if (dumpdata_flag)
	rk_dumpdata("ntlm-type3", data.data, data.length);

    ret = heim_ntlm_decode_type3(&data, 1, &type3);
    free(data.data);
    if (ret)
	errx(1, "heim_ntlm_decode_type3");

    if (strcmp("workstation", type3.ws) != 0)
	errx(1, "type3 ws wrong");

    if (strcmp(target, type3.targetname) != 0)
	errx(1, "type3 targetname wrong");

    if (strcmp(user, type3.username) != 0)
	errx(1, "type3 username wrong");


    heim_ntlm_free_type3(&type3);

    /*
     * NTLMv2
     */

    memset(&type2, 0, sizeof(type2));

    flags = NTLM_NEG_UNICODE | NTLM_NEG_NTLM | NTLM_TARGET_DOMAIN;
    type2.flags = flags;

    memset(type2.challenge, 0x7f, sizeof(type2.challenge));
    type2.targetname = rk_UNCONST(target);
    type2.targetinfo.data = "\x00\x00";
    type2.targetinfo.length = 2;

    ret = heim_ntlm_encode_type2(&type2, &data);
    if (ret)
	errx(1, "heim_ntlm_encode_type2");

    memset(&type2, 0, sizeof(type2));

    ret = heim_ntlm_decode_type2(&data, &type2);
    free(data.data);
    if (ret)
	errx(1, "heim_ntlm_decode_type2");

    heim_ntlm_free_type2(&type2);

    return 0;
}

static int
test_keys(void)
{
    const char
	*username = "test",
	*password = "test1234",
	*target = "TESTNT";
    const unsigned char
	serverchallenge[8] = "\x67\x7f\x1c\x55\x7a\x5e\xe9\x6c";
    struct ntlm_buf infotarget, infotarget2, answer, key;
    unsigned char ntlmv2[16], ntlmv2_1[16];
    int ret;

    infotarget.length = 70;
    infotarget.data =
	"\x02\x00\x0c\x00\x54\x00\x45\x00\x53\x00\x54\x00\x4e\x00\x54\x00"
	"\x01\x00\x0c\x00\x4d\x00\x45\x00\x4d\x00\x42\x00\x45\x00\x52\x00"
	"\x03\x00\x1e\x00\x6d\x00\x65\x00\x6d\x00\x62\x00\x65\x00\x72\x00"
	    "\x2e\x00\x74\x00\x65\x00\x73\x00\x74\x00\x2e\x00\x63\x00\x6f"
	    "\x00\x6d\x00"
	"\x00\x00\x00\x00";

    answer.length = 0;
    answer.data = NULL;

    heim_ntlm_nt_key(password, &key);

    ret = heim_ntlm_calculate_ntlm2(key.data,
				    key.length,
				    username,
				    target,
				    serverchallenge,
				    &infotarget,
				    ntlmv2,
				    &answer);
    if (ret)
	errx(1, "heim_ntlm_calculate_ntlm2");

    ret = heim_ntlm_verify_ntlm2(key.data,
				 key.length,
				 username,
				 target,
				 0,
				 serverchallenge,
				 &answer,
				 &infotarget2,
				 ntlmv2_1);
    if (ret)
	errx(1, "heim_ntlm_verify_ntlm2");

    if (memcmp(ntlmv2, ntlmv2_1, sizeof(ntlmv2)) != 0)
	errx(1, "ntlm master key not same");

    if (infotarget.length > infotarget2.length)
	errx(1, "infotarget length");

    if (memcmp(infotarget.data, infotarget2.data, infotarget.length) != 0)
	errx(1, "infotarget not the same");

    free(key.data);
    free(answer.data);
    free(infotarget2.data);

    return 0;
}

static int
test_ntlm2_session_resp(void)
{
    int ret;
    struct ntlm_buf lm, ntlm;

    const unsigned char lm_resp[24] =
	"\xff\xff\xff\x00\x11\x22\x33\x44"
	"\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00";
    const unsigned char ntlm2_sess_resp[24] =
	"\x10\xd5\x50\x83\x2d\x12\xb2\xcc"
	"\xb7\x9d\x5a\xd1\xf4\xee\xd3\xdf"
	"\x82\xac\xa4\xc3\x68\x1d\xd4\x55";

    const unsigned char client_nonce[8] =
	"\xff\xff\xff\x00\x11\x22\x33\x44";
    const unsigned char server_challenge[8] =
	"\x01\x23\x45\x67\x89\xab\xcd\xef";

    const unsigned char ntlm_hash[16] =
	"\xcd\x06\xca\x7c\x7e\x10\xc9\x9b"
	"\x1d\x33\xb7\x48\x5a\x2e\xd8\x08";

    ret = heim_ntlm_calculate_ntlm2_sess(client_nonce,
					 server_challenge,
					 ntlm_hash,
					 &lm,
					 &ntlm);
    if (ret)
	errx(1, "heim_ntlm_calculate_ntlm2_sess_resp");

    if (lm.length != 24 || memcmp(lm.data, lm_resp, 24) != 0)
	errx(1, "lm_resp wrong");
    if (ntlm.length != 24 || memcmp(ntlm.data, ntlm2_sess_resp, 24) != 0)
	errx(1, "ntlm2_sess_resp wrong");

    free(lm.data);
    free(ntlm.data);


    return 0;
}

static int
test_ntlmv2(void)
{
    unsigned char type3[413] = 
	"\x4e\x54\x4c\x4d\x53\x53\x50\x00\x03\x00\x00\x00\x18\x00\x18\x00"
	"\x80\x00\x00\x00\x9e\x00\x9e\x00\x98\x00\x00\x00\x14\x00\x14\x00"
	"\x48\x00\x00\x00\x10\x00\x10\x00\x5c\x00\x00\x00\x14\x00\x14\x00"
	"\x6c\x00\x00\x00\x00\x00\x00\x00\x36\x01\x00\x00\x05\x82\x88\xa2"
	"\x05\x01\x28\x0a\x00\x00\x00\x0f\x43\x00\x4f\x00\x4c\x00\x4c\x00"
	"\x45\x00\x59\x00\x2d\x00\x58\x00\x50\x00\x34\x00\x54\x00\x45\x00"
	"\x53\x00\x54\x00\x55\x00\x53\x00\x45\x00\x52\x00\x43\x00\x4f\x00"
	"\x4c\x00\x4c\x00\x45\x00\x59\x00\x2d\x00\x58\x00\x50\x00\x34\x00"
	"\x2f\x96\xec\x0a\xf7\x9f\x2e\x24\xba\x09\x48\x10\xa5\x22\xd4\xe1"
	"\x16\x6a\xca\x58\x74\x9a\xc1\x4f\x54\x6f\xee\x40\x96\xce\x43\x6e"
	"\xdf\x99\x20\x71\x6c\x9a\xda\x2a\x01\x01\x00\x00\x00\x00\x00\x00"
	"\x8d\xc0\x57\xc9\x79\x5e\xcb\x01\x16\x6a\xca\x58\x74\x9a\xc1\x4f"
	"\x00\x00\x00\x00\x02\x00\x14\x00\x4e\x00\x55\x00\x54\x00\x43\x00"
	"\x52\x00\x41\x00\x43\x00\x4b\x00\x45\x00\x52\x00\x01\x00\x14\x00"
	"\x4e\x00\x55\x00\x54\x00\x43\x00\x52\x00\x41\x00\x43\x00\x4b\x00"
	"\x45\x00\x52\x00\x04\x00\x12\x00\x61\x00\x70\x00\x70\x00\x6c\x00"
	"\x65\x00\x2e\x00\x63\x00\x6f\x00\x6d\x00\x03\x00\x20\x00\x68\x00"
	"\x75\x00\x6d\x00\x6d\x00\x65\x00\x6c\x00\x2e\x00\x61\x00\x70\x00"
	"\x70\x00\x6c\x00\x65\x00\x2e\x00\x63\x00\x6f\x00\x6d\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x57\x00\x69\x00\x6e\x00\x64\x00\x6f"
	"\x00\x77\x00\x73\x00\x20\x00\x32\x00\x30\x00\x30\x00\x32\x00\x20"
	"\x00\x53\x00\x65\x00\x72\x00\x76\x00\x69\x00\x63\x00\x65\x00\x20"
	"\x00\x50\x00\x61\x00\x63\x00\x6b\x00\x20\x00\x33\x00\x20\x00\x32"
	"\x00\x36\x00\x30\x00\x30\x00\x00\x00\x57\x00\x69\x00\x6e\x00\x64"
	"\x00\x6f\x00\x77\x00\x73\x00\x20\x00\x32\x00\x30\x00\x30\x00\x32"
	"\x00\x20\x00\x35\x00\x2e\x00\x31\x00\x00\x00\x00\x00";
    const unsigned char challenge[8] = 
	"\xe4\x9c\x6a\x12\xe1\xbd\xde\x6a";
    unsigned char sessionkey[16];

    const char key[16] = "\xD1\x83\x98\x3E\xAE\xA7\xBE\x99\x59\xC8\xF4\xC1\x98\xED\x0E\x68";

    struct ntlm_buf data;
    struct ntlm_type3 t3;
    int ret;

    struct ntlm_targetinfo ti;

    unsigned char timsg[114] = 
	"\002\000\024\000N\000U\000T\000C\000R\000A\000C\000K\000E\000R\000\001\000\024\000N\000U\000T\000C\000R\000A\000C\000K\000E\000R\000\004\000\022\000a\000p\000p\000l\000e\000.\000c\000o\000m\000\003\000 \000h\000u\000m\000m\000e\000l\000.\000a\000p\000p\000l\000e\000.\000c\000o\000m\000\000\000\000\000\000\000\000";


    data.data = type3;
    data.length = sizeof(type3);

    ret = heim_ntlm_decode_type3(&data, 1, &t3);
    if (ret)
	errx(1, "heim_ntlm_decode_type3");
    
    memset(&ti, 0, sizeof(ti));

    data.data = timsg;
    data.length = sizeof(timsg);

    ret = heim_ntlm_decode_targetinfo(&data, 1, &ti);
    if (ret)
	return ret;

    ret = heim_ntlm_verify_ntlm2(key, sizeof(key),
				 t3.username,
				 t3.targetname,
				 1285615547,
				 challenge,
				 &t3.ntlm,
				 &data,
				 sessionkey);
    if (ret)
	errx(1, "verify_ntlmv2");

    if (sizeof(timsg) != data.length || memcmp(timsg, data.data, sizeof(timsg)) != 0)
	errx(1, "target info wrong: %d != %d",
	     (int)sizeof(timsg), (int)data.length);

    heim_ntlm_free_type3(&t3);
    heim_ntlm_free_targetinfo(&ti);

    return 0;
}

static int
test_targetinfo(void)
{
    struct ntlm_targetinfo ti;
    struct ntlm_buf buf;
    const char *dnsservername = "dnsservername";
    const char *targetname = "targetname";
    const char z16[16] = { 0 };
    int ret;

    memset(&ti, 0, sizeof(ti));

    ti.dnsservername = rk_UNCONST(dnsservername);
    ti.avflags = 1;
    ti.targetname = rk_UNCONST(targetname);
    ti.channel_bindings.data = rk_UNCONST(z16);
    ti.channel_bindings.length = sizeof(z16);

    ret = heim_ntlm_encode_targetinfo(&ti, 1, &buf);
    if (ret)
	return ret;

    memset(&ti, 0, sizeof(ti));

    ret = heim_ntlm_decode_targetinfo(&buf, 1, &ti);
    if (ret)
	return ret;

    if (ti.dnsservername == NULL ||
	strcmp(ti.dnsservername, dnsservername) != 0)
	errx(1, "ti.dnshostname != %s", dnsservername);
    if (ti.avflags != 1)
	errx(1, "ti.avflags != 1");
    if (ti.targetname == NULL ||
	strcmp(ti.targetname, targetname) != 0)
	errx(1, "ti.targetname != %s", targetname);

    if (ti.channel_bindings.length != sizeof(z16) ||
	memcmp(ti.channel_bindings.data, z16, sizeof(z16)) != 0)
	errx(1, "ti.channel_bindings != Z(16)");

    heim_ntlm_free_targetinfo(&ti);

    return 0;
}

static int
test_string2key(void)
{
    const char *pw = "山田";
    struct ntlm_buf buf;

    unsigned char key[16] = {
	0xc6, 0x5d, 0xc7, 0x61, 0xa1, 0x34, 0x17, 0xa1,
	0x17, 0x08, 0x9c, 0x1b, 0xb0, 0x0d, 0x0f, 0x19
    };

    if (heim_ntlm_nt_key(pw, &buf) != 0)
	errx(1, "heim_ntlmv_nt_key(jp)");

    if (buf.length != 16 || memcmp(buf.data, key, 16) != 0)
	errx(1, "compare failed");

    heim_ntlm_free_buf(&buf);

    return 0;
}

static int
test_jp(void)
{
    char buf2[220] =
	"\x4e\x54\x4c\x4d\x53\x53\x50\x00\x02\x00\x00\x00\x06\x00\x06\x00"
	"\x38\x00\x00\x00\x05\x02\x89\x62\x62\x94\xb1\xf3\x56\x80\xb0\xf9"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x9e\x00\x9e\x00\x3e\x00\x00\x00"
	"\x06\x01\xb0\x1d\x00\x00\x00\x0f\x43\x00\x4f\x00\x53\x00\x02\x00"
	"\x06\x00\x43\x00\x4f\x00\x53\x00\x01\x00\x12\x00\x43\x00\x4f\x00"
	"\x53\x00\x57\x00\x49\x00\x4e\x00\x37\x00\x4a\x00\x50\x00\x04\x00"
	"\x1a\x00\x63\x00\x6f\x00\x73\x00\x2e\x00\x61\x00\x70\x00\x70\x00"
	"\x6c\x00\x65\x00\x2e\x00\x63\x00\x6f\x00\x6d\x00\x03\x00\x2e\x00"
	"\x63\x00\x6f\x00\x73\x00\x77\x00\x69\x00\x6e\x00\x37\x00\x6a\x00"
	"\x70\x00\x2e\x00\x63\x00\x6f\x00\x73\x00\x2e\x00\x61\x00\x70\x00"
	"\x70\x00\x6c\x00\x65\x00\x2e\x00\x63\x00\x6f\x00\x6d\x00\x05\x00"
	"\x1a\x00\x63\x00\x6f\x00\x73\x00\x2e\x00\x61\x00\x70\x00\x70\x00"
	"\x6c\x00\x65\x00\x2e\x00\x63\x00\x6f\x00\x6d\x00\x07\x00\x08\x00"
	"\x94\x51\xf0\xbd\xdc\x61\xcb\x01\x00\x00\x00\x00";

    char buf3[362] =
	"\x4e\x54\x4c\x4d\x53\x53\x50\x00\x03\x00\x00\x00\x18\x00\x18\x00"
	"\x74\x00\x00\x00\xce\x00\xce\x00\x8c\x00\x00\x00\x1a\x00\x1a\x00"
	"\x40\x00\x00\x00\x04\x00\x04\x00\x5a\x00\x00\x00\x16\x00\x16\x00"
	"\x5e\x00\x00\x00\x10\x00\x10\x00\x5a\x01\x00\x00\x05\x02\x89\x62"
	"\x31\x00\x37\x00\x2e\x00\x32\x00\x30\x00\x31\x00\x2e\x00\x35\x00"
	"\x37\x00\x2e\x00\x31\x00\x32\x00\x31\x00\x71\x5c\x30\x75\x77\x00"
	"\x6f\x00\x72\x00\x6b\x00\x73\x00\x74\x00\x61\x00\x74\x00\x69\x00"
	"\x6f\x00\x6e\x00\xab\xad\xeb\x72\x01\xd4\x5f\xdf\x59\x07\x5f\xa9"
	"\xfd\x54\x98\x2d\xfa\x17\xbb\xf1\x3c\x8f\xf5\x20\xe6\x8f\xd7\x0a"
	"\xc9\x19\x3e\x94\x61\x31\xdb\x0f\x55\xe8\xe2\x53\x01\x01\x00\x00"
	"\x00\x00\x00\x00\x00\x06\x3e\x30\xe4\x61\xcb\x01\x71\x98\x10\x6b"
	"\x4c\x82\xec\xb3\x00\x00\x00\x00\x02\x00\x06\x00\x43\x00\x4f\x00"
	"\x53\x00\x01\x00\x12\x00\x43\x00\x4f\x00\x53\x00\x57\x00\x49\x00"
	"\x4e\x00\x37\x00\x4a\x00\x50\x00\x04\x00\x1a\x00\x63\x00\x6f\x00"
	"\x73\x00\x2e\x00\x61\x00\x70\x00\x70\x00\x6c\x00\x65\x00\x2e\x00"
	"\x63\x00\x6f\x00\x6d\x00\x03\x00\x2e\x00\x63\x00\x6f\x00\x73\x00"
	"\x77\x00\x69\x00\x6e\x00\x37\x00\x6a\x00\x70\x00\x2e\x00\x63\x00"
	"\x6f\x00\x73\x00\x2e\x00\x61\x00\x70\x00\x70\x00\x6c\x00\x65\x00"
	"\x2e\x00\x63\x00\x6f\x00\x6d\x00\x05\x00\x1a\x00\x63\x00\x6f\x00"
	"\x73\x00\x2e\x00\x61\x00\x70\x00\x70\x00\x6c\x00\x65\x00\x2e\x00"
	"\x63\x00\x6f\x00\x6d\x00\x07\x00\x08\x00\xab\xec\xcc\x30\xe4\x61"
	"\xcb\x01\x00\x00\x00\x00\x00\x00\x00\x00\xbc\x2e\xba\x3f\xd1\xb1"
	"\xa7\x70\x00\x9d\x55\xa0\x59\x74\x2b\x78";


    struct ntlm_type2 type2;
    struct ntlm_type3 type3;
    struct ntlm_buf data;
    int ret;

    data.length = sizeof(buf2);
    data.data = buf2;

    memset(&type2, 0, sizeof(type2));

    ret = heim_ntlm_decode_type2(&data, &type2);
    if (ret)
	errx(1, "heim_ntlm_decode_type2(jp): %d", ret);

    data.data = NULL;
    data.length = 0;

    ret = heim_ntlm_encode_type2(&type2, &data);
    if (ret)
	errx(1, "heim_ntlm_encode_type2(jp): %d", ret);

    heim_ntlm_free_type2(&type2);
    heim_ntlm_free_buf(&data);

    data.length = sizeof(buf3);
    data.data = buf3;

    memset(&type3, 0, sizeof(type3));

    ret = heim_ntlm_decode_type3(&data, 1, &type3);
    if (ret)
	errx(1, "heim_ntlm_decode_type2(jp): %d", ret);

    data.data = NULL;
    data.length = 0;

    ret = heim_ntlm_encode_type3(&type3, &data, NULL);
    if (ret)
	errx(1, "heim_ntlm_decode_type2(jp): %d", ret);

    heim_ntlm_free_type3(&type3);
    heim_ntlm_free_buf(&data);

    return 0;
}


static int verbose_flag = 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"verbose",	0,	arg_flag,	&verbose_flag, "verbose printing", NULL },
    {"version",	0,	arg_flag,	&version_flag, "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args),
		    NULL, "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int ret = 0, optidx = 0;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    if (verbose_flag)
	printf("test_parse\n");
    ret |= test_parse();

    if (verbose_flag)
	printf("test_keys\n");
    ret |= test_keys();

    if (verbose_flag)
	printf("test_ntlm2_session_resp\n");
    ret |= test_ntlm2_session_resp();

    if (verbose_flag)
	printf("test_targetinfo\n");
    ret |= test_targetinfo();
	
    if (verbose_flag)
	printf("test_ntlmv2\n");
    ret |= test_ntlmv2();

    if (verbose_flag)
	printf("test_string2key\n");
    ret |= test_string2key();

    if (verbose_flag)
	printf("test_jp\n");
    ret |= test_jp();

    return ret;
}
