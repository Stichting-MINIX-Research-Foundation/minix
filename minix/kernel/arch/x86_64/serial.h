
#ifndef _KERN_SERIAL_H
#define _KERN_SERIAL_H 1

#define THRREG  0	/* transmitter holding, write-only, DLAB must be clear */
#define RBRREG  0	/* receiver buffer, read-only, DLAB must be clear */
#define DLLREG  0	/* divisor latch LSB, read/write, DLAB must be set */
#define DLMREG  1	/* divisor latch MSB, read/write, DLAB must be set */
#define	IERREG	1	/* interrupt enable (W) */
#define	IRRREG	2	/* interrupt identification (R) */
#define FICRREG 2	/* FIFO control, write-only */
#define LCRREG  3	/* line control, read/write */
#define	MCRREG	4	/* modem control register (R/W) */
#define LSRREG  5	/* line status, read-only */
#define	MSRREG	6	/* modem status register (R/W) */
#define SPRREG  7

#define COM1_BASE	0x3F8
#define COM1_THR	(COM1_BASE + THRREG)
#define COM1_RBR	(COM1_BASE + RBRREG)
#define COM1_DLL	(COM1_BASE + DLLREG)
#define COM1_DLM	(COM1_BASE + DLMREG)
#define COM1_LCR	(COM1_BASE + LCRREG)
#define         LCR_5BIT	0x00 /* 5 bits per data word */
#define         LCR_6BIT	0x01 /* 6 bits per data word */
#define         LCR_7BIT	0x02 /* 7 bits per data word */
#define         LCR_8BIT	0x03 /* 8 bits per data word */
#define         LCR_1STOP	0x00 /* 1/1.5 stop bits */
#define         LCR_2STOP	0x04 /* 2 stop bits */
#define         LCR_NPAR	0x00 /* no parity */
#define         LCR_OPAR	0x08 /* odd parity */
#define         LCR_EPAR	0x18 /* even parity */
#define         LCR_BREAK	0x40 /* enable break */
#define         LCR_DLAB	0x80 /* access DLAB registers */
#define COM1_LSR	(COM1_BASE + LSRREG)
#define         LSR_DR          0x01
#define         LSR_THRE        0x20
#define         LCR_DLA         0x80

/* fifo control register */
#define	FIFO_ENABLE	0x01	/* Turn the FIFO on */
#define	FIFO_RCV_RST	0x02	/* Reset RX FIFO */
#define	FIFO_XMT_RST	0x04	/* Reset TX FIFO */
#define	FIFO_DMA_MODE	0x08
#define	FIFO_UART_ON	0x10	/* JZ47xx only */
#define	FIFO_64B_ENABLE	0x20	/* 64byte FIFO Enable (16750) */
#define	FIFO_TRIGGER_1	0x00	/* Trigger RXRDY intr on 1 character */
#define	FIFO_TRIGGER_4	0x40	/* ibid 4 */
#define	FIFO_TRIGGER_8	0x80	/* ibid 8 */
#define	FIFO_TRIGGER_14	0xc0	/* ibid 14 */

/* modem control register */
#define MCR_PRESCALE	0x80	/* 16650/16950: Baud rate prescaler select */
#define MCR_MDCE	0x80	/* Ingenic: modem control enable */
#define MCR_TCR_TLR	0x40	/* OMAP: enables access to the TCR & TLR regs */
#define MCR_FCM		0x40	/* Ingenic: 1 - hardware flow control */
#define MCR_XONENABLE	0x20	/* OMAP XON_EN */
#define MCR_AFE		0x20	/* tl16c750: Flow Control Enable */
#define	MCR_LOOPBACK	0x10	/* Loop test: echos from TX to RX */
#define	MCR_IENABLE	0x08	/* Out2: enables UART interrupts */
#define	MCR_DRS		0x04	/* Out1: resets some internal modems */
#define	MCR_RTS		0x02	/* Request To Send */
#define	MCR_DTR		0x01	/* Data Terminal Ready */

#define	COM_FREQ	1843200	/* 16-bit baud rate divisor */
#ifndef COM_TOLERANCE
#define	COM_TOLERANCE	30	/* baud rate tolerance, in 0.1% units */
#endif

#define UART_BASE_FREQ	115200U	

#endif
