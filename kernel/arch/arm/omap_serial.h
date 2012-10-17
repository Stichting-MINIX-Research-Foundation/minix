#ifndef _OMAP_SERIAL_H
#define _OMAP_SERIAL_H

/* UART register map */
#define OMAP3_UART1_BASE 0x4806A000 /* UART1 physical address */
#define OMAP3_UART2_BASE 0x4806C000 /* UART2 physical address */
#define OMAP3_UART3_BASE 0x49020000 /* UART3 physical address */

/* UART registers */
#define OMAP3_THR 0x000 /* Transmit holding register */
#define OMAP3_LSR 0x014 /* Line status register */
#define OMAP3_SSR 0x044 /* Supplementary status register */

/* Line status register fields */
#define OMAP3_LSR_TEMT    0x40 /* Transmitter empty */
#define OMAP3_LSR_THRE    0x20 /* Transmit-hold-register empty */

/* Supplementary status register fields */
#define OMAP3_SSR_TX_FIFO_FULL (1 << 0) /* Transmit FIFO full */

#define OMAP3_UART3_THR (OMAP3_UART3_BASE + OMAP3_THR)
#define OMAP3_UART3_LSR (OMAP3_UART3_BASE + OMAP3_LSR)
#define OMAP3_UART3_SSR (OMAP3_UART3_BASE + OMAP3_SSR)

#ifndef __ASSEMBLY__

void omap3_ser_putc(char c);

#endif /* __ASSEMBLY__ */

#endif /* _OMAP_SERIAL_H */
