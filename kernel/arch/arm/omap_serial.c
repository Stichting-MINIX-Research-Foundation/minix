#include <sys/types.h>
#include <machine/cpu.h>
#include <io.h>
#include "omap_serial.h"

void omap3_ser_putc(char c)
{
    int i;

    /* Wait until FIFO's empty */
    for (i = 0; i < 100000; i++)
	if (mmio_read(OMAP3_UART3_LSR) & OMAP3_LSR_TX_FIFO_E)
	    break;

    /* Write character */
    mmio_write(OMAP3_UART3_THR, c);
}
