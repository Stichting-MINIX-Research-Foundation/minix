/*	$NetBSD: check-gen.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 1999 - 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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
#include <string.h>
#include <err.h>
#include <krb5/roken.h>

#include <krb5/asn1-common.h>
#include <krb5/asn1_err.h>
#include <krb5/der.h>
#include <krb5/krb5_asn1.h>
#include <krb5/heim_asn1.h>
#include <krb5/rfc2459_asn1.h>
#include <test_asn1.h>
#include <krb5/cms_asn1.h>

#include "check-common.h"

static char *lha_principal[] = { "lha" };
static char *lharoot_princ[] = { "lha", "root" };
static char *datan_princ[] = { "host", "nutcracker.e.kth.se" };
static char *nada_tgt_principal[] = { "krbtgt", "NADA.KTH.SE" };

static int
cmp_principal (void *a, void *b)
{
    Principal *pa = a;
    Principal *pb = b;
    int i;

    COMPARE_STRING(pa,pb,realm);
    COMPARE_INTEGER(pa,pb,name.name_type);
    COMPARE_INTEGER(pa,pb,name.name_string.len);

    for (i = 0; i < pa->name.name_string.len; i++)
	COMPARE_STRING(pa,pb,name.name_string.val[i]);

    return 0;
}

static int
test_principal (void)
{

    struct test_case tests[] = {
	{ NULL, 29,
	  "\x30\x1b\xa0\x10\x30\x0e\xa0\x03\x02\x01\x01\xa1\x07\x30\x05\x1b"
	  "\x03\x6c\x68\x61\xa1\x07\x1b\x05\x53\x55\x2e\x53\x45",
	  NULL
	},
	{ NULL, 35,
	  "\x30\x21\xa0\x16\x30\x14\xa0\x03\x02\x01\x01\xa1\x0d\x30\x0b\x1b"
	  "\x03\x6c\x68\x61\x1b\x04\x72\x6f\x6f\x74\xa1\x07\x1b\x05\x53\x55"
	  "\x2e\x53\x45",
	  NULL
	},
	{ NULL, 54,
	  "\x30\x34\xa0\x26\x30\x24\xa0\x03\x02\x01\x03\xa1\x1d\x30\x1b\x1b"
	  "\x04\x68\x6f\x73\x74\x1b\x13\x6e\x75\x74\x63\x72\x61\x63\x6b\x65"
	  "\x72\x2e\x65\x2e\x6b\x74\x68\x2e\x73\x65\xa1\x0a\x1b\x08\x45\x2e"
	  "\x4b\x54\x48\x2e\x53\x45",
	  NULL
	}
    };


    Principal values[] = {
	{ { KRB5_NT_PRINCIPAL, { 1, lha_principal } },  "SU.SE" },
	{ { KRB5_NT_PRINCIPAL, { 2, lharoot_princ } },  "SU.SE" },
	{ { KRB5_NT_SRV_HST, { 2, datan_princ } },  "E.KTH.SE" }
    };
    int i, ret;
    int ntests = sizeof(tests) / sizeof(*tests);

    for (i = 0; i < ntests; ++i) {
	tests[i].val = &values[i];
	if (asprintf (&tests[i].name, "Principal %d", i) < 0)
	    errx(1, "malloc");
	if (tests[i].name == NULL)
	    errx(1, "malloc");
    }

    ret = generic_test (tests, ntests, sizeof(Principal),
			(generic_encode)encode_Principal,
			(generic_length)length_Principal,
			(generic_decode)decode_Principal,
			(generic_free)free_Principal,
			cmp_principal,
			NULL);
    for (i = 0; i < ntests; ++i)
	free (tests[i].name);

    return ret;
}

static int
cmp_authenticator (void *a, void *b)
{
    Authenticator *aa = a;
    Authenticator *ab = b;
    int i;

    COMPARE_INTEGER(aa,ab,authenticator_vno);
    COMPARE_STRING(aa,ab,crealm);

    COMPARE_INTEGER(aa,ab,cname.name_type);
    COMPARE_INTEGER(aa,ab,cname.name_string.len);

    for (i = 0; i < aa->cname.name_string.len; i++)
	COMPARE_STRING(aa,ab,cname.name_string.val[i]);

    return 0;
}

static int
test_authenticator (void)
{
    struct test_case tests[] = {
	{ NULL, 63,
	  "\x62\x3d\x30\x3b\xa0\x03\x02\x01\x05\xa1\x0a\x1b\x08"
	  "\x45\x2e\x4b\x54\x48\x2e\x53\x45\xa2\x10\x30\x0e\xa0"
	  "\x03\x02\x01\x01\xa1\x07\x30\x05\x1b\x03\x6c\x68\x61"
	  "\xa4\x03\x02\x01\x0a\xa5\x11\x18\x0f\x31\x39\x37\x30"
	  "\x30\x31\x30\x31\x30\x30\x30\x31\x33\x39\x5a",
	  NULL
	},
	{ NULL, 67,
	  "\x62\x41\x30\x3f\xa0\x03\x02\x01\x05\xa1\x07\x1b\x05"
	  "\x53\x55\x2e\x53\x45\xa2\x16\x30\x14\xa0\x03\x02\x01"
	  "\x01\xa1\x0d\x30\x0b\x1b\x03\x6c\x68\x61\x1b\x04\x72"
	  "\x6f\x6f\x74\xa4\x04\x02\x02\x01\x24\xa5\x11\x18\x0f"
	  "\x31\x39\x37\x30\x30\x31\x30\x31\x30\x30\x31\x36\x33"
	  "\x39\x5a",
	  NULL
	}
    };

    Authenticator values[] = {
	{ 5, "E.KTH.SE", { KRB5_NT_PRINCIPAL, { 1, lha_principal } },
	  NULL, 10, 99, NULL, NULL, NULL },
	{ 5, "SU.SE", { KRB5_NT_PRINCIPAL, { 2, lharoot_princ } },
	  NULL, 292, 999, NULL, NULL, NULL }
    };
    int i, ret;
    int ntests = sizeof(tests) / sizeof(*tests);

    for (i = 0; i < ntests; ++i) {
	tests[i].val = &values[i];
	if (asprintf (&tests[i].name, "Authenticator %d", i) < 0)
	    errx(1, "malloc");
	if (tests[i].name == NULL)
	    errx(1, "malloc");
    }

    ret = generic_test (tests, ntests, sizeof(Authenticator),
			(generic_encode)encode_Authenticator,
			(generic_length)length_Authenticator,
			(generic_decode)decode_Authenticator,
			(generic_free)free_Authenticator,
			cmp_authenticator,
			(generic_copy)copy_Authenticator);
    for (i = 0; i < ntests; ++i)
	free(tests[i].name);

    return ret;
}

static int
cmp_KRB_ERROR (void *a, void *b)
{
    KRB_ERROR *aa = a;
    KRB_ERROR *ab = b;
    int i;

    COMPARE_INTEGER(aa,ab,pvno);
    COMPARE_INTEGER(aa,ab,msg_type);

    IF_OPT_COMPARE(aa,ab,ctime) {
	COMPARE_INTEGER(aa,ab,ctime);
    }
    IF_OPT_COMPARE(aa,ab,cusec) {
	COMPARE_INTEGER(aa,ab,cusec);
    }
    COMPARE_INTEGER(aa,ab,stime);
    COMPARE_INTEGER(aa,ab,susec);
    COMPARE_INTEGER(aa,ab,error_code);

    IF_OPT_COMPARE(aa,ab,crealm) {
	COMPARE_OPT_STRING(aa,ab,crealm);
    }
#if 0
    IF_OPT_COMPARE(aa,ab,cname) {
	COMPARE_OPT_STRING(aa,ab,cname);
    }
#endif
    COMPARE_STRING(aa,ab,realm);

    COMPARE_INTEGER(aa,ab,sname.name_string.len);
    for (i = 0; i < aa->sname.name_string.len; i++)
	COMPARE_STRING(aa,ab,sname.name_string.val[i]);

    IF_OPT_COMPARE(aa,ab,e_text) {
	COMPARE_OPT_STRING(aa,ab,e_text);
    }
    IF_OPT_COMPARE(aa,ab,e_data) {
	/* COMPARE_OPT_OCTET_STRING(aa,ab,e_data); */
    }

    return 0;
}

static int
test_krb_error (void)
{
    struct test_case tests[] = {
	{ NULL, 127,
	  "\x7e\x7d\x30\x7b\xa0\x03\x02\x01\x05\xa1\x03\x02\x01\x1e\xa4\x11"
	  "\x18\x0f\x32\x30\x30\x33\x31\x31\x32\x34\x30\x30\x31\x31\x31\x39"
	  "\x5a\xa5\x05\x02\x03\x04\xed\xa5\xa6\x03\x02\x01\x1f\xa7\x0d\x1b"
	  "\x0b\x4e\x41\x44\x41\x2e\x4b\x54\x48\x2e\x53\x45\xa8\x10\x30\x0e"
	  "\xa0\x03\x02\x01\x01\xa1\x07\x30\x05\x1b\x03\x6c\x68\x61\xa9\x0d"
	  "\x1b\x0b\x4e\x41\x44\x41\x2e\x4b\x54\x48\x2e\x53\x45\xaa\x20\x30"
	  "\x1e\xa0\x03\x02\x01\x01\xa1\x17\x30\x15\x1b\x06\x6b\x72\x62\x74"
	  "\x67\x74\x1b\x0b\x4e\x41\x44\x41\x2e\x4b\x54\x48\x2e\x53\x45",
	  "KRB-ERROR Test 1"
	}
    };
    int ntests = sizeof(tests) / sizeof(*tests);
    KRB_ERROR e1;
    PrincipalName lhaprincipalname = { 1, { 1, lha_principal } };
    PrincipalName tgtprincipalname = { 1, { 2, nada_tgt_principal } };
    char *realm = "NADA.KTH.SE";

    e1.pvno = 5;
    e1.msg_type = 30;
    e1.ctime = NULL;
    e1.cusec = NULL;
    e1.stime = 1069632679;
    e1.susec = 322981;
    e1.error_code = 31;
    e1.crealm = &realm;
    e1.cname = &lhaprincipalname;
    e1.realm = "NADA.KTH.SE";
    e1.sname = tgtprincipalname;
    e1.e_text = NULL;
    e1.e_data = NULL;

    tests[0].val = &e1;

    return generic_test (tests, ntests, sizeof(KRB_ERROR),
			 (generic_encode)encode_KRB_ERROR,
			 (generic_length)length_KRB_ERROR,
			 (generic_decode)decode_KRB_ERROR,
			 (generic_free)free_KRB_ERROR,
			 cmp_KRB_ERROR,
			 (generic_copy)copy_KRB_ERROR);
}

