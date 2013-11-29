/* kernel headers */
#include <minix/syslib.h>
#include <minix/drvlib.h>
#include <minix/log.h>
#include <minix/mmio.h>
#include <minix/clkconf.h>
#include <minix/type.h>
#include <minix/board.h>

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

/* used for logging */
static struct log log = {
	.name = "omap_clkconf",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

#define DM37XX_CM_BASE 0x48004000
#define AM335X_CM_BASE 0x44E00000

static u32_t base = 0;
static u32_t use_count = 0;

int
clkconf_init()
{
	use_count++;
	struct machine machine;
	sys_getmachine(&machine);
	u32_t cm_base = 0;


	if (base != 0) {
		/* when used in a library we can't guaranty we only call this
		 * method once */
		log_trace(&log, "Called %d times\n", use_count);
		return OK;
	}

	if (BOARD_IS_BBXM(machine.board_id)){
		cm_base = DM37XX_CM_BASE;
	} else if (BOARD_IS_BB(machine.board_id)){
		cm_base = AM335X_CM_BASE;
	}
	struct minix_mem_range mr;
	mr.mr_base = cm_base;
	mr.mr_limit = cm_base + 0x1000;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		log_warn(&log, "Unable to request permission to map memory\n");
		return EPERM;
	}

	base = (uint32_t) vm_map_phys(SELF, (void *) cm_base, 0x1000);

	if (base == (uint32_t) MAP_FAILED) {
		log_warn(&log, "Unable to map GPIO memory\n");
		return EPERM;
	}
	return OK;
}

int
clkconf_set(u32_t clk, u32_t mask, u32_t value)
{
	set32(base + clk, mask, value);
	return OK;
}

int
clkconf_release()
{
	assert(use_count > 0);
	use_count--;
	if (use_count == 0) {
		vm_unmap_phys(SELF, (void *) base, 0x1000);
	}
	return OK;
}
