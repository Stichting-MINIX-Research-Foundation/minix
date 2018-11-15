/*	$NetBSD: intr.c,v 1.2 2017/01/28 21:31:51 christos Exp $	*/

#include <config.h>

#include <krb5/getarg.h>
#include <krb5/roken.h>
#include <time.h>

static int help_flag;
static int timeout = 3;

static struct getargs args[] = {
    { "help", 'h', arg_flag, &help_flag, NULL, NULL },
    { "timeout", 't', arg_integer, &timeout, NULL, NULL }
};

static int nargs = sizeof(args) / sizeof(args[0]);

static time_t
handle_timeout(void *data)
{
    static int killed;

    if (!killed++)
        return -1;  /* kill it */
    return -2;      /* stop waiting for it */
}

static void
usage(int status)
{
    arg_printusage(args, nargs, NULL, "command");
    exit(status);
}


int
main(int argc, char **argv)
{
    int optidx = 0;

    setprogname(argv[0]);

    if (getarg(args, nargs, argc, argv, &optidx))
        usage(1);

    if (help_flag)
        usage(0);

    argc -= optidx;
    argv += optidx;

    if (argc == 0)
        usage(1);

    return simple_execvp_timed(argv[0], argv, handle_timeout, NULL,
                               timeout);
}
