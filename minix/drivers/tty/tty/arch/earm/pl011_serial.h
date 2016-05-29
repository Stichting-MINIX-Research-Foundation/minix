#ifndef _PL011_SERIAL_H
#define _PL011_SERIAL_H

/* UART register map */
#define PL011_UART0_BASE 0x3f201000	/* UART0 physical address */

/* UART registers */
#define PL011_DR		0x000	/* Data register, */
#define PL011_SR_CR		0x004	/* Receive status register/error clear register */
#define PL011_FR		0x018	/* Flag register, */
#define PL011_ILPR		0x020	/* IrDA low-power counter register */
#define PL011_IBRD		0x024	/* Integer baud rate register */
#define PL011_FBRD		0x028	/* Fractional baud rate register */
#define PL011_LCR_H		0x02C	/* Line control register, */
#define PL011_CR		0x030	/* control register */
#define PL011_IFLS		0x034	/* Interrupt FIFO level select register */
#define PL011_IMSC		0x038	/* Interrupt mask set/clear register */
#define PL011_RIS		0x03C	/* Raw interrupt status register */
#define PL011_MIS		0x040	/* Masked interrupt status register */
#define PL011_ICR		0x044	/* Interrupt clear register */
#define PL011_DMACR		0x048	/* DMA control register */

#define PL011_RXRIS	0x10
#define PL011_TXRIS	0x20

#define PL011_RXFE	0x10
#define PL011_TXFF	0x20

#define PL011_FEN	0x10

/* Line Control Register bits */
// #define PL011_LCR_PEN	0x01	/* Enable parity */

// #define PL011_LCR_WLEN5		0x00	/* Wordlength 5 bits */   //TODO How to know the offset? 0x05
// #define PL011_LCR_WLEN6		0x01	/* Wordlength 6 bits */
// #define PL011_LCR_WLEN7		0x02	/* Wordlength 7 bits */
// #define PL011_LCR_WLEN8		0x03	/* Wordlength 8 bits */

// Send break. If this bit is set to 1, a low-level is continually output on the UARTTXD output, after
// completing transmission of the current character. For the proper execution of the break command, the
// software must set this bit for at least two complete frames.
// For normal use, this bit must be cleared to 0
// #define PL011_LCR_BRK		0x00



/* Line Control Register bits */
// #define UART_LCR_DLAB		0x80	/* Divisor latch access bit */

// #define UART_LCR_SBC		0x40	/* Set break control */

// #define UART_LCR_EPAR		0x10	/* Even parity select */

// #define UART_LCR_PARITY		0x08	/* Enable parity */

// #define UART_LCR_STOP		0x04	/* Stop bits; 0=1 bit, 1=2 bits */

// #define UART_LCR_WLEN5		0x00	/* Wordlength 5 bits */
// #define UART_LCR_WLEN6		0x01	/* Wordlength 6 bits */
// #define UART_LCR_WLEN7		0x02	/* Wordlength 7 bits */
// #define UART_LCR_WLEN8		0x03	/* Wordlength 8 bits */

// #define UART_LCR_CONF_MODE_A	UART_LCR_DLAB	/* Configuration Mode A */
// #define UART_LCR_CONF_MODE_B	0xBF		/* Configuration Mode B */




//// PL011_IMSC   /* Interrupt mask set/clear register */
//// UARTIMSC

//// #define UART_IER_MSI		0x08	/* Modem status interrupt */
////  UARTMSINTR ??  /* Modem status interrupt */

/* UART register map */
// #define OMAP3_UART1_BASE 0x4806A000 /* UART1 physical address */
// #define OMAP3_UART2_BASE 0x4806C000 /* UART2 physical address */
// #define OMAP3_UART3_BASE 0x49020000 /* UART3 physical address */

// /* UART registers */
// #define OMAP3_THR		0	/* Transmit holding register */
// #define OMAP3_RHR		0	/* Receive holding register */
// #define OMAP3_DLL		0	/* Divisor latches low */
// #define OMAP3_DLH		1	/* Divisor latches high */
// #define OMAP3_IER		1	/* Interrupt enable register */
// #define OMAP3_IIR		2	/* Interrupt identification register */
// #define OMAP3_EFR		2	/* Extended features register */
// #define OMAP3_FCR		2	/* FIFO control register */
// #define OMAP3_LCR		3	/* Line control register */
// #define OMAP3_MCR		4	/* Modem control register */
// #define OMAP3_LSR		5	/* Line status register */
// #define OMAP3_MSR		6	/* Modem status register */
// #define OMAP3_TCR		6
// #define OMAP3_MDR1		0x08	/* Mode definition register 1 */
// #define OMAP3_MDR2		0x09	/* Mode definition register 2 */
// #define OMAP3_SCR		0x10	/* Supplementary control register */
// #define OMAP3_SSR		0x11	/* Supplementary status register */
// #define OMAP3_SYSC		0x15	/* System configuration register */
// #define OMAP3_SYSS		0x16	/* System status register */

