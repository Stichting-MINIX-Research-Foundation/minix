
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: eisabusprint.c,v 1.5 2006/11/16 01:32:50 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/eisa/eisavar.h>

int
eisabusprint(void *vea, const char *pnp)
{
	if (pnp)
		aprint_normal("eisa at %s", pnp);
	return (UNCONF);
}
