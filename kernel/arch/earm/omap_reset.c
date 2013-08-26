#include <assert.h>
#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/type.h>
#include <io.h>

#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "kernel/vm.h"
#include "kernel/proto.h"
#include "arch_proto.h"
#include "omap_reset.h"

#ifdef AM335X
#define CM_BASE 0x44E00000
#define CM_SIZE 0x1000
#define PRM_DEVICE_OFFSET 0xf00
#define PRM_RSTCTRL_REG 0x00
#define RST_GLOBAL_WARM_SW_BIT 0 
#elif DM37XX
#define CM_BASE 0x48307000
#define CM_SIZE 0x1000
#define PRM_RSTCTRL_REG 0x250
#define RST_DPLL3_BIT 2
#else
#define CM_BASE 0x00000000
#define CM_SIZE 0x1000
#endif

struct omap_reset
{
	vir_bytes base;
	vir_bytes size;
};

static struct omap_reset omap_reset = {
	.base = CM_BASE,
	.size = CM_SIZE
};

static kern_phys_map reset_phys_map;

void
omap3_reset_init(void)
{
#if defined(AM335X) || defined(DM37XX)
	kern_phys_map_ptr(omap_reset.base, omap_reset.size, &reset_phys_map,
	    (vir_bytes) &omap_reset.base);
#endif /* AM335X || DM37XX */
}

void
omap3_reset(void)
{
#ifdef AM335X
	mmio_set((omap_reset.base + PRM_DEVICE_OFFSET + PRM_RSTCTRL_REG), (1 << RST_GLOBAL_WARM_SW_BIT));
#elif DM37XX
	mmio_set((omap_reset.base + PRM_RSTCTRL_REG), (1 << RST_DPLL3_BIT));
#endif
}