/* Enhanced Features Register bits */
#define UART_EFR_ECB		(1 << 4)/* Enhanced control bit */
#define UART_EFR_AUTO_CTS	(1 << 6)/* auto cts enable */
#define UART_EFR_AUTO_RTS	(1 << 7)/* auto rts enable */

/* Interrupt Enable Register bits */
#define UART_IER_MSI		0x08	/* Modem status interrupt */
#define UART_IER_RLSI		0x04	/* Receiver line status interrupt */
#define UART_IER_THRI		0x02	/* Transmitter holding register int. */ 
#define UART_IER_RDI		0x01	/* Receiver data interrupt */

/* FIFO control register */
// #define OMAP_UART_FCR_RX_FIFO_TRIG_SHIFT	6
// #define OMAP_UART_FCR_RX_FIFO_TRIG_MASK		(0x3 << 6)
// #define OMAP_UART_FCR_TX_FIFO_TRIG_SHIFT	4
// #define OMAP_UART_FCR_TX_FIFO_TRIG_MASK		(0x3 << 4)
#define UART_FCR_ENABLE_FIFO	0x01	/* Enable the fifo */
#define UART_FCR_CLR_RCVR	0x02	/* Clear the RCVR FIFO */
#define UART_FCR_CLR_XMIT	0x04	/* Clear the XMIT FIFO */

/* Interrupt Identification Register bits */
#define UART_IIR_RDI		0x04	/* Data ready interrupt */
#define UART_IIR_THRI		0x02	/* Transmitter holding register empty */
#define UART_IIR_NO_INT		0x01	/* No interrupt is pending */

/* Line Control Register bits */
// #define UART_LCR_DLAB		0x80	/* Divisor latch access bit */
// #define UART_LCR_SBC		0x40	/* Set break control */
// #define UART_LCR_EPAR		0x10	/* Even parity select */
// #define UART_LCR_PARITY		0x08	/* Enable parity */
// #define UART_LCR_STOP		0x04	/* Stop bits; 0=1 bit, 1=2 bits */
// #define UART_LCR_WLEN5		0x00	/* Wordlength 5 bits */
// #define UART_LCR_WLEN6		0x01	/* Wordlength 6 bits */
// #define UART_LCR_WLEN7		0x02	/* Wordlength 7 bits */
// #define UART_LCR_WLEN8		0x03	/* Wordlength 8 bits */

#define UART_LCR_CONF_MODE_A	UART_LCR_DLAB	/* Configuration Mode A */
#define UART_LCR_CONF_MODE_B	0xBF		/* Configuration Mode B */

/* Line Status Register bits */
#define UART_LSR_THRE		0x20	/* Transmit-hold-register empty */
#define UART_LSR_BI		0x10	/* Break condition */
#define UART_LSR_DR		0x01	/* Data ready */

/* Modem Control Register bits */
#define UART_MCR_TCRTLR		0x40	/* Access TCR/TLR */
#define UART_MCR_OUT2		0x08	/* Out2 complement */
#define UART_MCR_RTS		0x02	/* RTS complement */
#define UART_MCR_DTR		0x01	/* DTR output low */

/* Mode Definition Register 1 bits */
#define OMAP_MDR1_DISABLE	0x07
#define OMAP_MDR1_MODE13X	0x03	
#define OMAP_MDR1_MODE16X	0x00

/* Modem Status Register bits */
#define UART_MSR_DCD		0x80	/* Data Carrier Detect */
#define UART_MSR_CTS		0x10	/* Clear to Send */
#define UART_MSR_DDCD		0x08	/* Delta DCD */

/* Supplementary control Register bits */
// #define OMAP_UART_SCR_RX_TRIG_GRANU1_MASK	(1 << 7)

/* System Control Register bits */
#define UART_SYSC_SOFTRESET	0x02

/* System Status Register bits */
#define UART_SYSS_RESETDONE	0x01

/* Line status register fields */
// #define OMAP3_LSR_TX_FIFO_E    (1 << 5) /* Transmit FIFO empty */
// #define OMAP3_LSR_RX_FIFO_E    (1 << 0) /* Receive FIFO empty */
// #define OMAP3_LSR_RXOE         (1 << 1) /* Overrun error.*/

/* Supplementary status register fields */
// #define OMAP3_SSR_TX_FIFO_FULL (1 << 0) /* Transmit FIFO full */

#endif /* _PL011_SERIAL_H */
