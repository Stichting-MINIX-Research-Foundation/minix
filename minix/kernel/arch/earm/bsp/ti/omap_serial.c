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

#include "omap_serial.h"

struct omap_serial
{
	vir_bytes base;
	vir_bytes size;
};

static struct omap_serial omap_serial = {
	.base = 0,
};

static kern_phys_map serial_phys_map;

/* 
 * In kernel serial for the omap. The serial driver like most other
 * drivers needs to be started early and even before the MMU is turned on.
 * We start by directly accessing the hardware memory address. Later on
 * a when the MMU is turned on we still use a 1:1 mapping for these addresses.
 *
 * Pretty soon we are going to remap these addresses at later stage. And this
 * requires us to use a dynamic base address. The idea is to receive a callback
 * from VM with the new address to use.
 *
 * We also anticipate on the beaglebone port an try to keep the differences between
 * the drivers to a minimum by initializing a struct here and not using (to much) 
 * constants in the code.
 *
 * The serial driver also gets used in the "pre_init" stage before the kernel is loaded
 * in high memory so keep in mind there are two copies of this code in the kernel.
 */
void
bsp_ser_init()
{
	if (BOARD_IS_BBXM(machine.board_id)) {
		omap_serial.base = OMAP3_DM37XX_DEBUG_UART_BASE;
	} else if (BOARD_IS_BB(machine.board_id)) {
		omap_serial.base = OMAP3_AM335X_DEBUG_UART_BASE;
	}
	omap_serial.size = 0x1000;	/* 4k */

	kern_phys_map_ptr(omap_serial.base, omap_serial.size,
	    VMMF_UNCACHED | VMMF_WRITE, &serial_phys_map,
	    (vir_bytes) & omap_serial.base);
	assert(omap_serial.base);
}

void
bsp_ser_putc(char c)
{
	int i;
	assert(omap_serial.base);

	/* Wait until FIFO's empty */
	for (i = 0; i < 100000; i++) {
		if (mmio_read(omap_serial.base + OMAP3_LSR) & OMAP3_LSR_THRE) {
			break;
		}
	}

	/* Write character */
	mmio_write(omap_serial.base + OMAP3_THR, c);

	/* And wait again until FIFO's empty to prevent TTY from overwriting */
	for (i = 0; i < 100000; i++) {
		if (mmio_read(omap_serial.base +
			OMAP3_LSR) & (OMAP3_LSR_THRE | OMAP3_LSR_TEMT)) {
			break;
		}
	}
}
