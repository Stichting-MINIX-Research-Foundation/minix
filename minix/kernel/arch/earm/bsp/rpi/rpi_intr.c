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
#include "hw_intr.h"

#include "rpi_intr_registers.h"
#include "rpi_timer_registers.h"

static struct rpi2_intr
{
	vir_bytes base;
	vir_bytes core_base;
	int size;
} rpi2_intr;

static kern_phys_map intr_phys_map;
static kern_phys_map timer_phys_map;

static irq_hook_t dummy8_irq_hook;
static irq_hook_t dummy40_irq_hook;
static irq_hook_t dummy41_irq_hook;
static irq_hook_t dummy51_irq_hook;

int
dummy_irq_handler()
{
	/*
	 * The Raspberry Pi has a bunch of cascaded interrupts that are useless
	 * for MINIX. This handler catches them so as not to pollute the console
	 * with spurious interrupts messages.
	 */
	return 0;
}

int
intr_init(const int auto_eoi)
{
	if (BOARD_IS_RPI_2_B(machine.board_id) ||
	    BOARD_IS_RPI_3_B(machine.board_id)) {
		rpi2_intr.base = RPI2_INTR_BASE;
		rpi2_intr.core_base = RPI2_QA7_BASE;
	} else {
		panic
		    ("Can not do the interrupt setup. machine (0x%08x) is unknown\n",
		    machine.board_id);
	}

	rpi2_intr.size = 0x1000;	/* 4K */

	kern_phys_map_ptr(rpi2_intr.base, rpi2_intr.size,
	    VMMF_UNCACHED | VMMF_WRITE,
	    &intr_phys_map, (vir_bytes) & rpi2_intr.base);
	kern_phys_map_ptr(rpi2_intr.core_base, rpi2_intr.size,
	    VMMF_UNCACHED | VMMF_WRITE,
	    &timer_phys_map, (vir_bytes) & rpi2_intr.core_base);

	/* Disable FIQ and all interrupts */
	mmio_write(rpi2_intr.base + RPI2_INTR_FIQ_CTRL, 0);
	mmio_write(rpi2_intr.base + RPI2_INTR_DISABLE_BASIC, 0xFFFFFFFF);
	mmio_write(rpi2_intr.base + RPI2_INTR_DISABLE1, 0xFFFFFFFF);
	mmio_write(rpi2_intr.base + RPI2_INTR_DISABLE2, 0xFFFFFFFF);

	/* Enable ARM timer routing to IRQ here */
	mmio_write(rpi2_intr.core_base + QA7_CORE0TIMER, 0x8);

	/* Register dummy irq handlers */
	put_irq_handler(&dummy8_irq_hook, 8, dummy_irq_handler);
	put_irq_handler(&dummy40_irq_hook, 40, dummy_irq_handler);
	put_irq_handler(&dummy41_irq_hook, 41, dummy_irq_handler);
	put_irq_handler(&dummy51_irq_hook, 51, dummy_irq_handler);

	return 0;
}

void
bsp_irq_handle(void)
{
	/* Function called from assembly to handle interrupts */
	uint32_t irq_0_31 = mmio_read(rpi2_intr.core_base + QA7_CORE0INT);
	uint32_t irq_32_63 = mmio_read(rpi2_intr.base + RPI2_INTR_BASIC_PENDING);
	uint32_t irq_64_95 = mmio_read(rpi2_intr.base + RPI2_INTR_PENDING1);
	uint64_t irq_96_128 = mmio_read(rpi2_intr.base + RPI2_INTR_PENDING2);

	int irq = 0;

	/* Scan all interrupts bits */
	for (irq = 0; irq < 128; irq++) {
		int is_pending = 0;
		if (irq < 32)
			is_pending = irq_0_31 & (1 << irq);
		else if (irq < 64)
			is_pending = irq_32_63 & (1 << (irq-32));
		else if (irq < 96)
			is_pending = irq_64_95 & (1 << (irq-64));
		else
			is_pending = irq_96_128 & (1 << (irq-96));

		if (is_pending)
			irq_handle(irq);
	}

	/* Clear all pending interrupts */
	mmio_write(rpi2_intr.base + RPI2_INTR_BASIC_PENDING, irq_32_63);
	mmio_write(rpi2_intr.base + RPI2_INTR_PENDING1, irq_64_95);
	mmio_write(rpi2_intr.base + RPI2_INTR_PENDING2, irq_96_128);
}

void
bsp_irq_unmask(int irq)
{
	if (irq < 32)
		/* Nothing to do */
		;
	else if (irq < 64)
		mmio_write(rpi2_intr.base + RPI2_INTR_ENABLE_BASIC, 1 << (irq-32));
	else if (irq < 96)
		mmio_write(rpi2_intr.base + RPI2_INTR_ENABLE1, 1 << (irq-64));
	else if (irq < 128)
		mmio_write(rpi2_intr.base + RPI2_INTR_ENABLE2, 1 << (irq-96));
}

void
bsp_irq_mask(const int irq)
{
	if (irq < 32)
		/* Nothing to do */
		;
	else if (irq < 64)
		mmio_write(rpi2_intr.base + RPI2_INTR_DISABLE_BASIC, 1 << (irq-32));
	else if (irq < 96)
		mmio_write(rpi2_intr.base + RPI2_INTR_DISABLE1, 1 << (irq-64));
	else if (irq < 128)
		mmio_write(rpi2_intr.base + RPI2_INTR_DISABLE2, 1 << (irq-96));
}
