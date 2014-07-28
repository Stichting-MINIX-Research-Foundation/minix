/*	sysenv 1.0 - request system boot parameter	Author: Kees J. Bot
 *								23 Dec 2000
 */
#define nil ((void*)0)
#include <minix/type.h>
#include <sys/types.h>
#include <sys/svrctl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define NIL ((char*)0)

static void tell(int fd, ...)
{
    va_list ap;
    char *s;

    va_start(ap, fd);
    while ((s= va_arg(ap, char *)) != NIL) {
	(void) write(fd, s, strlen(s));
    }
    va_end(ap);
}

int main(int argc, char **argv)
{
    struct sysgetenv sysgetenv;
    int i;
    int ex= 0;
    char *e;
    char val[1024];

    i= 1;
    while (i < argc && argv[i][0] == '-') {
	char *opt= argv[i++]+1;

	if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

	if (*opt != 0) {
	    tell(2, "Usage: sysenv [name ...]\n", NIL);
	    exit(1);
	}
    }

    do {
	if (i < argc) {
	    sysgetenv.key= argv[i];
	    sysgetenv.keylen= strlen(sysgetenv.key) + 1;
	} else {
	    sysgetenv.key= nil;
	    sysgetenv.keylen= 0;
	}
	sysgetenv.val= val;
	sysgetenv.vallen= sizeof(val);

	if (svrctl(PMGETPARAM, &sysgetenv) == -1) {
	    if (errno == ESRCH) {
		ex |= 2;
	    } else {
		ex |= 1;
		tell(2, "sysenv: ", strerror(errno), "\n", NIL);
	    }
	    continue;
	}

	e= sysgetenv.val;
	do {
	    e += strlen(e);
	    *e++ = '\n';
	} while (i == argc && *e != 0);

	if (write(1, sysgetenv.val, e - sysgetenv.val) < 0) {
	    ex |= 1;
	    tell(2, "sysenv: ", strerror(errno), "\n", NIL);
	}
    } while (++i < argc);
    return ex;
}
