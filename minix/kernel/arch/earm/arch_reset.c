#include "kernel/kernel.h"

// Removed: <unistd.h>, <ctype.h>, <string.h>, <assert.h>, <signal.h>, <sys/reboot.h>
// Kept: <machine/cpu.h>, <machine/vm.h>, <io.h>, <minix/board.h>, <minix/u64.h>, <machine/multiboot.h>
// (These will be reviewed later if they cause issues or pull userspace types)

#include <machine/cpu.h>
#include <machine/vm.h>
#include <io.h>
#include <minix/board.h>
#include <minix/u64.h>
#include <machine/multiboot.h>


// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>

#include "archconst.h"
#include "arch_proto.h"
#include "bsp_reset.h"
#include "bsp_serial.h"
#include "kernel/proc.h"
#include "kernel/debug.h"
#include "direct_utils.h"

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

	if((how & RB_POWERDOWN) == RB_POWERDOWN) { // RB_POWERDOWN might be undefined now
		/* Power off if possible, hang otherwise */
		poweroff();
		NOT_REACHABLE;
	}

	if(how & RB_HALT) { // RB_HALT might be undefined now
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
