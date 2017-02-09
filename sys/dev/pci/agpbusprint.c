
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: agpbusprint.c,v 1.5 2006/11/16 01:33:08 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/agpvar.h>

int
agpbusprint(void *vaa, const char *pnp)
{
#if 0
	struct agpbus_attach_args *aa = vaa;
#endif
	if (pnp)
		aprint_normal("agp at %s", pnp);
	return (UNCONF);
}
