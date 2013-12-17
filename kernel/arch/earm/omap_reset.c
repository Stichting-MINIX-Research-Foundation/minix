#include <assert.h>
#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/type.h>
#include <minix/board.h>
#include <io.h>

#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "kernel/vm.h"
#include "kernel/proto.h"
#include "arch_proto.h"
#include "omap_reset.h"

#define AM335X_CM_BASE 0x44E00000
#define AM335X_CM_SIZE 0x1000

#define AM335X_PRM_DEVICE_OFFSET 0xf00
#define AM335X_PRM_RSTCTRL_REG 0x00
#define AM335X_RST_GLOBAL_WARM_SW_BIT 0 

#define DM37XX_CM_BASE 0x48307000
#define DM37XX_CM_SIZE 0x1000
#define DM37XX_PRM_RSTCTRL_REG 0x250
#define DM37XX_RST_DPLL3_BIT 2

struct omap_reset
{
	vir_bytes base;
	vir_bytes size;
};

static struct omap_reset omap_reset;

static kern_phys_map reset_phys_map;

void
omap3_reset_init(void)
{
	if(BOARD_IS_BBXM(machine.board_id)) {
		omap_reset.base = DM37XX_CM_BASE;
		omap_reset.size = DM37XX_CM_SIZE;
	} else if(BOARD_IS_BB(machine.board_id)) {
		omap_reset.base = AM335X_CM_BASE;
		omap_reset.size = AM335X_CM_SIZE;
	}

	kern_phys_map_ptr(omap_reset.base, omap_reset.size, &reset_phys_map,
	    (vir_bytes) &omap_reset.base);
}

void
omap3_reset(void)
{
	if(BOARD_IS_BBXM(machine.board_id)) {
		mmio_set((omap_reset.base + DM37XX_PRM_RSTCTRL_REG), (1 << DM37XX_RST_DPLL3_BIT));
	} else if(BOARD_IS_BB(machine.board_id)) {
		mmio_set((omap_reset.base + AM335X_PRM_DEVICE_OFFSET + AM335X_PRM_RSTCTRL_REG), (1 << AM335X_RST_GLOBAL_WARM_SW_BIT));
	}
}
