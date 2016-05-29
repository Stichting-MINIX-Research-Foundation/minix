/* Implements sys_padconf() for the Raspberry Pi. */

#include "kernel/kernel.h"
#include "arch_proto.h"
#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/mmio.h>
#include <minix/padconf.h>
#include <minix/board.h>
#include <minix/com.h>
#include <assert.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>

#include "bsp_padconf.h"

struct rpi2_padconf
{
	vir_bytes base;
	vir_bytes offset;
	vir_bytes size;
};

static struct rpi2_padconf rpi2_padconf;

static kern_phys_map padconf_phys_map;

int
bsp_padconf_set(u32_t padconf, u32_t mask, u32_t value)
{
	/* check that the value will be inside the padconf memory range */
	if (padconf >= (rpi2_padconf.size - rpi2_padconf.offset)) {
		return EINVAL;	/* outside of valid range */
	}

	set32(padconf + rpi2_padconf.base + rpi2_padconf.offset, mask,
	    value);

	return OK;
}

void
bsp_padconf_init(void)
{
	if (BOARD_IS_RPI_2_B(machine.board_id) ||
	    BOARD_IS_RPI_3_B(machine.board_id)) {
		rpi2_padconf.base = PADCONF_RPI2_REGISTERS_BASE;
		rpi2_padconf.offset = PADCONF_RPI2_REGISTERS_OFFSET;
		rpi2_padconf.size = PADCONF_RPI2_REGISTERS_SIZE;
	}

	kern_phys_map_ptr(rpi2_padconf.base, rpi2_padconf.size,
	    VMMF_UNCACHED | VMMF_WRITE,
	    &padconf_phys_map, (vir_bytes) & rpi2_padconf.base);

	return;
}
