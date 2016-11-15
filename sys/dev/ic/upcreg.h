/* $NetBSD: upcreg.h,v 1.3 2003/03/02 00:21:47 bjh21 Exp $ */

/*
 * Ben Harris, 2000, 2003
 *
 * This file is in the public domain.
 */

/*
 * upcreg.h - CHIPS and Technologies Universal Peripheral Controllers
 */

/*
 * This file contains register details for:
 * CHIPS 82C710 Universal Peripheral Controller
 * CHIPS 82C711 Universal Peripheral Controller II
 * CHIPS 82C721 Universal Peripheral Controller III
 */

#define UPC_BUS_SIZE		0x400 /* Approximate */

/* Fixed port addresses */

#define UPC_PORT_IDECMDBASE    	0x1f0 /* IDE primary base */
#define UPC_PORT_IDECTLBASE    	0x3f6 /* IDE secondary base */
#define UPC_PORT_FDCBASE	0x3f4 /* FDC base address (82C721 only) */
#define UPC_PORT_GAME		0x201 /* -GAMECS active */

/* 82C710 Configuration magic sequences */

#define UPC1_PORT_CFG1		0x2fa /* First magic config register */
#define UPC1_PORT_CFG2		0x3fa /* Second magic config register */
#define UPC1_CFGMAGIC_1		0x55 /* First magic number */
#define UPC1_CFGMAGIC_2		0xaa /* Second magic number */
#define UPC1_CFGMAGIC_3		0x36 /* Third magic number */

/* 82C710 configuration registers */
#define UPC1_CFGADDR_CR0	0x00
#define UPC1_CFGADDR_CR1	0x01
#define UPC1_CFGADDR_CR2	0x02
#define UPC1_CFGADDR_UARTBASE	0x04
#define UPC1_CFGADDR_PARBASE	0x06
#define UPC1_CFGADDR_GPCSBASE	0x09
#define UPC1_CFGADDR_CRA	0x0a
#define UPC1_CFGADDR_CRB	0x0b
#define UPC1_CFGADDR_CRC	0x0c
#define UPC1_CFGADDR_MOUSEBASE	0x0d
#define UPC1_CFGADDR_CRE	0x0e
#define UPC1_CFGADDR_CONFBASE	0x0f
#define UPC1_CFGADDR_EXIT	0x0f

/* 82C710 configuration register 0x00 */
#define UPC1_CR0_VALID		0x80 /* Device has been configured */
#define UPC1_CR0_OSC_MASK	0x60 /* Oscillator control */
#define UPC1_CR0_OSC_ON		0x00 /* Oscillator always on */
#define UPC1_CR0_OSC_PWRGD	0x20 /* Oscillator on when PWRGD */
#define UPC1_CR0_OSC_OFF	0x60 /* Oscillator always off */
#define UPC1_CR0_PEN		0x08 /* Enable parallel port */
#define UPC1_CR0_SEN		0x04 /* Enable UART */

/* 82C710 configuration register 0x01 */
#define UPC1_CR1_RESET		0x80 /* Reset ignores serial port */
#define UPC1_CR1_PRBI		0x40 /* Bi-directional printer support */
#define UPC1_CR1_FCTS		0x20 /* Force UART CTS* active */
#define UPC1_CR1_FDSR		0x10 /* Force UART DSR* active */
#define UPC1_CR1_FDCD		0x08 /* Force UART DCD* active */

/* 82C710 configuration register 0x02 */
#define UPC1_CR2_SCLK		0x40 /* Divide UART clock by 4 rather than 2 */
#define UPC1_CR2_RXSRC		0x20 /* Use divider output for Rx clock */
#define UPC1_CR2_TXSRC		0x10 /* Use divider output for Tx clock */

/* 82C710 configuration register 0x04 */
#define UPC1_UARTBASE_SHIFT	2

/* 82C710 configuration register 0x06 */
#define UPC1_PARBASE_SHIFT	2

