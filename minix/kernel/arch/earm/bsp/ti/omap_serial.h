#ifndef _OMAP_SERIAL_H
#define _OMAP_SERIAL_H

/* UART register map */
#define OMAP3_DM37XX_DEBUG_UART_BASE 0x49020000 /* UART3 physical address */
#define OMAP3_AM335X_DEBUG_UART_BASE 0x44E09000 /* UART0? physical address */

/* UART registers */
#define OMAP3_THR 0x000 /* Transmit holding register */
#define OMAP3_LSR 0x014 /* Line status register */
#define OMAP3_SSR 0x044 /* Supplementary status register */

/* Line status register fields */
#define OMAP3_LSR_TEMT    0x40 /* Transmitter empty */
#define OMAP3_LSR_THRE    0x20 /* Transmit-hold-register empty */

/* Supplementary status register fields */
#define OMAP3_SSR_TX_FIFO_FULL (1 << 0) /* Transmit FIFO full */

#ifndef __ASSEMBLY__

void omap3_ser_init();
void omap3_ser_putc(char c);

#endif /* __ASSEMBLY__ */

#endif /* _OMAP_SERIAL_H */
