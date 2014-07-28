#ifndef __TPS65950_H
#define __TPS65950_H

/*
 * The chip is so huge it's split into multiple sub-modules, each with 256
 * byte register maps on it's own slave addresses. Use ID1, ID2, ... to
 * specify which sub-module to communicate with. Each slave address can have
 * registers for one or more peripherals.
 */

/* Slave Address 0x48 (USB) */
#define ID1 0

/* Slave Address 0x49 (AUDIO_VOICE/GPIO/INTBR/PIH/TEST) */
#define ID2 1

/* Interface Bit Registers (INTBR) */

/* Chip Identifier
 *
 * When combined to uint32_t, these are the subfields:
 *   [31:28] - Silicon version number
 *   [27:12] - Part code number
 *   [11: 1] - Manufacturer code
 *   [    0] - Least-significant bit (LSB)
 */
#define IDCODE_7_0_REG 0x85
#define IDCODE_15_8_REG 0x86
#define IDCODE_23_16_REG 0x87
#define IDCODE_31_24_REG 0x88

#define IDCODE_REV_1_0 0x00009002f
#define IDCODE_REV_1_1 0x01009002f
#define IDCODE_REV_1_2 0x03009002f

/* Write 0x49 to UNLOCK_TEST_REG to unlock reading of IDCODE and DIEID */
#define UNLOCK_TEST_REG 0x97
#define UNLOCK_TEST_CODE 0x49


/* Slave Address 0x4a (BCI/MADC/MAIN_CHARGE/PWM01/LED/PWMAB/KEYPAD) */
#define ID3 2


/* Slave Address 0x4b (BACKUP_REG/INT/PM_MASTER/SECURED_REG) */
#define ID4 3

/*
 * Real Time Clock
 */

#define SECONDS_REG 0x1c
#define MINUTES_REG 0x1d
#define HOURS_REG 0x1e
#define DAYS_REG 0x1f
#define MONTHS_REG 0x20
#define YEARS_REG 0x21

#define RTC_CTRL_REG 0x00000029
#define GET_TIME_BIT 6
#define STOP_RTC_BIT 0

#define RTC_STATUS_REG 0x0000002A
#define RUN_BIT 1

#define NADDRESSES 4

extern endpoint_t bus_endpoint;
extern i2c_addr_t addresses[NADDRESSES];

#endif /* __TPS65950_H */