/* 82C710 configuration register 0x09 */
#define UPC1_GPCSBASE_SHIFT	2

/* 82C710 configuration register 0x0a */
#define UPC1_CRA_GPCSMASK_MASK	0xe0 /* how many GPCSBASE bits matter */
#define UPC1_CRA_GPCSMASK_SHIFT	1
#define UPC1_CRA_GPCSA1		0x08 /* Extra bit at the bottom of GPCSBASE */
#define UPC1_CRA_IDEBEN		0x04 /* IDEENLO* buffer enable */
#define UPC1_CRA_GPCSEN		0x02 /* GPCS enable */
#define UPC1_CRA_GPCSBEN	0x01 /* GPCS buffer enable */

/* 82C710 configuration register 0x0b */
#define UPC1_CRB_MINTRP		0x80 /* Mouse interrupt polarity: 1->low */
#define UPC1_CRB_FINTRP		0x40 /* Floppy interrupt polarity */
#define UPC1_CRB_SINTRP		0x20 /* Serial interrupt polarity */
#define UPC1_CRB_PINTRP		0x10 /* Parallel interrupt polarity */
#define UPC1_CRB_SPDWN		0x08 /* Serial port power down */
#define UPC1_CRB_PPWDN		0x02 /* Parallel port power down */
#define UPC1_CRB_GPCSOUT	0x01 /* GPCS*/OUT1 pin function select */

/* 82C710 configuration register 0x0c */
#define UPC1_CRC_IDEEN		0x80 /* IDE enable */
#define UPC1_CRC_IDEATXT	0x40 /* IDE AT/XT select */
#define UPC1_CRC_FDCEN		0x20 /* FDC enable */
#define UPC1_CRC_FPWDN		0x10 /* FDC power down */
#define UPC1_CRC_RTCCSEN	0x08 /* RTCCS* enable */
#define UPC1_CRC_RTCBEN		0x04 /* RTCCS* buffer enable */
#define UPC1_CRC_MDDWN		0x01 /* Mouse port power down */

/* 82C710 configuration register 0x0d */
#define UPC1_MOUSEBASE_SHIFT	2

/* 82C710 configuration register 0x0e */
#define UPC1_CRE_STEN		0x40 /* Serial port test enabled */
#define UPC1_CRE_FTEN1		0x10 /* Floppy test bit 1 */
#define UPC1_CRE_FTEN2		0x80 /* Floppy test bit 2 */
#define UPC1_CRE_DSTEN		0x40

/* 82C710 configuration register 0x0f */
#define UPC1_CONFBASE_SHIFT	2

/* 82C711/721 Configuration magic sequences */

#define UPC2_PORT_CFGADDR	0x3f0 /* Configuration register address */
#define UPC2_PORT_CFGDATA	0x3f1 /* Configuration register value */
#define UPC2_CFGMAGIC_ENTER	0x55 /* Write twice to enter config mode. */
#define UPC2_CFGMAGIC_EXIT	0xaa /* Write once to exit config mode. */

/* Configuration registers */
#define UPC2_CFGADDR_CR0	0x00 /* Configuration Register 0 */
#define UPC2_CFGADDR_CR1	0x01 /* Configuration Register 1 */
#define UPC2_CFGADDR_CR2	0x02 /* Configuration Register 2 */
#define UPC2_CFGADDR_CR3	0x03 /* Configuration Register 3 */
#define UPC2_CFGADDR_CR4	0x04 /* Configuration Register 4 */

