
#include "kernel/kernel.h"

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <machine/cpu.h>
#include <assert.h>
#include <signal.h>
#include <machine/vm.h>
#include <io.h>

#include <minix/reboot.h>
#include <minix/u64.h>

#include "archconst.h"
#include "arch_proto.h"
#include "serial.h"
#include "omap_rtc.h"
#include "omap_reset.h"
#include "kernel/proc.h"
#include "kernel/debug.h"
#include "direct_utils.h"
#include <machine/multiboot.h>

void
halt_cpu(void)
{
	asm volatile("dsb");
	asm volatile("cpsie i");
	asm volatile("wfi");
	asm volatile("cpsid i");
}

void
reset(void)
{
	omap3_reset();
	direct_print("Reset not supported.");
	while (1);
}

void
poweroff(void)
{

/*
 * The am335x can signal an external power management chip to cut the power
 * by toggling the PMIC_POWER_EN pin. It might fail if there isn't an
 * external PMIC or if the PMIC hasn't been configured to respond to toggles.
 * The only way to pull the pin low is via ALARM2 (see TRM 20.3.3.8).
 * At this point PM should have already signaled readclock to set the alarm.
 */
#ifdef AM335X

	/* Powers down the SoC within 3 seconds */
	direct_print("PMIC Power-Off in 3 Seconds\n");

	/* rtc was frozen to prevent premature power-off, unfreeze it now */
	omap3_rtc_run();

	/* wait for the alarm to go off and PMIC to disable power to SoC */
	while (1);

#endif /* AM335X */

	/* fallback option: hang */
	direct_print("Unable to power-off this device.");
	while (1);
}

__dead void
arch_shutdown(int how)
{
	switch (how) {
	case RBT_HALT:
		/* Hang */
		for (; ; ) halt_cpu();
		NOT_REACHABLE;

	case RBT_POWEROFF:
		/* Power off if possible, hang otherwise */
		poweroff();
		NOT_REACHABLE;

	default:
	case RBT_DEFAULT:
	case RBT_REBOOT:
	case RBT_RESET:
		/* Reset the system */
		reset();
		NOT_REACHABLE;
	}
	while (1);
}

#ifdef DEBUG_SERIAL
void
ser_putc(char c)
{
	omap3_ser_putc(c);
}

#endif
