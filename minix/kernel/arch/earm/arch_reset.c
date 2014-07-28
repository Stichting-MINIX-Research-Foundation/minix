#include "kernel/kernel.h"

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <machine/cpu.h>
#include <assert.h>
#include <signal.h>
#include <machine/vm.h>
#include <io.h>

#include <minix/board.h>
#include <sys/reboot.h>

#include <minix/u64.h>

#include "archconst.h"
#include "arch_proto.h"
#include "bsp_reset.h"
#include "bsp_serial.h"
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
	bsp_reset(); /* should not exit */
	direct_print("Reset not supported.");
	while (1);
}

void
poweroff(void)
{
	bsp_poweroff();
	/* fallback option: hang */
	direct_print("Unable to power-off this device.");
	while (1);
}

__dead void
arch_shutdown(int how)
{

	if((how & RB_POWERDOWN) == RB_POWERDOWN) {
		/* Power off if possible, hang otherwise */
		poweroff();
		NOT_REACHABLE;
	}

	if(how & RB_HALT) {
		/* Hang */
		for (; ; ) halt_cpu();
		NOT_REACHABLE;
	}

	/* Reset the system */
	reset();
	NOT_REACHABLE;

	while (1);
}

#ifdef DEBUG_SERIAL
void
ser_putc(char c)
{
	bsp_ser_putc(c);
}

#endif
