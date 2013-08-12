#ifndef _OMAP_I2C_REGISTERS_H
#define _OMAP_I2C_REGISTERS_H

/* I2C Addresses for am335x (BeagleBone White / BeagleBone Black) */

/* IRQ Numbers */
#define AM335X_I2C0_IRQ 70
#define AM335X_I2C1_IRQ 71
#define AM335X_I2C2_IRQ 30

/* Base Addresses */
#define AM335X_I2C0_BASE 0x44e0b000
#define AM335X_I2C1_BASE 0x4802a000
#define AM335X_I2C2_BASE 0x4819c000

/* Size of I2C Register Address Range */
#define AM335X_I2C0_SIZE 0x1000
#define AM335X_I2C1_SIZE 0x1000
#define AM335X_I2C2_SIZE 0x1000

/* Register Offsets */
#define AM335X_I2C_REVNB_LO        0x00
#define AM335X_I2C_REVNB_HI        0x04
#define AM335X_I2C_SYSC            0x10
#define AM335X_I2C_IRQSTATUS_RAW   0x24
#define AM335X_I2C_IRQSTATUS       0x28
#define AM335X_I2C_IRQENABLE_SET   0x2c
#define AM335X_I2C_IRQENABLE_CLR   0x30
#define AM335X_I2C_WE              0x34
#define AM335X_I2C_DMARXENABLE_SET 0x38
#define AM335X_I2C_DMATXENABLE_SET 0x3c
#define AM335X_I2C_DMARXENABLE_CLR 0x40
#define AM335X_I2C_DMATXENABLE_CLR 0x44
#define AM335X_I2C_DMARXWAKE_EN    0x48
#define AM335X_I2C_DMATXWAKE_EN    0x4c
#define AM335X_I2C_SYSS            0x90
#define AM335X_I2C_BUF             0x94
#define AM335X_I2C_CNT             0x98
#define AM335X_I2C_DATA            0x9c
#define AM335X_I2C_CON             0xa4
#define AM335X_I2C_OA              0xa8
#define AM335X_I2C_SA              0xac
#define AM335X_I2C_PSC             0xb0
#define AM335X_I2C_SCLL            0xb4
#define AM335X_I2C_SCLH            0xb8
#define AM335X_I2C_SYSTEST         0xbc
#define AM335X_I2C_BUFSTAT         0xc0
#define AM335X_I2C_OA1             0xc4
#define AM335X_I2C_OA2             0xc8
#define AM335X_I2C_OA3             0xcc
#define AM335X_I2C_ACTOA           0xd0
#define AM335X_I2C_SBLOCK          0xd4

/* Constants */
#define AM335X_FUNCTIONAL_CLOCK 96000000 /* 96 MHz */
#define AM335X_MODULE_CLOCK 12000000	/* 12 MHz */

/* I2C_REV value found on the BeagleBone / BeagleBone Black */
#define AM335X_REV_MAJOR 0x00
#define AM335X_REV_MINOR 0x0b

/* I2C Addresses for dm37xx (BeagleBoard-xM) */

/* IRQ Numbers */
#define DM37XX_I2C0_IRQ 56
#define DM37XX_I2C1_IRQ 57
#define DM37XX_I2C2_IRQ 61

/* Base Addresses */
#define DM37XX_I2C0_BASE 0x48070000
#define DM37XX_I2C1_BASE 0x48072000
#define DM37XX_I2C2_BASE 0x48060000

/* Size of I2C Register Address Range */
#define DM37XX_I2C0_SIZE 0x1000
#define DM37XX_I2C1_SIZE 0x1000
#define DM37XX_I2C2_SIZE 0x1000

/* Register Offsets */
#define DM37XX_I2C_REV     0x00
#define DM37XX_I2C_IE      0x04
#define DM37XX_I2C_STAT    0x08
#define DM37XX_I2C_WE      0x0C
#define DM37XX_I2C_SYSS    0x10
#define DM37XX_I2C_BUF     0x14
#define DM37XX_I2C_CNT     0x18
#define DM37XX_I2C_DATA    0x1c
#define DM37XX_I2C_SYSC    0x20
#define DM37XX_I2C_CON     0x24
#define DM37XX_I2C_OA0     0x28
#define DM37XX_I2C_SA      0x2c
#define DM37XX_I2C_PSC     0x30
#define DM37XX_I2C_SCLL    0x34
#define DM37XX_I2C_SCLH    0x38
#define DM37XX_I2C_SYSTEST 0x3c
#define DM37XX_I2C_BUFSTAT 0x40
#define DM37XX_I2C_OA1     0x44
#define DM37XX_I2C_OA2     0x48
#define DM37XX_I2C_OA3     0x4c
#define DM37XX_I2C_ACTOA   0x50
#define DM37XX_I2C_SBLOCK  0x54

/* Constants */
#define DM37XX_FUNCTIONAL_CLOCK 96000000 /* 96 MHz */
#define DM37XX_MODULE_CLOCK 19200000	/* 19.2 MHz */

#define DM37XX_REV_MAJOR 0x04
#define DM37XX_REV_MINOR 0x00

/* Shared Values */

#define BUS_SPEED_100KHz 100000	/* 100 KHz */
#define BUS_SPEED_400KHz 400000	/* 400 KHz */
#define I2C_OWN_ADDRESS 0x01

/* Masks */

#define MAX_I2C_SA_MASK (0x3ff)	/* Highest 10 bit address -- 9..0 */

/* Bit Offsets within Registers (only those used are listed) */

/* Same offsets for both dm37xx and am335x */

#define I2C_EN 15 /* I2C_CON */
#define MST    10 /* I2C_CON */
#define TRX	9 /* I2C_CON */
#define XSA     8 /* I2C_CON */
#define STP     1 /* I2C_CON */
#define STT     0 /* I2C_CON */

#define CLKACTIVITY_S 9 /* I2C_SYSC */
#define CLKACTIVITY_I 8 /* I2C_SYSC */
#define SMART_WAKE_UP 4 /* I2C_SYSC */
#define NO_IDLE_MODE 3 /* I2C_SYSC */
#define SRST     1 /* I2C_SYSC */
#define AUTOIDLE 0 /* I2C_SYSC */

#define RDONE 0 /* I2C_SYSS */

#define RXFIFO_CLR 14 /* I2C_BUF */
#define TXFIFO_CLR  6 /* I2C_BUF */

#define BB   12 /* I2C_IRQSTATUS / I2C_STAT / I2C_IRQENABLE_SET / I2C_IE */
#define ROVR 11 /* I2C_IRQSTATUS / I2C_STAT / I2C_IRQENABLE_SET / I2C_IE */
#define AERR  7 /* I2C_IRQSTATUS / I2C_STAT / I2C_IRQENABLE_SET / I2C_IE */
#define XRDY  4 /* I2C_IRQSTATUS / I2C_STAT / I2C_IRQENABLE_SET / I2C_IE */
#define RRDY  3 /* I2C_IRQSTATUS / I2C_STAT / I2C_IRQENABLE_SET / I2C_IE */
#define ARDY  2 /* I2C_IRQSTATUS / I2C_STAT / I2C_IRQENABLE_SET / I2C_IE */
#define NACK  1 /* I2C_IRQSTATUS / I2C_STAT / I2C_IRQENABLE_SET / I2C_IE */
#define AL    0 /* I2C_IRQSTATUS / I2C_STAT / I2C_IRQENABLE_SET / I2C_IE */

#endif /* _OMAP_I2C_REGISTERS_H */
