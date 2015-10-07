/*	$NetBSD: test_sequence.c,v 1.1.1.1 2011/04/13 18:14:46 elric Exp $	*/

/*
 * Copyright (c) 2003 Kungliga Tekniska Högskolan
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

#include "gsskrb5_locl.h"

/* correct ordering */
OM_uint32 pattern1[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
};

/* gap 10 */
OM_uint32 pattern2[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13
};

/* dup 9 */
OM_uint32 pattern3[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10, 11, 12, 13
};

/* gaps */
OM_uint32 pattern4[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 13, 14, 15, 16, 18, 100
};

/* 11 before 10 */
OM_uint32 pattern5[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
};

/* long */
OM_uint32 pattern6[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59
};

/* dont start at 0 */
OM_uint32 pattern7[] = {
    11, 12, 13
};

/* wrap around */
OM_uint32 pattern8[] = {
    4294967293U, 4294967294U, 4294967295U, 0, 1, 2
};

static int
test_seq(int t, OM_uint32 flags, OM_uint32 start_seq,
	 OM_uint32 *pattern, int pattern_len, OM_uint32 expected_error)
{
    struct gss_msg_order *o;
    OM_uint32 maj_stat, min_stat;
    krb5_storage *sp;
    int i;

    maj_stat = _gssapi_msg_order_create(&min_stat, &o, flags,
					start_seq, 20, 0);
    if (maj_stat)
	errx(1, "create: %d %d", maj_stat, min_stat);

    sp = krb5_storage_emem();
    if (sp == NULL)
	errx(1, "krb5_storage_from_emem");

    _gssapi_msg_order_export(sp, o);

    for (i = 0; i < pattern_len; i++) {
	maj_stat = _gssapi_msg_order_check(o, pattern[i]);
	if (maj_stat)
	    break;
    }
    if (maj_stat != expected_error) {
	printf("test pattern %d failed with %d (should have been %d)\n",
	       t, maj_stat, expected_error);
	krb5_storage_free(sp);
	_gssapi_msg_order_destroy(&o);
	return 1;
    }


    _gssapi_msg_order_destroy(&o);

    /* try again, now with export/imported blob */
    krb5_storage_seek(sp, 0, SEEK_SET);

    maj_stat = _gssapi_msg_order_import(&min_stat, sp, &o);
    if (maj_stat)
	errx(1, "import: %d %d", maj_stat, min_stat);

    for (i = 0; i < pattern_len; i++) {
	maj_stat = _gssapi_msg_order_check(o, pattern[i]);
	if (maj_stat)
	    break;
    }
    if (maj_stat != expected_error) {
	printf("import/export test pattern %d failed "
	       "with %d (should have been %d)\n",
	       t, maj_stat, expected_error);
	_gssapi_msg_order_destroy(&o);
	krb5_storage_free(sp);
	return 1;
    }

    _gssapi_msg_order_destroy(&o);
    krb5_storage_free(sp);

    return 0;
}

