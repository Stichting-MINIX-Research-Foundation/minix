/*	$NetBSD: test_mkey.c,v 1.2 2017/01/28 21:31:48 christos Exp $	*/


#include "hdb_locl.h"
#include <krb5/getarg.h>
#include <krb5/base64.h>

static char *mkey_file;
static int help_flag;
static int version_flag;

struct getargs args[] = {
    { "mkey-file",	0,      arg_string, &mkey_file,    NULL, NULL },
    { "help",		'h',	arg_flag,   &help_flag,    NULL, NULL },
    { "version",	0,	arg_flag,   &version_flag, NULL, NULL }
};

static int num_args = sizeof(args) / sizeof(args[0]);

int
main(int argc, char **argv)
{
    krb5_context context;
    int ret, o = 0;

    setprogname(argv[0]);

    if(getarg(args, num_args, argc, argv, &o))
	krb5_std_usage(1, args, num_args);

    if(help_flag)
	krb5_std_usage(0, args, num_args);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context failed: %d", ret);

    if (mkey_file) {
        hdb_master_key mkey;

	ret = hdb_read_master_key(context, mkey_file, &mkey);
	if (ret)
	    krb5_err(context, 1, ret, "failed to read master key %s", mkey_file);

	hdb_free_master_key(context, mkey);
    } else
      krb5_errx(context, 1, "no command option given");

    krb5_free_context(context);

    return 0;
}
