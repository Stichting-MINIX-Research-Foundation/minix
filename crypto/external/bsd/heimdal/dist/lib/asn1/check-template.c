/*	$NetBSD: check-template.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

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
#include <test_asn1.h>

#include "check-common.h"
#include "der_locl.h"

static int
cmp_dummy (void *a, void *b)
{
    return 0;
}

static int
test_uint64(void)
{
    struct test_case tests[] = {
        { NULL, 3, "\x02\x01\x00", "uint64 0" },
        { NULL, 7, "\x02\x05\x01\xff\xff\xff\xff", "uint64 1" },
        { NULL, 7, "\x02\x05\x02\x00\x00\x00\x00", "uint64 2" },
        { NULL, 9, "\x02\x07\x7f\xff\xff\xff\xff\xff\xff", "uint64 3" },
        { NULL, 10, "\x02\x08\x00\x80\x00\x00\x00\x00\x00\x00", "uint64 4" },
        { NULL, 10, "\x02\x08\x7f\xff\xff\xff\xff\xff\xff\xff", "uint64 5" },
        { NULL, 11, "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xff", "uint64 6" }
    };

    size_t i;
    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTuint64 values[] = { 0, 8589934591LL, 8589934592LL,
                           36028797018963967LL, 36028797018963968LL,
                           9223372036854775807LL, 18446744073709551615ULL };

    for (i = 0; i < ntests; i++)
       tests[i].val = &values[i];

    if (sizeof(TESTuint64) != sizeof(uint64_t)) {
       ret += 1;
       printf("sizeof(TESTuint64) %d != sizeof(uint64_t) %d\n",
              (int)sizeof(TESTuint64), (int)sizeof(uint64_t));
    }

    ret += generic_test (tests, ntests, sizeof(TESTuint64),
                        (generic_encode)encode_TESTuint64,
                        (generic_length)length_TESTuint64,
                        (generic_decode)decode_TESTuint64,
                        (generic_free)free_TESTuint64,
                        cmp_dummy,
                        NULL);
    return ret;
}

static int
test_seqofseq(void)
{
    struct test_case tests[] = {
	{ NULL,  2,
	  "\x30\x00",
	  "seqofseq 0" },
	{ NULL,  9,
	  "\x30\x07\x30\x05\xa0\x03\x02\x01\x00",
	  "seqofseq 1" },
	{ NULL,  16,
	  "\x30\x0e\x30\x05\xa0\x03\x02\x01\x00\x30\x05\xa0\x03\x02\x01\x01",
	  "seqofseq 2" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTSeqOfSeq c0, c1, c2;
    struct TESTSeqOfSeq_val i[2];

    i[0].zero = 0;
    i[1].zero = 1;

    c0.len = 0;
    c0.val = NULL;
    tests[0].val = &c0;

    c1.len = 1;
    c1.val = i;
    tests[1].val = &c1;

    c2.len = 2;
    c2.val = i;
    tests[2].val = &c2;

    ret += generic_test (tests, ntests, sizeof(TESTSeqOfSeq),
			 (generic_encode)encode_TESTSeqOfSeq,
			 (generic_length)length_TESTSeqOfSeq,
			 (generic_decode)decode_TESTSeqOfSeq,
			 (generic_free)free_TESTSeqOfSeq,
			 cmp_dummy,
			 NULL);
    return ret;
}

static int
test_seqofseq2(void)
{
    struct test_case tests[] = {
	{ NULL,  2,
	  "\x30\x00",
	  "seqofseq2 0" },
	{ NULL,  11,
	  "\x30\x09\x30\x07\xa0\x05\x1b\x03\x65\x74\x74",
	  "seqofseq2 1" },
	{ NULL,  21,
	  "\x30\x13\x30\x07\xa0\x05\x1b\x03\x65\x74\x74\x30\x08\xa0"
	  "\x06\x1b\x04\x74\x76\x61\x61",
	  "seqofseq2 2" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTSeqOfSeq2 c0, c1, c2;
    struct TESTSeqOfSeq2_val i[2];

    i[0].string = "ett";
    i[1].string = "tvaa";

    c0.len = 0;
    c0.val = NULL;
    tests[0].val = &c0;

    c1.len = 1;
    c1.val = i;
    tests[1].val = &c1;

    c2.len = 2;
    c2.val = i;
    tests[2].val = &c2;

    ret += generic_test (tests, ntests, sizeof(TESTSeqOfSeq2),
			 (generic_encode)encode_TESTSeqOfSeq2,
			 (generic_length)length_TESTSeqOfSeq2,
			 (generic_decode)decode_TESTSeqOfSeq2,
			 (generic_free)free_TESTSeqOfSeq2,
			 cmp_dummy,
			 NULL);
    return ret;
}

static int
test_seqof2(void)
{
    struct test_case tests[] = {
	{ NULL,  4,
	  "\x30\x02\x30\x00",
	  "seqof2 1" },
	{ NULL,  9,
	  "\x30\x07\x30\x05\x1b\x03\x66\x6f\x6f",
	  "seqof2 2" },
	{ NULL,  14,
	  "\x30\x0c\x30\x0a\x1b\x03\x66\x6f\x6f\x1b\x03\x62\x61\x72",
	  "seqof2 3" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTSeqOf2 c0, c1, c2;
    heim_general_string i[2];

    i[0] = "foo";
    i[1] = "bar";

    c0.strings.val = NULL;
    c0.strings.len = 0;
    tests[0].val = &c0;

    c1.strings.len = 1;
    c1.strings.val = i;
    tests[1].val = &c1;

    c2.strings.len = 2;
    c2.strings.val = i;
    tests[2].val = &c2;

    ret += generic_test (tests, ntests, sizeof(TESTSeqOf2),
			 (generic_encode)encode_TESTSeqOf2,
			 (generic_length)length_TESTSeqOf2,
			 (generic_decode)decode_TESTSeqOf2,
			 (generic_free)free_TESTSeqOf2,
			 cmp_dummy,
			 NULL);
    return ret;
}

static int
test_seqof3(void)
{
    struct test_case tests[] = {
	{ NULL,  2,
	  "\x30\x00",
	  "seqof3 0" },
	{ NULL,  4,
	  "\x30\x02\x30\x00",
	  "seqof3 1" },
	{ NULL,  9,
	  "\x30\x07\x30\x05\x1b\x03\x66\x6f\x6f",
	  "seqof3 2" },
	{ NULL,  14,
	  "\x30\x0c\x30\x0a\x1b\x03\x66\x6f\x6f\x1b\x03\x62\x61\x72",
	  "seqof3 3" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTSeqOf3 c0, c1, c2, c3;
    struct TESTSeqOf3_strings s1, s2, s3;
    heim_general_string i[2];

    i[0] = "foo";
    i[1] = "bar";

    c0.strings = NULL;
    tests[0].val = &c0;

    s1.val = NULL;
    s1.len = 0;
    c1.strings = &s1;
    tests[1].val = &c1;

    s2.len = 1;
    s2.val = i;
    c2.strings = &s2;
    tests[2].val = &c2;

    s3.len = 2;
    s3.val = i;
    c3.strings = &s3;
    tests[3].val = &c3;

    ret += generic_test (tests, ntests, sizeof(TESTSeqOf3),
			 (generic_encode)encode_TESTSeqOf3,
			 (generic_length)length_TESTSeqOf3,
			 (generic_decode)decode_TESTSeqOf3,
			 (generic_free)free_TESTSeqOf3,
			 cmp_dummy,
			 NULL);
    return ret;
}


static int
test_seqof4(void)
{
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

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
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
			 cmp_dummy,
			 NULL);
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

    ret += test_uint64();
    ret += test_seqofseq();
    ret += test_seqofseq2();
    ret += test_seqof2();
    ret += test_seqof3();
    ret += test_seqof4();
    ret += test_seqof5();

    return ret;
}