/* Configuration register 0 */
#define UPC2_CR0_VALID		0x80 /* Device has been configured */
#define UPC2_CR0_OSC_MASK	0x60 /* Oscillator control */
#define UPC2_CR0_OSC_ON		0x00 /* Oscillator always on */
#define UPC2_CR0_OSC_PWRGD	0x20 /* Oscillator on when PWRGD */
#define UPC2_CR0_OSC_OFF	0x60 /* Oscillator always off */
#define UPC2_CR0_FDC_ENABLE	0x10 /* FDC enabled */
#define UPC2_CR0_FDC_ON		0x08 /* FDC powered */
#define UPC2_CR0_IDE_AT		0x02 /* IDE controller is AT type */
#define UPC2_CR0_IDE_ENABLE	0x01 /* IDE controller enabled */

/* Configuration register 1 */
#define UPC2_CR1_READ_ENABLE	0x80 /* Enable reading of config regs */
#define UPC2_CR1_COM34_MASK	0x60 /* COM3/COM4 addresses */
#define UPC2_CR1_COM34_338_238	0x00 /* COM3 = 0x338; COM4 = 0x238 */
#define UPC2_CR1_COM34_3E8_2E8	0x20 /* COM3 = 0x3E8; COM4 = 0x2E8 */
#define UPC2_CR1_COM34_2E8_2E0	0x40 /* COM3 = 0x2E8; COM4 = 0x2E0 */
#define UPC2_CR1_COM34_220_228	0x60 /* COM3 = 0x220; COM4 = 0x228 */
#define UPC2_CR1_IRQ_ACTHIGH	0x10 /* IRQ is active-high */
#define UPC2_CR1_LPT_BORING	0x08 /* Parallel port is not EPP */
#define UPC2_CR1_LPT_ON		0x04 /* Parallel port is powered */
#define UPC2_CR1_LPT_MASK	0x03 /* Parallel port address */
#define UPC2_CR1_LPT_DISABLE	0x00 /* Parallel port disabled */
#define UPC2_CR1_LPT_3BC		0x01 /* Parallel port at 0x3BC */
#define UPC2_CR1_LPT_378		0x02 /* Parallel port at 0x378 */
#define UPC2_CR1_LPT_278		0x03 /* Parallel port at 0x278 */

/* Configuration register 2 */
#define UPC2_CR2_UART2_ON	0x80 /* 2ndary serial powered */
#define UPC2_CR2_UART2_ENABLE	0x40 /* 2ndary serial enabled */
#define UPC2_CR2_UART2_MASK	0x30 /* 2ndary serial address */
#define UPC2_CR2_UART2_3F8	0x00 /* 2ndary serial at 0x3F8 */
#define UPC2_CR2_UART2_2F8	0x10 /* 2ndary serial at 0x2F8 */
#define UPC2_CR2_UART2_COM3	0x20 /* 2ndary serial at COM3 (see CR1) */
#define UPC2_CR2_UART2_COM4	0x30 /* 2ndary serial at COM4 (see CR1) */
#define UPC2_CR2_UART1_ON	0x08 /* primary serial powered */
#define UPC2_CR2_UART1_ENABLE	0x04 /* primary serial enabled */
#define UPC2_CR2_UART1_MASK	0x03 /* primary serial address */
#define UPC2_CR2_UART1_3F8	0x00 /* primary serial at 0x3F8 */
#define UPC2_CR2_UART1_2F8	0x01 /* primary serial at 0x2F8 */
#define UPC2_CR2_UART1_COM3	0x02 /* primary serial at COM3 (see CR1) */
#define UPC2_CR2_UART1_COM4	0x03 /* primary serial at COM4 (see CR1) */

/* Configuration register 3 */
#define UPC2_CR3_UART2_TEST	0x80 /* 2ndary serial test mode */
#define UPC2_CR3_UART1_TEST	0x40 /* primary serial test mode */
#define UPC2_CR3_FDC_TEST_MASK	0x30 /* FDC test modes */
#define UPC2_CR3_FDC_TEST_NORMAL	0x00 /* FDC normal mode */

/* Configuration register 4 (82C721 only) */
#define UPC2_CR4_UART2_DIV13	0x01 /* Use normal (cf MIDI) clock for UART2 */
