#ifndef _RPI_SERIAL_H
#define _RPI_SERIAL_H

#define RPI2_PL011_DEBUG_UART_BASE 0x3f201000

#define PL011_DR        0x0
#define PL011_FR        0x18
#define PL011_IBRD      0x24
#define PL011_FBRD      0x28
#define PL011_LCRH      0x2c
#define PL011_CR        0x30

#define PL011_FR_TXFF   (1<<5)
#define PL011_FR_TXFE   (1<<7)

#ifndef __ASSEMBLY__

void bsp_ser_init();
void bsp_ser_putc(char c);

#endif /* __ASSEMBLY__ */

#endif /* _RPI_SERIAL_H */
