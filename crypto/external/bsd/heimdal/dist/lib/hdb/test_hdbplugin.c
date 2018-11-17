/*	$NetBSD: test_hdbplugin.c,v 1.2 2017/01/28 21:31:48 christos Exp $	*/

/*
 * Copyright (c) 2013 Jeffrey Clark
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

#include "hdb_locl.h"

struct hdb_called {
    int create;
    int init;
    int fini;
};
struct hdb_called testresult;

static krb5_error_code
hdb_test_create(krb5_context context, struct HDB **db, const char *arg)
{
	testresult.create = 1;
	return 0;
}

static krb5_error_code
hdb_test_init(krb5_context context, void **ctx)
{
	*ctx = NULL;
	testresult.init = 1;
	return 0;
}

static void hdb_test_fini(void *ctx)
{
	testresult.fini = 1;
}

struct hdb_method hdb_test =
{
#ifdef WIN32
	/* Not c99 */
	HDB_INTERFACE_VERSION,
	hdb_test_init,
	hdb_test_fini,
	"test",
	hdb_test_create
#else
	.version = HDB_INTERFACE_VERSION,
	.init = hdb_test_init,
	.fini = hdb_test_fini,
	.prefix = "test",
	.create = hdb_test_create
#endif
};

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    HDB *db;

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_contex");

    ret = krb5_plugin_register(context,
		    PLUGIN_TYPE_DATA, "hdb_test_interface",
		    &hdb_test);
    if(ret) {
	    krb5_err(context, 1, ret, "krb5_plugin_register");
    }

    ret = hdb_create(context, &db, "test:test&1234");
    if(ret) {
	    krb5_err(context, 1, ret, "hdb_create");
    }

    krb5_free_context(context);
    return 0;
}
