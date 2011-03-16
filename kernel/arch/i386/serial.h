
#ifndef _KERN_SERIAL_H
#define _KERN_SERIAL_H 1

#define THRREG  0	/* transmitter holding, write-only, DLAB must be clear */
#define RBRREG  0	/* receiver buffer, read-only, DLAB must be clear */
#define DLLREG  0	/* divisor latch LSB, read/write, DLAB must be set */
#define DLMREG  1	/* divisor latch MSB, read/write, DLAB must be set */
#define FICRREG 2	/* FIFO control, write-only */
#define LCRREG  3	/* line control, read/write */
#define LSRREG  5	/* line status, read-only */
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

#define UART_BASE_FREQ	115200U	

#endif
