/*	$NetBSD: kcpytkt.c,v 1.1.1.2 2014/04/24 12:45:28 pettai Exp $	*/


#include "kuser_locl.h"

static char *etypestr = 0;
static char *fromccachestr = 0;
static char *flagstr = 0;
static int  quiet_flag = 0;
static int  version_flag = 0;
static int  help_flag = 0;

struct getargs args[] = {
    { "cache",  'c', arg_string, &fromccachestr,
      "Credentials cache", "cachename" },
    { "enctype", 'e', arg_string, &etypestr,
      "Encryption type", "enctype" },
    { "flags", 'f', arg_string, &flagstr,
      "Flags", "flags" },
    { "quiet", 'q', arg_flag, &quiet_flag, "Quiet" },
    { "version",        0, arg_flag, &version_flag },
    { "help",           0, arg_flag, &help_flag }
};

static void
usage(int ret)
{
    arg_printusage(args, sizeof(args)/sizeof(args[0]),
                   "Usage: ", "dest_ccache service1 [service2 ...]");
    exit (ret);
}

static void do_kcpytkt (int argc, char *argv[], char *fromccachestr, char *etypestr, int flags);

int main(int argc, char *argv[])
{
    int optidx;
    int flags = 0;

    setprogname(argv[0]);

    if (getarg(args, sizeof(args)/sizeof(args[0]), argc, argv, &optidx))
        usage(1);

    if (help_flag)
        usage(0);

    if (version_flag) {
        print_version(NULL);
        exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc < 2)
        usage(1);

    if (flagstr)
        flags = atoi(flagstr);

    do_kcpytkt(argc, argv, fromccachestr, etypestr, flags);

    return 0;
}

static void do_kcpytkt (int count, char *names[],
                        char *fromccachestr, char *etypestr, int flags)
{
    krb5_context context;
    krb5_error_code ret;
    int i, errors;
    krb5_enctype etype;
    krb5_ccache fromccache;
    krb5_ccache destccache;
    krb5_principal me;
    krb5_creds in_creds, out_creds;
    int retflags;
    char *princ;

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context failed: %d", ret);

    if (etypestr) {
        ret = krb5_string_to_enctype(context, etypestr, &etype);
	if (ret)
	    krb5_err(context, 1, ret, "Can't convert enctype %s", etypestr);
        retflags = KRB5_TC_MATCH_SRV_NAMEONLY | KRB5_TC_MATCH_KEYTYPE;
    } else {
	etype = 0;
        retflags = KRB5_TC_MATCH_SRV_NAMEONLY;
    }

    if (fromccachestr)
        ret = krb5_cc_resolve(context, fromccachestr, &fromccache);
    else
        ret = krb5_cc_default(context, &fromccache);
    if (ret)
        krb5_err(context, 1, ret, "Can't resolve credentials cache");

    ret = krb5_cc_get_principal(context, fromccache, &me);
    if (ret)
        krb5_err(context, 1, ret, "Can't query client principal name");

    ret = krb5_cc_resolve(context, names[0], &destccache);
    if (ret)
        krb5_err(context, 1, ret, "Can't resolve destination cache");

    errors = 0;

    for (i = 1; i < count; i++) {
	memset(&in_creds, 0, sizeof(in_creds));

	in_creds.client = me;

	ret = krb5_parse_name(context, names[i], &in_creds.server);
	if (ret) {
	    if (!quiet_flag)
                krb5_warn(context, ret, "Parse error for %s", names[i]);
	    errors++;
	    continue;
	}

	ret = krb5_unparse_name(context, in_creds.server, &princ);
	if (ret) {
            krb5_warn(context, ret, "Unparse error for %s", names[i]);
	    errors++;
	    continue;
	}

	in_creds.session.keytype = etype;

        ret = krb5_cc_retrieve_cred(context, fromccache, retflags,
                                    &in_creds, &out_creds);
	if (ret) {
            krb5_warn(context, ret, "Can't retrieve credentials for %s", princ);

	    krb5_free_unparsed_name(context, princ);

	    errors++;
	    continue;
	}

	ret = krb5_cc_store_cred(context, destccache, &out_creds);

	krb5_free_principal(context, in_creds.server);

	if (ret) {
            krb5_warn(context, ret, "Can't store credentials for %s", princ);

            krb5_free_cred_contents(context, &out_creds);
	    krb5_free_unparsed_name(context, princ);

	    errors++;
	    continue;
	}

	krb5_free_unparsed_name(context, princ);
        krb5_free_cred_contents(context, &out_creds);
    }

    krb5_free_principal(context, me);
    krb5_cc_close(context, fromccache);
    krb5_cc_close(context, destccache);
    krb5_free_context(context);

    if (errors)
	exit(1);

    exit(0);
}