struct {
    OM_uint32 flags;
    OM_uint32 *pattern;
    int pattern_len;
    OM_uint32 error_code;
    OM_uint32 start_seq;
} pl[] = {
    {
	GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG,
	pattern1,
	sizeof(pattern1)/sizeof(pattern1[0]),
	0
    },
    {
	GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG,
	pattern2,
	sizeof(pattern2)/sizeof(pattern2[0]),
	GSS_S_GAP_TOKEN
    },
    {
	GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG,
	pattern3,
	sizeof(pattern3)/sizeof(pattern3[0]),
	GSS_S_DUPLICATE_TOKEN
    },
    {
	GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG,
	pattern4,
	sizeof(pattern4)/sizeof(pattern4[0]),
	GSS_S_GAP_TOKEN
    },
    {
	GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG,
	pattern5,
	sizeof(pattern5)/sizeof(pattern5[0]),
	GSS_S_GAP_TOKEN
    },
    {
	GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG,
	pattern6,
	sizeof(pattern6)/sizeof(pattern6[0]),
	GSS_S_COMPLETE
    },
    {
	GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG,
	pattern7,
	sizeof(pattern7)/sizeof(pattern7[0]),
	GSS_S_GAP_TOKEN
    },
    {
	GSS_C_REPLAY_FLAG|GSS_C_SEQUENCE_FLAG,
	pattern8,
	sizeof(pattern8)/sizeof(pattern8[0]),
	GSS_S_COMPLETE,
	4294967293U
    },
    {
	0,
	pattern1,
	sizeof(pattern1)/sizeof(pattern1[0]),
	GSS_S_COMPLETE
    },
    {
	0,
	pattern2,
	sizeof(pattern2)/sizeof(pattern2[0]),
	GSS_S_COMPLETE
    },
    {
	0,
	pattern3,
	sizeof(pattern3)/sizeof(pattern3[0]),
	GSS_S_COMPLETE
    },
    {
	0,
	pattern4,
	sizeof(pattern4)/sizeof(pattern4[0]),
	GSS_S_COMPLETE
    },
    {
	0,
	pattern5,
	sizeof(pattern5)/sizeof(pattern5[0]),
	GSS_S_COMPLETE
    },
    {
	0,
	pattern6,
	sizeof(pattern6)/sizeof(pattern6[0]),
	GSS_S_COMPLETE
    },
    {
	0,
	pattern7,
	sizeof(pattern7)/sizeof(pattern7[0]),
	GSS_S_COMPLETE
    },
    {
	0,
	pattern8,
	sizeof(pattern8)/sizeof(pattern8[0]),
	GSS_S_COMPLETE,
	4294967293U

    },
    {
	GSS_C_REPLAY_FLAG,
	pattern1,
	sizeof(pattern1)/sizeof(pattern1[0]),
	GSS_S_COMPLETE
    },
    {
	GSS_C_REPLAY_FLAG,
	pattern2,
	sizeof(pattern2)/sizeof(pattern2[0]),
	GSS_S_COMPLETE
    },
    {
	GSS_C_REPLAY_FLAG,
	pattern3,
	sizeof(pattern3)/sizeof(pattern3[0]),
	GSS_S_DUPLICATE_TOKEN
    },
    {
	GSS_C_REPLAY_FLAG,
	pattern4,
	sizeof(pattern4)/sizeof(pattern4[0]),
	GSS_S_COMPLETE
    },
    {
	GSS_C_REPLAY_FLAG,
	pattern5,
	sizeof(pattern5)/sizeof(pattern5[0]),
	0
    },
    {
	GSS_C_REPLAY_FLAG,
	pattern6,
	sizeof(pattern6)/sizeof(pattern6[0]),
	GSS_S_COMPLETE
    },
    {
	GSS_C_REPLAY_FLAG,
	pattern7,
	sizeof(pattern7)/sizeof(pattern7[0]),
	GSS_S_COMPLETE
    },
    {
	GSS_C_SEQUENCE_FLAG,
	pattern8,
	sizeof(pattern8)/sizeof(pattern8[0]),
	GSS_S_COMPLETE,
	4294967293U
    },
    {
	GSS_C_SEQUENCE_FLAG,
	pattern1,
	sizeof(pattern1)/sizeof(pattern1[0]),
	0
    },
    {
	GSS_C_SEQUENCE_FLAG,
	pattern2,
	sizeof(pattern2)/sizeof(pattern2[0]),
	GSS_S_GAP_TOKEN
    },
    {
	GSS_C_SEQUENCE_FLAG,
	pattern3,
	sizeof(pattern3)/sizeof(pattern3[0]),
	GSS_S_DUPLICATE_TOKEN
    },
    {
	GSS_C_SEQUENCE_FLAG,
	pattern4,
	sizeof(pattern4)/sizeof(pattern4[0]),
	GSS_S_GAP_TOKEN
    },
    {
	GSS_C_SEQUENCE_FLAG,
	pattern5,
	sizeof(pattern5)/sizeof(pattern5[0]),
	GSS_S_GAP_TOKEN
    },
    {
	GSS_C_SEQUENCE_FLAG,
	pattern6,
	sizeof(pattern6)/sizeof(pattern6[0]),
	GSS_S_COMPLETE
    },
    {
	GSS_C_SEQUENCE_FLAG,
	pattern7,
	sizeof(pattern7)/sizeof(pattern7[0]),
	GSS_S_GAP_TOKEN
    },
    {
	GSS_C_REPLAY_FLAG,
	pattern8,
	sizeof(pattern8)/sizeof(pattern8[0]),
	GSS_S_COMPLETE,
	4294967293U
    }
};

int
main(int argc, char **argv)
{
    int i, failed = 0;

    for (i = 0; i < sizeof(pl)/sizeof(pl[0]); i++) {
	if (test_seq(i,
		     pl[i].flags,
		     pl[i].start_seq,
		     pl[i].pattern,
		     pl[i].pattern_len,
		     pl[i].error_code))
	    failed++;
    }
    if (failed)
	printf("FAILED %d tests\n", failed);
    return failed != 0;
}
