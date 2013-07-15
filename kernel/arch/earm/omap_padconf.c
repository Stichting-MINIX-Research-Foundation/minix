/* temporary padconf work-around
 *
 * pinmux must be configured in privileged mode on the Cortex-A8. This code
 * does a simple static configuration of a few pins. It's only here until
 * a proper padconf server that does kernel calls can be developed.
 */

#include "kernel/kernel.h"
#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/mmio.h>
#include <assert.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>

#include "omap_padconf.h"

u32_t base = PADCONF_REGISTERS_BASE;

static int kernel_padconf_set(u32_t padconf, u32_t mask, u32_t value);

static int
kernel_padconf_set(u32_t padconf, u32_t mask, u32_t value)
{
	assert(padconf <= CONTROL_CONF_USB1_DRVVBUS);
	set32(base + padconf, mask, value);
	return OK;
}

void
omap3_padconf_init(void)
{

#ifdef AM335X

	u32_t pinopts, value;

	/* Common options for BeagleBone I2C Pins */
	pinopts = CONTROL_CONF_SLEWCTRL | CONTROL_CONF_RXACTIVE |
	    CONTROL_CONF_PUTYPESEL;

	/* I2C0 */
	value = pinopts | CONTROL_CONF_MUXMODE(0);
	kernel_padconf_set(CONTROL_CONF_I2C0_SDA, 0xffffffff, value);
	kernel_padconf_set(CONTROL_CONF_I2C0_SCL, 0xffffffff, value);

	/* I2C1 */
	value = pinopts | CONTROL_CONF_MUXMODE(2);
	kernel_padconf_set(CONTROL_CONF_SPI0_CS0, 0xffffffff, value);
	kernel_padconf_set(CONTROL_CONF_SPI0_D1, 0xffffffff, value);

	/* I2C2 */
	value = pinopts | CONTROL_CONF_MUXMODE(3);
	kernel_padconf_set(CONTROL_CONF_UART1_CTSN, 0xffffffff, value);
	kernel_padconf_set(CONTROL_CONF_UART1_RTSN, 0xffffffff, value);

#endif

	/* nothing to do for DM37XX */
	return;
}