static int
cmp_Name (void *a, void *b)
{
    Name *aa = a;
    Name *ab = b;

    COMPARE_INTEGER(aa,ab,element);

    return 0;
}

static int
test_Name (void)
{
    struct test_case tests[] = {
	{ NULL, 35,
	  "\x30\x21\x31\x1f\x30\x0b\x06\x03\x55\x04\x03\x13\x04\x4c\x6f\x76"
	  "\x65\x30\x10\x06\x03\x55\x04\x07\x13\x09\x53\x54\x4f\x43\x4b\x48"
	  "\x4f\x4c\x4d",
	  "Name CN=Love+L=STOCKHOLM"
	},
	{ NULL, 35,
	  "\x30\x21\x31\x1f\x30\x0b\x06\x03\x55\x04\x03\x13\x04\x4c\x6f\x76"
	  "\x65\x30\x10\x06\x03\x55\x04\x07\x13\x09\x53\x54\x4f\x43\x4b\x48"
	  "\x4f\x4c\x4d",
	  "Name L=STOCKHOLM+CN=Love"
	}
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    Name n1, n2;
    RelativeDistinguishedName rdn1[1];
    RelativeDistinguishedName rdn2[1];
    AttributeTypeAndValue atv1[2];
    AttributeTypeAndValue atv2[2];
    unsigned cmp_CN[] = { 2, 5, 4, 3 };
    unsigned cmp_L[] = { 2, 5, 4, 7 };

    /* n1 */
    n1.element = choice_Name_rdnSequence;
    n1.u.rdnSequence.val = rdn1;
    n1.u.rdnSequence.len = sizeof(rdn1)/sizeof(rdn1[0]);
    rdn1[0].val = atv1;
    rdn1[0].len = sizeof(atv1)/sizeof(atv1[0]);

    atv1[0].type.length = sizeof(cmp_CN)/sizeof(cmp_CN[0]);
    atv1[0].type.components = cmp_CN;
    atv1[0].value.element = choice_DirectoryString_printableString;
    atv1[0].value.u.printableString.data = "Love";
    atv1[0].value.u.printableString.length = 4;

    atv1[1].type.length = sizeof(cmp_L)/sizeof(cmp_L[0]);
    atv1[1].type.components = cmp_L;
    atv1[1].value.element = choice_DirectoryString_printableString;
    atv1[1].value.u.printableString.data = "STOCKHOLM";
    atv1[1].value.u.printableString.length = 9;

    /* n2 */
    n2.element = choice_Name_rdnSequence;
    n2.u.rdnSequence.val = rdn2;
    n2.u.rdnSequence.len = sizeof(rdn2)/sizeof(rdn2[0]);
    rdn2[0].val = atv2;
    rdn2[0].len = sizeof(atv2)/sizeof(atv2[0]);

    atv2[0].type.length = sizeof(cmp_L)/sizeof(cmp_L[0]);
    atv2[0].type.components = cmp_L;
    atv2[0].value.element = choice_DirectoryString_printableString;
    atv2[0].value.u.printableString.data = "STOCKHOLM";
    atv2[0].value.u.printableString.length = 9;

    atv2[1].type.length = sizeof(cmp_CN)/sizeof(cmp_CN[0]);
    atv2[1].type.components = cmp_CN;
    atv2[1].value.element = choice_DirectoryString_printableString;
    atv2[1].value.u.printableString.data = "Love";
    atv2[1].value.u.printableString.length = 4;

    /* */
    tests[0].val = &n1;
    tests[1].val = &n2;

    return generic_test (tests, ntests, sizeof(Name),
			 (generic_encode)encode_Name,
			 (generic_length)length_Name,
			 (generic_decode)decode_Name,
			 (generic_free)free_Name,
			 cmp_Name,
			 (generic_copy)copy_Name);
}

static int
cmp_KeyUsage (void *a, void *b)
{
    KeyUsage *aa = a;
    KeyUsage *ab = b;

    return KeyUsage2int(*aa) != KeyUsage2int(*ab);
}

static int
test_bit_string (void)
{
    struct test_case tests[] = {
	{ NULL, 4,
	  "\x03\x02\x07\x80",
	  "bitstring 1"
	},
	{ NULL, 4,
	  "\x03\x02\x05\xa0",
	  "bitstring 2"
	},
	{ NULL, 5,
	  "\x03\x03\x07\x00\x80",
	  "bitstring 3"
	},
	{ NULL, 3,
	  "\x03\x01\x00",
	  "bitstring 4"
	}
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    KeyUsage ku1, ku2, ku3, ku4;

    memset(&ku1, 0, sizeof(ku1));
    ku1.digitalSignature = 1;
    tests[0].val = &ku1;

    memset(&ku2, 0, sizeof(ku2));
    ku2.digitalSignature = 1;
    ku2.keyEncipherment = 1;
    tests[1].val = &ku2;

    memset(&ku3, 0, sizeof(ku3));
    ku3.decipherOnly = 1;
    tests[2].val = &ku3;

    memset(&ku4, 0, sizeof(ku4));
    tests[3].val = &ku4;


    return generic_test (tests, ntests, sizeof(KeyUsage),
			 (generic_encode)encode_KeyUsage,
			 (generic_length)length_KeyUsage,
			 (generic_decode)decode_KeyUsage,
			 (generic_free)free_KeyUsage,
			 cmp_KeyUsage,
			 (generic_copy)copy_KeyUsage);
}

static int
cmp_TicketFlags (void *a, void *b)
{
    TicketFlags *aa = a;
    TicketFlags *ab = b;

    return TicketFlags2int(*aa) != TicketFlags2int(*ab);
}

static int
test_bit_string_rfc1510 (void)
{
    struct test_case tests[] = {
	{ NULL, 7,
	  "\x03\x05\x00\x80\x00\x00\x00",
	  "TF bitstring 1"
	},
	{ NULL, 7,
	  "\x03\x05\x00\x40\x20\x00\x00",
	  "TF bitstring 2"
	},
	{ NULL, 7,
	  "\x03\x05\x00\x00\x20\x00\x00",
	  "TF bitstring 3"
	},
	{ NULL, 7,
	  "\x03\x05\x00\x00\x00\x00\x00",
	  "TF bitstring 4"
	}
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    TicketFlags tf1, tf2, tf3, tf4;

    memset(&tf1, 0, sizeof(tf1));
    tf1.reserved = 1;
    tests[0].val = &tf1;

    memset(&tf2, 0, sizeof(tf2));
    tf2.forwardable = 1;
    tf2.pre_authent = 1;
    tests[1].val = &tf2;

    memset(&tf3, 0, sizeof(tf3));
    tf3.pre_authent = 1;
    tests[2].val = &tf3;

    memset(&tf4, 0, sizeof(tf4));
    tests[3].val = &tf4;


    return generic_test (tests, ntests, sizeof(TicketFlags),
			 (generic_encode)encode_TicketFlags,
			 (generic_length)length_TicketFlags,
			 (generic_decode)decode_TicketFlags,
			 (generic_free)free_TicketFlags,
			 cmp_TicketFlags,
			 (generic_copy)copy_TicketFlags);
}

static int
cmp_KerberosTime (void *a, void *b)
{
    KerberosTime *aa = a;
    KerberosTime *ab = b;

    return *aa != *ab;
}

static int
test_time (void)
{
    struct test_case tests[] = {
	{ NULL,  17,
	  "\x18\x0f\x31\x39\x37\x30\x30\x31\x30\x31\x30\x31\x31\x38\x33\x31"
	  "\x5a",
	  "time 1" },
	{ NULL,  17,
	  "\x18\x0f\x32\x30\x30\x39\x30\x35\x32\x34\x30\x32\x30\x32\x34\x30"
	  "\x5a",
	  "time 2" }
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    KerberosTime times[] = {
	4711,
	1243130560
    };

    tests[0].val = &times[0];
    tests[1].val = &times[1];

    return generic_test (tests, ntests, sizeof(KerberosTime),
			 (generic_encode)encode_KerberosTime,
			 (generic_length)length_KerberosTime,
			 (generic_decode)decode_KerberosTime,
			 (generic_free)free_KerberosTime,
			 cmp_KerberosTime,
			 (generic_copy)copy_KerberosTime);
}

struct {
    const char *cert;
    size_t len;
} certs[] = {
    {
	"\x30\x82\x02\x6c\x30\x82\x01\xd5\xa0\x03\x02\x01\x02\x02\x09\x00"
	"\x99\x32\xde\x61\x0e\x40\x19\x8a\x30\x0d\x06\x09\x2a\x86\x48\x86"
	"\xf7\x0d\x01\x01\x05\x05\x00\x30\x2a\x31\x1b\x30\x19\x06\x03\x55"
	"\x04\x03\x0c\x12\x68\x78\x35\x30\x39\x20\x54\x65\x73\x74\x20\x52"
	"\x6f\x6f\x74\x20\x43\x41\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13"
	"\x02\x53\x45\x30\x1e\x17\x0d\x30\x39\x30\x34\x32\x36\x32\x30\x32"
	"\x39\x34\x30\x5a\x17\x0d\x31\x39\x30\x34\x32\x34\x32\x30\x32\x39"
	"\x34\x30\x5a\x30\x2a\x31\x1b\x30\x19\x06\x03\x55\x04\x03\x0c\x12"
	"\x68\x78\x35\x30\x39\x20\x54\x65\x73\x74\x20\x52\x6f\x6f\x74\x20"
	"\x43\x41\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x53\x45\x30"
	"\x81\x9f\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05"
	"\x00\x03\x81\x8d\x00\x30\x81\x89\x02\x81\x81\x00\xb9\xd3\x1b\x67"
	"\x1c\xf7\x5e\x26\x81\x3b\x82\xff\x03\xa4\x43\xb5\xb2\x63\x0b\x89"
	"\x58\x43\xfe\x3d\xe0\x38\x7d\x93\x74\xbb\xad\x21\xa4\x29\xd9\x34"
	"\x79\xf3\x1c\x8c\x5a\xd6\xb0\xd7\x19\xea\xcc\xaf\xe0\xa8\x40\x02"
	"\x1d\x91\xf1\xac\x36\xb0\xfb\x08\xbd\xcc\x9a\xe1\xb7\x6e\xee\x0a"
	"\x69\xbf\x6d\x2b\xee\x20\x82\x61\x06\xf2\x18\xcc\x89\x11\x64\x7e"
	"\xb2\xff\x47\xd1\x3b\x52\x73\xeb\x5a\xc0\x03\xa6\x4b\xc7\x40\x7e"
	"\xbc\xe1\x0e\x65\x44\x3f\x40\x8b\x02\x82\x54\x04\xd9\xcc\x2c\x67"
	"\x01\xb6\x16\x82\xd8\x33\x53\x17\xd7\xde\x8d\x5d\x02\x03\x01\x00"
	"\x01\xa3\x81\x99\x30\x81\x96\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16"
	"\x04\x14\x6e\x48\x13\xdc\xbf\x8b\x95\x4c\x13\xf3\x1f\x97\x30\xdd"
	"\x27\x96\x59\x9b\x0e\x68\x30\x5a\x06\x03\x55\x1d\x23\x04\x53\x30"
	"\x51\x80\x14\x6e\x48\x13\xdc\xbf\x8b\x95\x4c\x13\xf3\x1f\x97\x30"
	"\xdd\x27\x96\x59\x9b\x0e\x68\xa1\x2e\xa4\x2c\x30\x2a\x31\x1b\x30"
	"\x19\x06\x03\x55\x04\x03\x0c\x12\x68\x78\x35\x30\x39\x20\x54\x65"
	"\x73\x74\x20\x52\x6f\x6f\x74\x20\x43\x41\x31\x0b\x30\x09\x06\x03"
	"\x55\x04\x06\x13\x02\x53\x45\x82\x09\x00\x99\x32\xde\x61\x0e\x40"
	"\x19\x8a\x30\x0c\x06\x03\x55\x1d\x13\x04\x05\x30\x03\x01\x01\xff"
	"\x30\x0b\x06\x03\x55\x1d\x0f\x04\x04\x03\x02\x01\xe6\x30\x0d\x06"
	"\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05\x05\x00\x03\x81\x81\x00"
	"\x52\x9b\xe4\x0e\xee\xc2\x5d\xb7\xf1\xba\x47\xe3\xfe\xaf\x3d\x51"
	"\x10\xfd\xe8\x0d\x14\x58\x05\x36\xa7\xeb\xd8\x05\xe5\x27\x6f\x51"
	"\xb8\xec\x90\xd9\x03\xe1\xbc\x9c\x93\x38\x21\x5c\xaf\x4e\x6c\x7b"
	"\x6c\x65\xa9\x92\xcd\x94\xef\xa8\xae\x90\x12\x14\x78\x2d\xa3\x15"
	"\xaa\x42\xf1\xd9\x44\x64\x2c\x3c\xc0\xbd\x3a\x48\xd8\x80\x45\x8b"
	"\xd1\x79\x82\xe0\x0f\xdf\x08\x3c\x60\x21\x6f\x31\x47\x98\xae\x2f"
	"\xcb\xb1\xa1\xb9\xc1\xa3\x71\x5e\x4a\xc2\x67\xdf\x66\x0a\x51\xb5"
	"\xad\x60\x05\xdb\x02\xd4\x1a\xd2\xb9\x4e\x01\x08\x2b\xc3\x57\xaf",
	624 },
    {
	"\x30\x82\x02\x54\x30\x82\x01\xbd\xa0\x03\x02\x01\x02\x02\x01\x08"
	"\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05\x05\x00\x30"
	"\x2a\x31\x1b\x30\x19\x06\x03\x55\x04\x03\x0c\x12\x68\x78\x35\x30"
	"\x39\x20\x54\x65\x73\x74\x20\x52\x6f\x6f\x74\x20\x43\x41\x31\x0b"
	"\x30\x09\x06\x03\x55\x04\x06\x13\x02\x53\x45\x30\x1e\x17\x0d\x30"
	"\x39\x30\x34\x32\x36\x32\x30\x32\x39\x34\x30\x5a\x17\x0d\x31\x39"
	"\x30\x34\x32\x34\x32\x30\x32\x39\x34\x30\x5a\x30\x1b\x31\x0b\x30"
	"\x09\x06\x03\x55\x04\x06\x13\x02\x53\x45\x31\x0c\x30\x0a\x06\x03"
	"\x55\x04\x03\x0c\x03\x6b\x64\x63\x30\x81\x9f\x30\x0d\x06\x09\x2a"
	"\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x81\x8d\x00\x30\x81"
	"\x89\x02\x81\x81\x00\xd2\x41\x7a\xf8\x4b\x55\xb2\xaf\x11\xf9\x43"
	"\x9b\x43\x81\x09\x3b\x9a\x94\xcf\x00\xf4\x85\x75\x92\xd7\x2a\xa5"
	"\x11\xf1\xa8\x50\x6e\xc6\x84\x74\x24\x17\xda\x84\xc8\x03\x37\xb2"
	"\x20\xf3\xba\xb5\x59\x36\x21\x4d\xab\x70\xe2\xc3\x09\x93\x68\x14"
	"\x12\x79\xc5\xbb\x9e\x1b\x4a\xf0\xc6\x24\x59\x25\xc3\x1c\xa8\x70"
	"\x66\x5b\x3e\x41\x8e\xe3\x25\x71\x9a\x94\xa0\x5b\x46\x91\x6f\xdd"
	"\x58\x14\xec\x89\xe5\x8c\x96\xc5\x38\x60\xe4\xab\xf2\x75\xee\x6e"
	"\x62\xfc\xe1\xbd\x03\x47\xff\xc4\xbe\x0f\xca\x70\x73\xe3\x74\x58"
	"\x3a\x2f\x04\x2d\x39\x02\x03\x01\x00\x01\xa3\x81\x98\x30\x81\x95"
	"\x30\x09\x06\x03\x55\x1d\x13\x04\x02\x30\x00\x30\x0b\x06\x03\x55"
	"\x1d\x0f\x04\x04\x03\x02\x05\xe0\x30\x12\x06\x03\x55\x1d\x25\x04"
	"\x0b\x30\x09\x06\x07\x2b\x06\x01\x05\x02\x03\x05\x30\x1d\x06\x03"
	"\x55\x1d\x0e\x04\x16\x04\x14\x3a\xd3\x73\xff\xab\xdb\x7d\x8d\xc6"
	"\x3a\xa2\x26\x3e\xae\x78\x95\x80\xc9\xe6\x31\x30\x48\x06\x03\x55"
	"\x1d\x11\x04\x41\x30\x3f\xa0\x3d\x06\x06\x2b\x06\x01\x05\x02\x02"
	"\xa0\x33\x30\x31\xa0\x0d\x1b\x0b\x54\x45\x53\x54\x2e\x48\x35\x4c"
	"\x2e\x53\x45\xa1\x20\x30\x1e\xa0\x03\x02\x01\x01\xa1\x17\x30\x15"
	"\x1b\x06\x6b\x72\x62\x74\x67\x74\x1b\x0b\x54\x45\x53\x54\x2e\x48"
	"\x35\x4c\x2e\x53\x45\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01"
	"\x01\x05\x05\x00\x03\x81\x81\x00\x83\xf4\x14\xa7\x6e\x59\xff\x80"
	"\x64\xe7\xfa\xcf\x13\x80\x86\xe1\xed\x02\x38\xad\x96\x72\x25\xe5"
	"\x06\x7a\x9a\xbc\x24\x74\xa9\x75\x55\xb2\x49\x80\x69\x45\x95\x4a"
	"\x4c\x76\xa9\xe3\x4e\x49\xd3\xc2\x69\x5a\x95\x03\xeb\xba\x72\x23"
	"\x9c\xfd\x3d\x8b\xc6\x07\x82\x3b\xf4\xf3\xef\x6c\x2e\x9e\x0b\xac"
	"\x9e\x6c\xbb\x37\x4a\xa1\x9e\x73\xd1\xdc\x97\x61\xba\xfc\xd3\x49"
	"\xa6\xc2\x4c\x55\x2e\x06\x37\x76\xb5\xef\x57\xe7\x57\x58\x8a\x71"
	"\x63\xf3\xeb\xe7\x55\x68\x0d\xf6\x46\x4c\xfb\xf9\x43\xbb\x0c\x92"
	"\x4f\x4e\x22\x7b\x63\xe8\x4f\x9c",
	600
    }
};

static int
test_cert(void)
{
    Certificate c, c2;
    size_t size;
    size_t i;
    int ret;

    for (i = 0; i < sizeof(certs)/sizeof(certs[0]); i++) {

	ret = decode_Certificate((unsigned char *)certs[i].cert,
				 certs[i].len, &c, &size);
	if (ret)
	    return ret;

	ret = copy_Certificate(&c, &c2);
	free_Certificate(&c);
	if (ret)
	    return ret;

	free_Certificate(&c2);
    }

    return 0;
}

struct {
    const char *sd;
    size_t len;
} signeddata[] = {
    {
	"\x30\x80\x02\x01\x03\x31\x0b\x30\x09\x06\x05\x2b\x0e\x03\x02\x1a"
	"\x05\x00\x30\x80\x06\x07\x2b\x06\x01\x05\x02\x03\x03\xa0\x80\x24"
	"\x80\x04\x50\x30\x4e\xa0\x2b\x30\x29\xa0\x03\x02\x01\x12\xa1\x22"
	"\x04\x20\x78\xf4\x86\x31\xc6\xc2\xc9\xcb\xef\x0c\xd7\x3a\x2a\xcd"
	"\x8c\x13\x34\x83\xb1\x5c\xa8\xbe\xbf\x2f\xea\xd2\xbb\xd8\x8c\x18"
	"\x47\x01\xa1\x1f\x30\x1d\xa0\x03\x02\x01\x0c\xa1\x16\x04\x14\xa6"
	"\x2c\x52\xb2\x80\x98\x30\x40\xbc\x5f\xb0\x77\x2d\x8a\xd7\xa1\xda"
	"\x3c\xc5\x62\x00\x00\x00\x00\x00\x00\xa0\x82\x02\x09\x30\x82\x02"
	"\x05\x30\x82\x01\x6e\xa0\x03\x02\x01\x02\x02\x04\x49\x75\x57\xbf"
	"\x30\x0b\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05\x30\x3b\x31"
	"\x1f\x30\x1d\x06\x03\x55\x04\x03\x0c\x16\x63\x6f\x6d\x2e\x61\x70"
	"\x70\x6c\x65\x2e\x6b\x65\x72\x62\x65\x72\x6f\x73\x2e\x6b\x64\x63"
	"\x31\x18\x30\x16\x06\x03\x55\x04\x0a\x0c\x0f\x53\x79\x73\x74\x65"
	"\x6d\x20\x49\x64\x65\x6e\x74\x69\x74\x79\x30\x1e\x17\x0d\x30\x39"
	"\x31\x32\x30\x34\x30\x30\x32\x30\x32\x34\x5a\x17\x0d\x32\x39\x31"
	"\x31\x32\x39\x30\x30\x32\x30\x32\x34\x5a\x30\x3b\x31\x1f\x30\x1d"
	"\x06\x03\x55\x04\x03\x0c\x16\x63\x6f\x6d\x2e\x61\x70\x70\x6c\x65"
	"\x2e\x6b\x65\x72\x62\x65\x72\x6f\x73\x2e\x6b\x64\x63\x31\x18\x30"
	"\x16\x06\x03\x55\x04\x0a\x0c\x0f\x53\x79\x73\x74\x65\x6d\x20\x49"
	"\x64\x65\x6e\x74\x69\x74\x79\x30\x81\x9f\x30\x0d\x06\x09\x2a\x86"
	"\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x81\x8d\x00\x30\x81\x89"
	"\x02\x81\x81\x00\xb2\xc5\x4b\x34\xe3\x93\x99\xbb\xaa\xd1\x70\x62"
	"\x6c\x9c\xcc\xa6\xbc\x47\xc3\x23\xff\x15\xb9\x11\x27\x0a\xf8\x55"
	"\x4c\xb2\x43\x34\x75\xad\x55\xbb\xb9\x8a\xd0\x25\x64\xa4\x8c\x82"
	"\x74\x5d\x89\x52\xe2\x76\x75\x08\x67\xb5\x9c\x9c\x69\x86\x0c\x6d"
	"\x79\xf7\xa0\xbe\x42\x8f\x90\x46\x0c\x18\xf4\x7a\x56\x17\xa4\x65"
	"\x00\x3a\x5e\x3e\xbf\xbc\xf5\xe2\x2c\x26\x03\x52\xdd\xd4\x85\x3f"
	"\x03\xd7\x0c\x45\x7f\xff\xdd\x1e\x70\x6c\x9f\xb0\x8c\xd0\x33\xad"
	"\x92\x54\x17\x9d\x88\x89\x1a\xee\xef\xf7\x96\x3e\x68\xc3\xd1\x60"
	"\x47\x86\x80\x5d\x02\x03\x01\x00\x01\xa3\x18\x30\x16\x30\x14\x06"
	"\x03\x55\x1d\x25\x04\x0d\x30\x0b\x06\x09\x2a\x86\x48\x86\xf7\x63"
	"\x64\x04\x04\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05"
	"\x05\x00\x03\x81\x81\x00\x9b\xbb\xaa\x63\x66\xd8\x70\x84\x3e\xf6"
	"\xa1\x3b\xf3\xe6\xd7\x3d\xfc\x4f\xc9\x45\xaa\x31\x43\x8d\xb5\x72"
	"\xe4\x34\x95\x7b\x6e\x5f\xe5\xc8\x5e\xaf\x12\x08\x6d\xd7\x25\x76"
	"\x40\xd5\xdc\x83\x7f\x2f\x74\xd1\x63\xc0\x7c\x26\x4d\x53\x10\xe7"
	"\xfa\xcc\xf2\x60\x41\x63\xdf\x56\xd6\xd9\xc0\xb4\xd0\x73\x99\x54"
	"\x40\xad\x90\x79\x2d\xd2\x5e\xcb\x13\x22\x2b\xd0\x76\xef\x8a\x48"
	"\xfd\xb2\x6e\xca\x04\x4e\x91\x3f\xb4\x63\xad\x22\x3a\xf7\x20\x9c"
	"\x4c\x0e\x47\x78\xe5\x2a\x85\x0e\x90\x7a\xce\x46\xe6\x15\x02\xb0"
	"\x83\xe7\xac\xfa\x92\xf8\x31\x81\xe8\x30\x81\xe5\x02\x01\x01\x30"
	"\x43\x30\x3b\x31\x1f\x30\x1d\x06\x03\x55\x04\x03\x0c\x16\x63\x6f"
	"\x6d\x2e\x61\x70\x70\x6c\x65\x2e\x6b\x65\x72\x62\x65\x72\x6f\x73"
	"\x2e\x6b\x64\x63\x31\x18\x30\x16\x06\x03\x55\x04\x0a\x0c\x0f\x53"
	"\x79\x73\x74\x65\x6d\x20\x49\x64\x65\x6e\x74\x69\x74\x79\x02\x04"
	"\x49\x75\x57\xbf\x30\x09\x06\x05\x2b\x0e\x03\x02\x1a\x05\x00\x30"
	"\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x04\x81"
	"\x80\x50\x2c\x69\xe1\xd2\xc4\xd1\xcc\xdc\xe0\xe9\x8a\x6b\x6a\x97"
	"\x1b\xb4\xe0\xa8\x20\xbe\x09\x6d\xe1\x55\x5f\x07\x70\x94\x2e\x14"
	"\xed\x4e\xb1\x69\x75\x40\xbb\x99\x87\xed\x23\x50\x27\x5f\xaa\xc4"
	"\x84\x60\x06\xfe\x45\xfd\x7e\x1b\x18\xe0\x0b\x77\x35\x2a\xb2\xf2"
	"\xe0\x88\x31\xad\x82\x31\x4a\xbc\x6d\x71\x62\xe6\x4d\x33\xb4\x09"
	"\x6e\x3f\x14\x12\xf2\x89\x29\x31\x84\x60\x2b\xa8\x2d\xe6\xca\x2f"
	"\x03\x3d\xd4\x69\x89\xb3\x98\xfd\xac\x63\x14\xaf\x6a\x52\x2a\xac"
	"\xe3\x8e\xfa\x21\x41\x8f\xcc\x04\x2d\x52\xee\x49\x54\x0d\x58\x51"
	"\x77\x00\x00",
	883
    }
};

static int
test_SignedData(void)
{
    SignedData sd;
    size_t size, i;
    int ret;

    for (i = 0; i < sizeof(signeddata) / sizeof(signeddata[0]); i++) {

	ret = decode_SignedData((unsigned char *)signeddata[i].sd,
				signeddata[i].len, &sd, &size);
	if (ret)
	    return ret;

	free_SignedData(&sd);
    }

    return 0;
}


static int
cmp_TESTLargeTag (void *a, void *b)
{
    TESTLargeTag *aa = a;
    TESTLargeTag *ab = b;

    COMPARE_INTEGER(aa,ab,foo);
    COMPARE_INTEGER(aa,ab,bar);
    return 0;
}

static int
test_large_tag (void)
{
    struct test_case tests[] = {
	{ NULL,  15,  "\x30\x0d\xbf\x7f\x03\x02\x01\x01\xbf\x81\x00\x03\x02\x01\x02", "large tag 1" }
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    TESTLargeTag lt1;

    memset(&lt1, 0, sizeof(lt1));
    lt1.foo = 1;
    lt1.bar = 2;

    tests[0].val = &lt1;

    return generic_test (tests, ntests, sizeof(TESTLargeTag),
			 (generic_encode)encode_TESTLargeTag,
			 (generic_length)length_TESTLargeTag,
			 (generic_decode)decode_TESTLargeTag,
			 (generic_free)free_TESTLargeTag,
			 cmp_TESTLargeTag,
			 (generic_copy)copy_TESTLargeTag);
}

struct test_data {
    int ok;
    size_t len;
    size_t expected_len;
    void *data;
};

static int
check_tag_length(void)
{
    struct test_data td[] = {
	{ 1, 3, 3, "\x02\x01\x00"},
	{ 1, 3, 3, "\x02\x01\x7f"},
	{ 1, 4, 4, "\x02\x02\x00\x80"},
	{ 1, 4, 4, "\x02\x02\x01\x00"},
	{ 1, 4, 4, "\x02\x02\x02\x00"},
	{ 0, 3, 0, "\x02\x02\x00"},
	{ 0, 3, 0, "\x02\x7f\x7f"},
	{ 0, 4, 0, "\x02\x03\x00\x80"},
	{ 0, 4, 0, "\x02\x7f\x01\x00"},
	{ 0, 5, 0, "\x02\xff\x7f\x02\x00"}
    };
    size_t sz;
    TESTuint32 values[] = {0, 127, 128, 256, 512,
			 0, 127, 128, 256, 512 };
    TESTuint32 u;
    int i, ret, failed = 0;
    void *buf;

    for (i = 0; i < sizeof(td)/sizeof(td[0]); i++) {
	struct map_page *page;

	buf = map_alloc(OVERRUN, td[i].data, td[i].len, &page);

	ret = decode_TESTuint32(buf, td[i].len, &u, &sz);
	if (ret) {
	    if (td[i].ok) {
		printf("failed with tag len test %d\n", i);
		failed = 1;
	    }
	} else {
	    if (td[i].ok == 0) {
		printf("failed with success for tag len test %d\n", i);
		failed = 1;
	    }
	    if (td[i].expected_len != sz) {
		printf("wrong expected size for tag test %d\n", i);
		failed = 1;
	    }
	    if (values[i] != u) {
		printf("wrong value for tag test %d\n", i);
		failed = 1;
	    }
	}
	map_free(page, "test", "decode");
    }
    return failed;
}

static int
check_tag_length64(void)
{
    struct test_data td[] = {
	{ 1, 3, 3, "\x02\x01\x00"},
	{ 1, 7, 7, "\x02\x05\x01\xff\xff\xff\xff"},
	{ 1, 7, 7, "\x02\x05\x02\x00\x00\x00\x00"},
	{ 1, 9, 9, "\x02\x07\x7f\xff\xff\xff\xff\xff\xff"},
	{ 1, 10, 10, "\x02\x08\x00\x80\x00\x00\x00\x00\x00\x00"},
	{ 1, 10, 10, "\x02\x08\x7f\xff\xff\xff\xff\xff\xff\xff"},
	{ 1, 11, 11, "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xff"},
	{ 0, 3, 0, "\x02\x02\x00"},
	{ 0, 3, 0, "\x02\x7f\x7f"},
	{ 0, 4, 0, "\x02\x03\x00\x80"},
	{ 0, 4, 0, "\x02\x7f\x01\x00"},
	{ 0, 5, 0, "\x02\xff\x7f\x02\x00"}
    };
    size_t sz;
    TESTuint64 values[] = {0, 8589934591LL, 8589934592LL,
			   36028797018963967LL, 36028797018963968LL,
			   9223372036854775807LL, 18446744073709551615ULL,
			   0, 127, 128, 256, 512 };
    TESTuint64 u;
    int i, ret, failed = 0;
    void *buf;

    if (sizeof(TESTuint64) != sizeof(uint64_t)) {
	ret += 1;
	printf("sizeof(TESTuint64) %d != sizeof(uint64_t) %d\n",
	       (int)sizeof(TESTuint64), (int)sizeof(uint64_t));
    }

    for (i = 0; i < sizeof(td)/sizeof(td[0]); i++) {
	struct map_page *page;

	buf = map_alloc(OVERRUN, td[i].data, td[i].len, &page);

	ret = decode_TESTuint64(buf, td[i].len, &u, &sz);
	if (ret) {
	    if (td[i].ok) {
		printf("failed with tag len test %d\n", i);
		printf("ret = %d\n", ret);
		failed = 1;
	    }
	} else {
	    if (td[i].ok == 0) {
		printf("failed with success for tag len test %d\n", i);
		failed = 1;
	    }
	    if (td[i].expected_len != sz) {
		printf("wrong expected size for tag test %d\n", i);
		printf("sz = %lu\n", (unsigned long)sz);
		failed = 1;
	    }
	    if (values[i] != u) {
		printf("wrong value for tag test %d\n", i);
		printf("Expected value: %llu\nActual value: %llu\n",
		       (unsigned long long)values[i], (unsigned long long)u);
		failed = 1;
	    }
	}
	map_free(page, "test", "decode");
    }
    return failed;
}

static int
check_tag_length64s(void)
{
    struct test_data td[] = {
	{ 1, 3, 3, "\x02\x01\x00"},
	{ 1, 7, 7, "\x02\x05\xfe\x00\x00\x00\x01"},
	{ 1, 7, 7, "\x02\x05\xfe\x00\x00\x00\x00"},
	{ 1, 9, 9, "\x02\x07\x80\x00\x00\x00\x00\x00\x01"},
	{ 1, 9, 9, "\x02\x07\x80\x00\x00\x00\x00\x00\x00"},
	{ 1, 10, 10, "\x02\x08\x80\x00\x00\x00\x00\x00\x00\x01"},
	{ 1, 9, 9, "\x02\x07\x80\x00\x00\x00\x00\x00\x01"},
	{ 0, 3, 0, "\x02\x02\x00"},
	{ 0, 3, 0, "\x02\x7f\x7f"},
	{ 0, 4, 0, "\x02\x03\x00\x80"},
	{ 0, 4, 0, "\x02\x7f\x01\x00"},
	{ 0, 5, 0, "\x02\xff\x7f\x02\x00"}
    };
    size_t sz;
    TESTint64 values[] = {0, -8589934591LL, -8589934592LL,
			   -36028797018963967LL, -36028797018963968LL,
			   -9223372036854775807LL, -36028797018963967LL,
			   0, 127, 128, 256, 512 };
    TESTint64 u;
    int i, ret, failed = 0;
    void *buf;

    for (i = 0; i < sizeof(td)/sizeof(td[0]); i++) {
	struct map_page *page;

	buf = map_alloc(OVERRUN, td[i].data, td[i].len, &page);

	ret = decode_TESTint64(buf, td[i].len, &u, &sz);
	if (ret) {
	    if (td[i].ok) {
		printf("failed with tag len test %d\n", i);
		printf("ret = %d\n", ret);
		failed = 1;
	    }
	} else {
	    if (td[i].ok == 0) {
		printf("failed with success for tag len test %d\n", i);
		failed = 1;
	    }
	    if (td[i].expected_len != sz) {
		printf("wrong expected size for tag test %d\n", i);
		printf("sz = %lu\n", (unsigned long)sz);
		failed = 1;
	    }
	    if (values[i] != u) {
		printf("wrong value for tag test %d\n", i);
		printf("Expected value: %lld\nActual value: %lld\n",
		       (long long)values[i], (long long)u);
		failed = 1;
	    }
	}
	map_free(page, "test", "decode");
    }
    return failed;
}

static int
cmp_TESTChoice (void *a, void *b)
{
    return 0;
}

static int
test_choice (void)
{
    struct test_case tests[] = {
	{ NULL,  5,  "\xa1\x03\x02\x01\x01", "large choice 1" },
	{ NULL,  5,  "\xa2\x03\x02\x01\x02", "large choice 2" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTChoice1 c1;
    TESTChoice1 c2_1;
    TESTChoice2 c2_2;

    memset(&c1, 0, sizeof(c1));
    c1.element = choice_TESTChoice1_i1;
    c1.u.i1 = 1;
    tests[0].val = &c1;

    memset(&c2_1, 0, sizeof(c2_1));
    c2_1.element = choice_TESTChoice1_i2;
    c2_1.u.i2 = 2;
    tests[1].val = &c2_1;

    ret += generic_test (tests, ntests, sizeof(TESTChoice1),
			 (generic_encode)encode_TESTChoice1,
			 (generic_length)length_TESTChoice1,
			 (generic_decode)decode_TESTChoice1,
			 (generic_free)free_TESTChoice1,
			 cmp_TESTChoice,
			 (generic_copy)copy_TESTChoice1);

    memset(&c2_2, 0, sizeof(c2_2));
    c2_2.element = choice_TESTChoice2_asn1_ellipsis;
    c2_2.u.asn1_ellipsis.data = "\xa2\x03\x02\x01\x02";
    c2_2.u.asn1_ellipsis.length = 5;
    tests[1].val = &c2_2;

    ret += generic_test (tests, ntests, sizeof(TESTChoice2),
			 (generic_encode)encode_TESTChoice2,
			 (generic_length)length_TESTChoice2,
			 (generic_decode)decode_TESTChoice2,
			 (generic_free)free_TESTChoice2,
			 cmp_TESTChoice,
			 (generic_copy)copy_TESTChoice2);

    return ret;
}

#ifdef IMPLICIT_TAGGING_WORKS
static int
cmp_TESTImplicit (void *a, void *b)
{
    TESTImplicit *aa = a;
    TESTImplicit *ab = b;

    COMPARE_INTEGER(aa,ab,ti1);
    COMPARE_INTEGER(aa,ab,ti2.foo);
    COMPARE_INTEGER(aa,ab,ti3);
    return 0;
}
#endif

/*
UNIV CONS Sequence 14
  CONTEXT PRIM 0 1 00
  CONTEXT CONS 1 6
    CONTEXT CONS 127 3
      UNIV PRIM Integer 1 02
  CONTEXT PRIM 2 1 03
*/

static int
test_implicit (void)
{
    int ret = 0;
#ifdef IMPLICIT_TAGGING_WORKS
    struct test_case tests[] = {
	{ NULL,  18,
	  "\x30\x10\x80\x01\x00\xa1\x06\xbf"
	  "\x7f\x03\x02\x01\x02\xa2\x03\x84\x01\x03",
	  "implicit 1" }
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    TESTImplicit c0;

    memset(&c0, 0, sizeof(c0));
    c0.ti1 = 0;
    c0.ti2.foo = 2;
    c0.ti3 = 3;
    tests[0].val = &c0;

    ret += generic_test (tests, ntests, sizeof(TESTImplicit),
			 (generic_encode)encode_TESTImplicit,
			 (generic_length)length_TESTImplicit,
			 (generic_decode)decode_TESTImplicit,
			 (generic_free)free_TESTImplicit,
			 cmp_TESTImplicit,
			 (generic_copy)copy_TESTImplicit);

    ret += generic_test (tests, ntests, sizeof(TESTImplicit2),
			 (generic_encode)encode_TESTImplicit2,
			 (generic_length)length_TESTImplicit2,
			 (generic_decode)decode_TESTImplicit2,
			 (generic_free)free_TESTImplicit2,
			 cmp_TESTImplicit,
			 NULL);

#endif /* IMPLICIT_TAGGING_WORKS */
    return ret;
}

static int
cmp_TESTAlloc (void *a, void *b)
{
    TESTAlloc *aa = a;
    TESTAlloc *ab = b;

    IF_OPT_COMPARE(aa,ab,tagless) {
	COMPARE_INTEGER(aa,ab,tagless->ai);
    }

    COMPARE_INTEGER(aa,ab,three);

    IF_OPT_COMPARE(aa,ab,tagless2) {
	COMPARE_OPT_OCTET_STRING(aa, ab, tagless2);
    }

    return 0;
}

/*
UNIV CONS Sequence 12
  UNIV CONS Sequence 5
    CONTEXT CONS 0 3
      UNIV PRIM Integer 1 01
  CONTEXT CONS 1 3
    UNIV PRIM Integer 1 03

UNIV CONS Sequence 5
  CONTEXT CONS 1 3
    UNIV PRIM Integer 1 03

UNIV CONS Sequence 8
  CONTEXT CONS 1 3
    UNIV PRIM Integer 1 04
  UNIV PRIM Integer 1 05

*/

static int
test_taglessalloc (void)
{
    struct test_case tests[] = {
	{ NULL,  14,
	  "\x30\x0c\x30\x05\xa0\x03\x02\x01\x01\xa1\x03\x02\x01\x03",
	  "alloc 1" },
	{ NULL,  7,
	  "\x30\x05\xa1\x03\x02\x01\x03",
	  "alloc 2" },
	{ NULL,  10,
	  "\x30\x08\xa1\x03\x02\x01\x04\x02\x01\x05",
	  "alloc 3" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTAlloc c1, c2, c3;
    heim_any any3;

    memset(&c1, 0, sizeof(c1));
    c1.tagless = ecalloc(1, sizeof(*c1.tagless));
    c1.tagless->ai = 1;
    c1.three = 3;
    tests[0].val = &c1;

    memset(&c2, 0, sizeof(c2));
    c2.tagless = NULL;
    c2.three = 3;
    tests[1].val = &c2;

    memset(&c3, 0, sizeof(c3));
    c3.tagless = NULL;
    c3.three = 4;
    c3.tagless2 = &any3;
    any3.data = "\x02\x01\x05";
    any3.length = 3;
    tests[2].val = &c3;

    ret += generic_test (tests, ntests, sizeof(TESTAlloc),
			 (generic_encode)encode_TESTAlloc,
			 (generic_length)length_TESTAlloc,
			 (generic_decode)decode_TESTAlloc,
			 (generic_free)free_TESTAlloc,
			 cmp_TESTAlloc,
			 (generic_copy)copy_TESTAlloc);

    free(c1.tagless);

    return ret;
}

static int
cmp_TESTOptional (void *a, void *b)
{
    TESTOptional *aa = a;
    TESTOptional *ab = b;

    IF_OPT_COMPARE(aa,ab,zero) {
	COMPARE_OPT_INTEGER(aa,ab,zero);
    }
    IF_OPT_COMPARE(aa,ab,one) {
	COMPARE_OPT_INTEGER(aa,ab,one);
    }
    return 0;
}

/*
UNIV CONS Sequence 5
  CONTEXT CONS 0 3
    UNIV PRIM Integer 1 00

UNIV CONS Sequence 5
  CONTEXT CONS 1 3
    UNIV PRIM Integer 1 03

UNIV CONS Sequence 10
  CONTEXT CONS 0 3
    UNIV PRIM Integer 1 00
  CONTEXT CONS 1 3
    UNIV PRIM Integer 1 01

*/

static int
test_optional (void)
{
    struct test_case tests[] = {
	{ NULL,  2,
	  "\x30\x00",
	  "optional 0" },
	{ NULL,  7,
	  "\x30\x05\xa0\x03\x02\x01\x00",
	  "optional 1" },
	{ NULL,  7,
	  "\x30\x05\xa1\x03\x02\x01\x01",
	  "optional 2" },
	{ NULL,  12,
	  "\x30\x0a\xa0\x03\x02\x01\x00\xa1\x03\x02\x01\x01",
	  "optional 3" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTOptional c0, c1, c2, c3;
    int zero = 0;
    int one = 1;

    c0.zero = NULL;
    c0.one = NULL;
    tests[0].val = &c0;

    c1.zero = &zero;
    c1.one = NULL;
    tests[1].val = &c1;

    c2.zero = NULL;
    c2.one = &one;
    tests[2].val = &c2;

    c3.zero = &zero;
    c3.one = &one;
    tests[3].val = &c3;

    ret += generic_test (tests, ntests, sizeof(TESTOptional),
			 (generic_encode)encode_TESTOptional,
			 (generic_length)length_TESTOptional,
			 (generic_decode)decode_TESTOptional,
			 (generic_free)free_TESTOptional,
			 cmp_TESTOptional,
			 (generic_copy)copy_TESTOptional);

    return ret;
}

static int
check_fail_largetag(void)
{
    struct test_case tests[] = {
	{NULL, 14, "\x30\x0c\xbf\x87\xff\xff\xff\xff\xff\x7f\x03\x02\x01\x01",
	 "tag overflow"},
	{NULL, 0, "", "empty buffer"},
	{NULL, 7, "\x30\x05\xa1\x03\x02\x02\x01",
	 "one too short" },
	{NULL, 7, "\x30\x04\xa1\x03\x02\x02\x01",
	 "two too short" },
	{NULL, 7, "\x30\x03\xa1\x03\x02\x02\x01",
	 "three too short" },
	{NULL, 7, "\x30\x02\xa1\x03\x02\x02\x01",
	 "four too short" },
	{NULL, 7, "\x30\x01\xa1\x03\x02\x02\x01",
	 "five too short" },
	{NULL, 7, "\x30\x00\xa1\x03\x02\x02\x01",
	 "six too short" },
	{NULL, 7, "\x30\x05\xa1\x04\x02\x02\x01",
	 "inner one too long" },
	{NULL, 7, "\x30\x00\xa1\x02\x02\x02\x01",
	 "inner one too short" },
	{NULL, 8, "\x30\x05\xbf\x7f\x03\x02\x02\x01",
	 "inner one too short"},
	{NULL, 8, "\x30\x06\xbf\x64\x03\x02\x01\x01",
	 "wrong tag"},
	{NULL, 10, "\x30\x08\xbf\x9a\x9b\x38\x03\x02\x01\x01",
	 "still wrong tag"}
    };
    int ntests = sizeof(tests) / sizeof(*tests);

    return generic_decode_fail(tests, ntests, sizeof(TESTLargeTag),
			       (generic_decode)decode_TESTLargeTag);
}


static int
check_fail_sequence(void)
{
    struct test_case tests[] = {
	{NULL, 0, "", "empty buffer"},
	{NULL, 24,
	 "\x30\x16\xa0\x03\x02\x01\x01\xa1\x08\x30\x06\xbf\x7f\x03\x02\x01\x01"
	 "\x02\x01\x01\xa2\x03\x02\x01\x01",
	 "missing one byte from the end, internal length ok"},
	{NULL, 25,
	 "\x30\x18\xa0\x03\x02\x01\x01\xa1\x08\x30\x06\xbf\x7f\x03\x02\x01\x01"
	 "\x02\x01\x01\xa2\x03\x02\x01\x01",
	 "inner length one byte too long"},
	{NULL, 24,
	 "\x30\x17\xa0\x03\x02\x01\x01\xa1\x08\x30\x06\xbf\x7f\x03\x02\x01"
	 "\x01\x02\x01\x01\xa2\x03\x02\x01\x01",
	 "correct buffer but missing one too short"}
    };
    int ntests = sizeof(tests) / sizeof(*tests);

    return generic_decode_fail(tests, ntests, sizeof(TESTSeq),
			       (generic_decode)decode_TESTSeq);
}

static int
check_fail_choice(void)
{
    struct test_case tests[] = {
	{NULL, 6,
	 "\xa1\x02\x02\x01\x01",
	 "choice one too short"},
	{NULL, 6,
	 "\xa1\x03\x02\x02\x01",
	 "choice one too short inner"}
    };
    int ntests = sizeof(tests) / sizeof(*tests);

    return generic_decode_fail(tests, ntests, sizeof(TESTChoice1),
			       (generic_decode)decode_TESTChoice1);
}

static int
check_fail_Ticket(void)
{
    char buf[100];
    size_t i;
    int ret;
    struct test_case test;
    Ticket ticket;

    for (i = 0; i < sizeof(buf); i++) {
	memset(buf, 0, sizeof(buf));
	memset(&ticket, 0, sizeof(ticket));
	test.val = &ticket;
	test.byte_len = i;
	test.bytes = buf;
	test.name = "zero life";
	ret = generic_decode_fail(&test, 1, sizeof(Ticket),
				  (generic_decode)decode_Ticket);
	if (ret)
	    return ret;
    }
    return 0;
}

static int
check_seq(void)
{
    TESTSeqOf seq;
    TESTInteger i;
    int ret;

    seq.val = NULL;
    seq.len = 0;

    ret = add_TESTSeqOf(&seq, &i);
    if (ret) { printf("failed adding\n"); goto out; }
    ret = add_TESTSeqOf(&seq, &i);
    if (ret) { printf("failed adding\n"); goto out; }
    ret = add_TESTSeqOf(&seq, &i);
    if (ret) { printf("failed adding\n"); goto out; }
    ret = add_TESTSeqOf(&seq, &i);
    if (ret) { printf("failed adding\n"); goto out; }

    ret = remove_TESTSeqOf(&seq, seq.len - 1);
    if (ret) { printf("failed removing\n"); goto out; }
    ret = remove_TESTSeqOf(&seq, 2);
    if (ret) { printf("failed removing\n"); goto out; }
    ret = remove_TESTSeqOf(&seq, 0);
    if (ret) { printf("failed removing\n"); goto out; }
    ret = remove_TESTSeqOf(&seq, 0);
    if (ret) { printf("failed removing\n"); goto out; }
    ret = remove_TESTSeqOf(&seq, 0);
    if (ret == 0) {
	printf("can remove from empty list");
	return 1;
    }

    if (seq.len != 0) {
	printf("seq not empty!");
	return 1;
    }
    free_TESTSeqOf(&seq);
    ret = 0;

out:

    return ret;
}

#define test_seq_of(type, ok, ptr)					\
{									\
    heim_octet_string os;						\
    size_t size;							\
    type decode;							\
    ASN1_MALLOC_ENCODE(type, os.data, os.length, ptr, &size, ret);	\
    if (ret)								\
	return ret;							\
    if (os.length != size)						\
	abort();							\
    ret = decode_##type(os.data, os.length, &decode, &size);		\
    free(os.data);							\
    if (ret) {								\
	if (ok)								\
	    return 1;							\
    } else {								\
	free_##type(&decode);						\
	if (!ok)							\
	    return 1;							\
	if (size != 0)							\
            return 1;							\
    }									\
    return 0;								\
}

static int
check_seq_of_size(void)
{
#if 0 /* template */
    TESTInteger integers[4] = { 1, 2, 3, 4 };
    int ret;

    {
	TESTSeqSizeOf1 ssof1f1 = { 1, integers };
	TESTSeqSizeOf1 ssof1ok1 = { 2, integers };
	TESTSeqSizeOf1 ssof1f2 = { 3, integers };

	test_seq_of(TESTSeqSizeOf1, 0, &ssof1f1);
	test_seq_of(TESTSeqSizeOf1, 1, &ssof1ok1);
	test_seq_of(TESTSeqSizeOf1, 0, &ssof1f2);
    }
    {
	TESTSeqSizeOf2 ssof2f1 = { 0, NULL };
	TESTSeqSizeOf2 ssof2ok1 = { 1, integers };
	TESTSeqSizeOf2 ssof2ok2 = { 2, integers };
	TESTSeqSizeOf2 ssof2f2 = { 3, integers };

	test_seq_of(TESTSeqSizeOf2, 0, &ssof2f1);
	test_seq_of(TESTSeqSizeOf2, 1, &ssof2ok1);
	test_seq_of(TESTSeqSizeOf2, 1, &ssof2ok2);
	test_seq_of(TESTSeqSizeOf2, 0, &ssof2f2);
    }
    {
	TESTSeqSizeOf3 ssof3f1 = { 0, NULL };
	TESTSeqSizeOf3 ssof3ok1 = { 1, integers };
	TESTSeqSizeOf3 ssof3ok2 = { 2, integers };

	test_seq_of(TESTSeqSizeOf3, 0, &ssof3f1);
	test_seq_of(TESTSeqSizeOf3, 1, &ssof3ok1);
	test_seq_of(TESTSeqSizeOf3, 1, &ssof3ok2);
    }
    {
	TESTSeqSizeOf4 ssof4ok1 = { 0, NULL };
	TESTSeqSizeOf4 ssof4ok2 = { 1, integers };
	TESTSeqSizeOf4 ssof4ok3 = { 2, integers };
	TESTSeqSizeOf4 ssof4f1  = { 3, integers };

	test_seq_of(TESTSeqSizeOf4, 1, &ssof4ok1);
	test_seq_of(TESTSeqSizeOf4, 1, &ssof4ok2);
	test_seq_of(TESTSeqSizeOf4, 1, &ssof4ok3);
	test_seq_of(TESTSeqSizeOf4, 0, &ssof4f1);
   }
#endif
    return 0;
}

static int
check_TESTMechTypeList(void)
{
    TESTMechTypeList tl;
    unsigned oid1[] =  { 1, 2, 840, 48018, 1, 2, 2};
    unsigned oid2[] =  { 1, 2, 840, 113554, 1, 2, 2};
    unsigned oid3[] =   { 1, 3, 6, 1, 4, 1, 311, 2, 2, 30};
    unsigned oid4[] =   { 1, 3, 6, 1, 4, 1, 311, 2, 2, 10};
    TESTMechType array[] = {{ 7, oid1 },
                            { 7, oid2 },
                            { 10, oid3 },
                            { 10, oid4 }};
    size_t size, len;
    void *ptr;
    int ret;

    tl.len = 4;
    tl.val = array;

    ASN1_MALLOC_ENCODE(TESTMechTypeList, ptr, len, &tl, &size, ret);
    if (ret)
	errx(1, "TESTMechTypeList: %d", ret);
    if (len != size)
	abort();
    return 0;
}

#ifdef IMPLICIT_TAGGING_WORKS
static int
cmp_TESTSeqOf4(void *a, void *b)
{
    TESTSeqOf4 *aa = a;
    TESTSeqOf4 *ab = b;
    int i;

    IF_OPT_COMPARE(aa, ab, b1) {
	COMPARE_INTEGER(aa->b1, ab->b1, len);
	for (i = 0; i < aa->b1->len; ++i) {
	    COMPARE_INTEGER(aa->b1->val+i, ab->b1->val+i, u1);
	    COMPARE_INTEGER(aa->b1->val+i, ab->b1->val+i, u2);
	    COMPARE_OCTET_STRING(aa->b1->val+i, ab->b1->val+i, s1);
	    COMPARE_OCTET_STRING(aa->b1->val+i, ab->b1->val+i, s2);
	}
    }
    IF_OPT_COMPARE(aa, ab, b2) {
	COMPARE_INTEGER(aa->b2, ab->b2, len);
	for (i = 0; i < aa->b2->len; ++i) {
	    COMPARE_INTEGER(aa->b2->val+i, ab->b2->val+i, u1);
	    COMPARE_INTEGER(aa->b2->val+i, ab->b2->val+i, u2);
	    COMPARE_INTEGER(aa->b2->val+i, ab->b2->val+i, u3);
	    COMPARE_OCTET_STRING(aa->b2->val+i, ab->b2->val+i, s1);
	    COMPARE_OCTET_STRING(aa->b2->val+i, ab->b2->val+i, s2);
	    COMPARE_OCTET_STRING(aa->b2->val+i, ab->b2->val+i, s3);
	}
    }
    IF_OPT_COMPARE(aa, ab, b3) {
	COMPARE_INTEGER(aa->b3, ab->b3, len);
	for (i = 0; i < aa->b3->len; ++i) {
	    COMPARE_INTEGER(aa->b3->val+i, ab->b3->val+i, u1);
	    COMPARE_INTEGER(aa->b3->val+i, ab->b3->val+i, u2);
	    COMPARE_INTEGER(aa->b3->val+i, ab->b3->val+i, u3);
	    COMPARE_INTEGER(aa->b3->val+i, ab->b3->val+i, u4);
	    COMPARE_OCTET_STRING(aa->b3->val+i, ab->b3->val+i, s1);
	    COMPARE_OCTET_STRING(aa->b3->val+i, ab->b3->val+i, s2);
	    COMPARE_OCTET_STRING(aa->b3->val+i, ab->b3->val+i, s3);
	    COMPARE_OCTET_STRING(aa->b3->val+i, ab->b3->val+i, s4);
	}
    }
    return 0;
}
#endif  /* IMPLICIT_TAGGING_WORKS */

static int
test_seq4 (void)
{
    int ret = 0;
#ifdef IMPLICIT_TAGGING_WORKS
    struct test_case tests[] = {
	{ NULL,  2,
	  "\x30\x00",
	  "seq4 0" },
	{ NULL,  4,
	  "\x30\x02" "\xa1\x00",
	  "seq4 1" },
	{ NULL,  8,
	  "\x30\x06" "\xa0\x02\x30\x00" "\xa1\x00",
	  "seq4 2" },
	{ NULL,  2 + (2 + 0x18) + (2 + 0x27) + (2 + 0x31),
	  "\x30\x76"					/* 2 SEQ */
	   "\xa0\x18\x30\x16"				/* 4 [0] SEQ */
	    "\x30\x14"					/* 2 SEQ */
	     "\x04\x00"					/* 2 OCTET-STRING */
             "\x04\x02\x01\x02"				/* 4 OCTET-STRING */
	     "\x02\x01\x01"				/* 3 INT */
	     "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xff"
							/* 11 INT */
	   "\xa1\x27"					/* 2 [1] IMPL SEQ */
	    "\x30\x25"					/* 2 SEQ */
	     "\x02\x01\x01"				/* 3 INT */
	     "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xff"
							/* 11 INT */
	     "\x02\x09\x00\x80\x00\x00\x00\x00\x00\x00\x00"
							/* 11 INT */
	     "\x04\x00"					/* 2 OCTET-STRING */
             "\x04\x02\x01\x02"				/* 4 OCTET-STRING */
             "\x04\x04\x00\x01\x02\x03"			/* 6 OCTET-STRING */
	   "\xa2\x31"					/* 2 [2] IMPL SEQ */
	    "\x30\x2f"					/* 2 SEQ */
	     "\x04\x00"					/* 2 OCTET-STRING */
	     "\x02\x01\x01"				/* 3 INT */
             "\x04\x02\x01\x02"				/* 4 OCTET-STRING */
	     "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xff"
							/* 11 INT */
             "\x04\x04\x00\x01\x02\x03"			/* 6 OCTET-STRING */
	     "\x02\x09\x00\x80\x00\x00\x00\x00\x00\x00\x00"
							/* 11 INT */
	     "\x04\x01\x00"				/* 3 OCTET-STRING */
	     "\x02\x05\x01\x00\x00\x00\x00",		/* 7 INT */
	  "seq4 3" },
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    TESTSeqOf4 c[4];
    struct TESTSeqOf4_b1 b1[4];
    struct TESTSeqOf4_b2 b2[4];
    struct TESTSeqOf4_b3 b3[4];
    struct TESTSeqOf4_b1_val b1val[4];
    struct TESTSeqOf4_b2_val b2val[4];
    struct TESTSeqOf4_b3_val b3val[4];

    c[0].b1 = NULL;
    c[0].b2 = NULL;
    c[0].b3 = NULL;
    tests[0].val = &c[0];

    b2[1].len = 0;
    b2[1].val = NULL;
    c[1].b1 = NULL;
    c[1].b2 = &b2[1];
    c[1].b3 = NULL;
    tests[1].val = &c[1];

    b1[2].len = 0;
    b1[2].val = NULL;
    b2[2].len = 0;
    b2[2].val = NULL;
    c[2].b1 = &b1[2];
    c[2].b2 = &b2[2];
    c[2].b3 = NULL;
    tests[2].val = &c[2];

    b1val[3].s1.data = "";
    b1val[3].s1.length = 0;
    b1val[3].u1 = 1LL;
    b1val[3].s2.data = "\x01\x02";
    b1val[3].s2.length = 2;
    b1val[3].u2 = -1LL;

    b2val[3].s1.data = "";
    b2val[3].s1.length = 0;
    b2val[3].u1 = 1LL;
    b2val[3].s2.data = "\x01\x02";
    b2val[3].s2.length = 2;
    b2val[3].u2 = -1LL;
    b2val[3].s3.data = "\x00\x01\x02\x03";
    b2val[3].s3.length = 4;
    b2val[3].u3 = 1LL<<63;

    b3val[3].s1.data = "";
    b3val[3].s1.length = 0;
    b3val[3].u1 = 1LL;
    b3val[3].s2.data = "\x01\x02";
    b3val[3].s2.length = 2;
    b3val[3].u2 = -1LL;
    b3val[3].s3.data = "\x00\x01\x02\x03";
    b3val[3].s3.length = 4;
    b3val[3].u3 = 1LL<<63;
    b3val[3].s4.data = "\x00";
    b3val[3].s4.length = 1;
    b3val[3].u4 = 1LL<<32;

    b1[3].len = 1;
    b1[3].val = &b1val[3];
    b2[3].len = 1;
    b2[3].val = &b2val[3];
    b3[3].len = 1;
    b3[3].val = &b3val[3];
    c[3].b1 = &b1[3];
    c[3].b2 = &b2[3];
    c[3].b3 = &b3[3];
    tests[3].val = &c[3];

    ret += generic_test (tests, ntests, sizeof(TESTSeqOf4),
			 (generic_encode)encode_TESTSeqOf4,
			 (generic_length)length_TESTSeqOf4,
			 (generic_decode)decode_TESTSeqOf4,
			 (generic_free)free_TESTSeqOf4,
			 cmp_TESTSeqOf4,
			 (generic_copy)copy_TESTSeqOf4);
#endif  /* IMPLICIT_TAGGING_WORKS */
    return ret;
}

static int
cmp_test_seqof5 (void *a, void *b)
{
    TESTSeqOf5 *aval = a;
    TESTSeqOf5 *bval = b;

    IF_OPT_COMPARE(aval, bval, outer) {
            COMPARE_INTEGER(&aval->outer->inner, &bval->outer->inner, u0);
            COMPARE_OCTET_STRING(&aval->outer->inner, &bval->outer->inner, s0);
            COMPARE_INTEGER(&aval->outer->inner, &bval->outer->inner, u1);
            COMPARE_OCTET_STRING(&aval->outer->inner, &bval->outer->inner, s1);
            COMPARE_INTEGER(&aval->outer->inner, &bval->outer->inner, u2);
            COMPARE_OCTET_STRING(&aval->outer->inner, &bval->outer->inner, s2);
            COMPARE_INTEGER(&aval->outer->inner, &bval->outer->inner, u3);
            COMPARE_OCTET_STRING(&aval->outer->inner, &bval->outer->inner, s3);
            COMPARE_INTEGER(&aval->outer->inner, &bval->outer->inner, u4);
            COMPARE_OCTET_STRING(&aval->outer->inner, &bval->outer->inner, s4);
            COMPARE_INTEGER(&aval->outer->inner, &bval->outer->inner, u5);
            COMPARE_OCTET_STRING(&aval->outer->inner, &bval->outer->inner, s5);
            COMPARE_INTEGER(&aval->outer->inner, &bval->outer->inner, u6);
            COMPARE_OCTET_STRING(&aval->outer->inner, &bval->outer->inner, s6);
            COMPARE_INTEGER(&aval->outer->inner, &bval->outer->inner, u7);
            COMPARE_OCTET_STRING(&aval->outer->inner, &bval->outer->inner, s7);
    }
    return 0;
}

static int
test_seqof5(void)
{
    struct test_case tests[] = {
	{ NULL,  2, "\x30\x00", "seq5 0" },
	{ NULL,  126,
          "\x30\x7c"                                            /* SEQ */
            "\x30\x7a"                                          /* SEQ */
              "\x30\x78"                                        /* SEQ */
                "\x02\x01\x01"                                  /* INT 1 */
                "\x04\x06\x01\x01\x01\x01\x01\x01"              /* "\0x1"x6 */
                "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xfe"  /* INT ~1 */
                "\x04\x06\x02\x02\x02\x02\x02\x02"              /* "\x02"x6 */
                "\x02\x01\x02"                                  /* INT 2 */
                "\x04\x06\x03\x03\x03\x03\x03\x03"              /* "\x03"x6 */
                "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xfd"  /* INT ~2 */
                "\x04\x06\x04\x04\x04\x04\x04\x04"              /* ... */
                "\x02\x01\x03"
                "\x04\x06\x05\x05\x05\x05\x05\x05"
                "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xfc"
                "\x04\x06\x06\x06\x06\x06\x06\x06"
                "\x02\x01\x04"
                "\x04\x06\x07\x07\x07\x07\x07\x07"
                "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xfb"
                "\x04\x06\x08\x08\x08\x08\x08\x08",
          "seq5 1" },
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTSeqOf5 c[2];
    struct TESTSeqOf5_outer outer;
    struct TESTSeqOf5_outer_inner inner;
    TESTuint64 u[8];
    heim_octet_string s[8];
    int i;

    c[0].outer = NULL;
    tests[0].val = &c[0];

    for (i = 0; i < 8; ++i) {
        u[i] = (i&1) == 0 ? i/2+1 : ~(i/2+1);
        s[i].data = memset(malloc(s[i].length = 6), i+1, 6);
    }

    inner.u0 = u[0]; inner.u1 = u[1]; inner.u2 = u[2]; inner.u3 = u[3];
    inner.u4 = u[4]; inner.u5 = u[5]; inner.u6 = u[6]; inner.u7 = u[7];
    inner.s0 = s[0]; inner.s1 = s[1]; inner.s2 = s[2]; inner.s3 = s[3];
    inner.s4 = s[4]; inner.s5 = s[5]; inner.s6 = s[6]; inner.s7 = s[7];

    outer.inner = inner;
    c[1].outer = &outer;
    tests[1].val = &c[1];

    ret += generic_test (tests, ntests, sizeof(TESTSeqOf5),
			 (generic_encode)encode_TESTSeqOf5,
			 (generic_length)length_TESTSeqOf5,
			 (generic_decode)decode_TESTSeqOf5,
			 (generic_free)free_TESTSeqOf5,
			 cmp_test_seqof5,
			 NULL);

    for (i = 0; i < 8; ++i)
        free(s[i].data);

    return ret;
}

int
main(int argc, char **argv)
{
    int ret = 0;

    ret += test_principal ();
    ret += test_authenticator();
    ret += test_krb_error();
    ret += test_Name();
    ret += test_bit_string();
    ret += test_bit_string_rfc1510();
    ret += test_time();
    ret += test_cert();

    ret += check_tag_length();
    ret += check_tag_length64();
    ret += check_tag_length64s();
    ret += test_large_tag();
    ret += test_choice();

    ret += test_implicit();

    ret += test_taglessalloc();
    ret += test_optional();

    ret += check_fail_largetag();
    ret += check_fail_sequence();
    ret += check_fail_choice();
    ret += check_fail_Ticket();

    ret += check_seq();
    ret += check_seq_of_size();
    ret += test_SignedData();

    ret += check_TESTMechTypeList();
    ret += test_seq4();
    ret += test_seqof5();

    return ret;
}
