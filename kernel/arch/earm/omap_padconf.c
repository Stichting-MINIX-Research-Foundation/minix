/* Implements sys_padconf() for the AM335X and DM37XX. */

#include "kernel/kernel.h"
#include "arch_proto.h"
#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/mmio.h>
#include <minix/padconf.h>
#include <assert.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>

#include "omap_padconf.h"

struct omap_padconf
{
	vir_bytes base;
	vir_bytes offset;
	vir_bytes size;
};

static struct omap_padconf omap_padconf = {
	.base = PADCONF_REGISTERS_BASE,
	.offset = PADCONF_REGISTERS_OFFSET,
	.size = PADCONF_REGISTERS_SIZE
};

static kern_phys_map padconf_phys_map;

int
arch_padconf_set(u32_t padconf, u32_t mask, u32_t value)
{
	/* check that the value will be inside the padconf memory range */
	if (padconf >= (PADCONF_REGISTERS_SIZE - PADCONF_REGISTERS_OFFSET)) {
		return EINVAL;	/* outside of valid range */
	}

	set32(padconf + omap_padconf.base + omap_padconf.offset, mask, value);

	return OK;
}

void
arch_padconf_init(void)
{
	kern_phys_map_ptr(omap_padconf.base, omap_padconf.size,
	     &padconf_phys_map, (vir_bytes) &omap_padconf.base);

	return;
}
