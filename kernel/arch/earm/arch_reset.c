
#include "kernel/kernel.h"

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <machine/cpu.h>
#include <assert.h>
#include <signal.h>
#include <machine/vm.h>

#include <minix/u64.h>

#include "archconst.h"
#include "arch_proto.h"
#include "serial.h"
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
}

void
reset(void)
{
    while (1);
}

__dead void
arch_shutdown(int how)
{
	while (1);
}

#ifdef DEBUG_SERIAL
void
ser_putc(char c)
{
	omap3_ser_putc(c);
}

#endif
