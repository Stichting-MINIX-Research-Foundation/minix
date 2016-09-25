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

#include "rpi_serial.h"

struct pl011_serial
{
	vir_bytes base;
	vir_bytes size;
};

static struct pl011_serial pl011_serial = {
	.base = 0,
};

static kern_phys_map serial_phys_map;

/*
 * In kernel serial for the RPi. The serial driver like most other
 * drivers needs to be started early and even before the MMU is turned on.
 * We start by directly accessing the hardware memory address. Later on
 * when the MMU is turned on we still use a 1:1 mapping for these addresses.
 *
 * Pretty soon we are going to remap these addresses at later stage. And this
 * requires us to use a dynamic base address. The idea is to receive a callback
 * from VM with the new address to use.
 *
 * The serial driver also gets used in the "pre_init" stage before the kernel is loaded
 * in high memory so keep in mind there are two copies of this code in the kernel.
 */
void
bsp_ser_init()
{
	if (BOARD_IS_RPI_2_B(machine.board_id) ||
	    BOARD_IS_RPI_3_B(machine.board_id)) {
		pl011_serial.base = RPI2_PL011_DEBUG_UART_BASE;
	}

	pl011_serial.size = 0x1000;	/* 4k */

	kern_phys_map_ptr(pl011_serial.base, pl011_serial.size,
	    VMMF_UNCACHED | VMMF_WRITE, &serial_phys_map,
	    (vir_bytes) & pl011_serial.base);

	assert(pl011_serial.base);

	/* Set UART to 115200 bauds */
	if (BOARD_IS_RPI_2_B(machine.board_id)) {
		/* UARTCLK=48MHz */
		mmio_write(pl011_serial.base + PL011_IBRD, 1);
		mmio_write(pl011_serial.base + PL011_FBRD, 40);
	}
	else if (BOARD_IS_RPI_3_B(machine.board_id)) {
		/* UARTCLK=3MHz */
		mmio_write(pl011_serial.base + PL011_IBRD, 26);
		mmio_write(pl011_serial.base + PL011_FBRD, 3);
	}

	mmio_write(pl011_serial.base + PL011_LCRH, 0x70);
	mmio_write(pl011_serial.base + PL011_CR, 0x301);
}

void
bsp_ser_putc(char c)
{
	int i;
	assert(pl011_serial.base);

	/* Wait until FIFO's not full */
	for (i = 0; i < 100000; i++) {
		if ((mmio_read(pl011_serial.base + PL011_FR) & PL011_FR_TXFF) == 0) {
			break;
		}
	}

	/* Write character */
	mmio_write(pl011_serial.base + PL011_DR, c);

	/* And wait again until FIFO's empty to prevent TTY from overwriting */
	for (i = 0; i < 100000; i++) {
		if (mmio_read(pl011_serial.base + PL011_FR) & PL011_FR_TXFE) {
			break;
		}
	}
}
