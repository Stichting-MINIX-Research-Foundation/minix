/* kernel headers */
#include <minix/syslib.h>
#include <minix/drvlib.h>
#include <minix/log.h>
#include <minix/padconf.h>
#include <minix/mmio.h>

/* system headers */
#include <sys/mman.h>
#include <sys/types.h>

/* usr headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/* local headers */

/* used for logging */
static struct log log = {
	.name = "omap_padconf",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static u32_t base = 0;
static u32_t use_count = 0;

int
padconf_init()
{
	struct minix_mem_range mr;

	use_count++;

	if (base != 0) {
		/* when used in a library we can't guaranty we only call this
		 * method once */
		log_trace(&log, "Called %d times\n", use_count);
		return OK;
	}
	mr.mr_base = PADCONF_REGISTERS_BASE;
	mr.mr_limit = PADCONF_REGISTERS_BASE + 0x1000;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		log_warn(&log, "Unable to request permission to map memory\n");
		return EPERM;
	}

	base =
	    (uint32_t) vm_map_phys(SELF, (void *) PADCONF_REGISTERS_BASE,
	    0x1000);

	if (base == (uint32_t) MAP_FAILED) {
		log_warn(&log, "Unable to map GPIO memory\n");
		return EPERM;
	}

	mr.mr_base = PADCONF_REGISTERS_BASE;
	mr.mr_limit = PADCONF_REGISTERS_BASE + 0x1000;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		log_warn(&log, "Unable to request permission to map memory\n");
		return EPERM;
	}

	base =
	    (uint32_t) vm_map_phys(SELF, (void *) PADCONF_REGISTERS_BASE,
	    0x1000);

	if (base == (uint32_t) MAP_FAILED) {
		log_warn(&log, "Unable to map GPIO memory\n");
		return EPERM;
	}
	return OK;
}

int
padconf_set(u32_t padconf, u32_t mask, u32_t value)
{
	assert(padconf <= CONTROL_PADCONF_ETK_D14);
	set32(base + padconf, mask, value);
	return OK;
}

int
padconf_release()
{
	assert(use_count > 0);
	use_count--;

	if (use_count == 0) {
		vm_unmap_phys(SELF, (void *) base, 0x1000);
	}
	base = 0;
	return OK;
}
