/*
 * This is a mini driver for the AM335X Real Time Clock. The majority of the
 * work is done in user space in readclock, but for power-off the clock needs
 * to be put into run mode at the last possible moment in arch_reset.c. This
 * driver just implements mapping the memory and re-starting the clock.
 */

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
#include "omap_rtc.h"

#define RTC_SS_BASE 0x44e3e000
#define RTC_SS_SIZE 0x1000
#define RTC_CTRL_REG 0x40
#define RTC_CTRL_RTC_STOP_BIT 0

struct omap_rtc
{
	vir_bytes base;
	vir_bytes size;
};

static struct omap_rtc omap_rtc = {
	.base = RTC_SS_BASE,
	.size = RTC_SS_SIZE
};

static kern_phys_map rtc_phys_map;

void
omap3_rtc_init(void)
{
	if (BOARD_IS_BB(machine.board_id)) {
		kern_phys_map_ptr(omap_rtc.base, omap_rtc.size,
		    VMMF_UNCACHED | VMMF_WRITE, &rtc_phys_map,
		    (vir_bytes) & omap_rtc.base);
	}
}

void
omap3_rtc_run(void)
{
	if (BOARD_IS_BB(machine.board_id)) {
		/* Setting the stop bit starts the RTC running */
		mmio_set((omap_rtc.base + RTC_CTRL_REG),
		    (1 << RTC_CTRL_RTC_STOP_BIT));
	}
}
