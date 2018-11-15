/*	$NetBSD: test_alname.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

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
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "krb5_locl.h"
#include <krb5/getarg.h>
#include <err.h>

char localname[1024];
static size_t lname_size = sizeof (localname);
static int lname_size_arg = 0;
static int simple_flag = 0;
static int verbose_flag = 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"lname-size",	0,	arg_integer,	&lname_size_arg,
     "set localname size (0 means use default, must be 0..1023)", "integer" },
    {"simple",	0,	arg_flag,	&simple_flag, /* Used for scripting */
     "map the given principal and print the resulting localname", NULL },
    {"verbose",	0,	arg_flag,	&verbose_flag,
     "print the actual principal name as well as the localname", NULL },
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
test_alname(krb5_context context, krb5_const_realm realm,
	    const char *user, const char *inst,
	    const char *localuser, int ok)
{
    krb5_principal p;
    krb5_error_code ret;
    char *princ;

    ret = krb5_make_principal(context, &p, realm, user, inst, NULL);
    if (ret)
	krb5_err(context, 1, ret, "krb5_build_principal");

    ret = krb5_unparse_name(context, p, &princ);
    if (ret)
	krb5_err(context, 1, ret, "krb5_unparse_name");

    ret = krb5_aname_to_localname(context, p, lname_size, localname);
    krb5_free_principal(context, p);
    if (ret) {
	if (!ok) {
	    free(princ);
	    return;
	}
	krb5_err(context, 1, ret, "krb5_aname_to_localname: %s -> %s",
		 princ, localuser);
	free(princ);
    }

    if (strcmp(localname, localuser) != 0) {
	if (ok)
	    errx(1, "compared failed %s != %s (should have succeded)",
		 localname, localuser);
    } else {
	if (!ok)
	    errx(1, "compared failed %s == %s (should have failed)",
		 localname, localuser);
    }

}

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_realm realm;
    int optidx = 0;
    char *user;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    if (simple_flag) {
	krb5_principal princ;
	char *unparsed;
	int status = 0;

	/* Map then print the result and exit */
	if (argc != 1)
	    errx(1, "One argument is required and it must be a principal name");

	ret = krb5_parse_name(context, argv[0], &princ);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_build_principal");

	ret = krb5_unparse_name(context, princ, &unparsed);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_unparse_name");

	if (lname_size_arg > 0 && lname_size_arg < 1024)
	    lname_size = lname_size_arg;
	else if (lname_size_arg != 0)
	    errx(1, "local name size must be between 0 and 1023 (inclusive)");

	ret = krb5_aname_to_localname(context, princ, lname_size, localname);
	if (ret == KRB5_NO_LOCALNAME) {
	    if (verbose_flag)
		fprintf(stderr, "No mapping obtained for %s\n", unparsed);
	    exit(1);
	}
	switch (ret) {
	case KRB5_PLUGIN_NO_HANDLE:
	    fprintf(stderr, "Error: KRB5_PLUGIN_NO_HANDLE leaked!\n");
	    status = 2;
	    break;
	case KRB5_CONFIG_NOTENUFSPACE:
	    fprintf(stderr, "Error: lname-size (%lu) too small\n",
		    (long unsigned)lname_size);
	    status = 3;
	    break;
	case 0:
	    if (verbose_flag)
		printf("%s ", unparsed);
	    printf("%s\n", localname);
	    break;
	default:
	    krb5_err(context, 4, ret, "krb5_aname_to_localname");
	    break;
	}
	free(unparsed);
	krb5_free_principal(context, princ);
	krb5_free_context(context);
	exit(status);
    }

    if (argc != 1)
	errx(1, "first argument should be a local user that is in root .k5login");

    user = argv[0];

    ret = krb5_get_default_realm(context, &realm);
    if (ret)
	krb5_err(context, 1, ret, "krb5_get_default_realm");

    test_alname(context, realm, user, NULL, user, 1);
    test_alname(context, realm, user, "root", "root", 1);

    test_alname(context, "FOO.BAR.BAZ.KAKA", user, NULL, user, 0);
    test_alname(context, "FOO.BAR.BAZ.KAKA", user, "root", "root", 0);

    test_alname(context, realm, user, NULL,
		"not-same-as-user", 0);
    test_alname(context, realm, user, "root",
		"not-same-as-user", 0);

    test_alname(context, "FOO.BAR.BAZ.KAKA", user, NULL,
		"not-same-as-user", 0);
    test_alname(context, "FOO.BAR.BAZ.KAKA", user, "root",
		"not-same-as-user", 0);

    krb5_free_context(context);

    return 0;
}
