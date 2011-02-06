/*
ibm/cmos.h

Created:	Dec 1998 by Philip Homburg <philip@cs.vu.nl>

Definitions for the CMOS/realtime clock. Based on the datasheet for the
Dallas DS12887, compatible with the Motorola MC146818
*/

#define RTC_INDEX	0x70	/* Bit 7 = NMI enable (1) / disable (0)
				 * bits 0..6 index
				 */
#define RTC_IO		0x71	/* Data register, 
				 * Note: the operation following a write to
				 * RTC_INDEX should an access (read or write)
				 * to RTC_IO
				 */

#define RTC_SEC		0x0	/* Seconds register */
#define RTC_SEC_ALRM	0x1	/* Seconds register for alarm */
#define RTC_MIN		0x2	/* Minutes register */
#define RTC_MIN_ALRM	0x3	/* Minutes register for alarm */
#define RTC_HOUR	0x4	/* Hours register */
#define RTC_HOUR_ALRM	0x5	/* Hours register for alarm */
#define RTC_WDAY	0x6	/* Day of the week, 1..7, Sunday = 1 */
#define RTC_MDAY	0x7	/* Day of the month, 1..31 */
#define RTC_MONTH	0x8	/* Month, 1..12 */
#define RTC_YEAR	0x9	/* Year, 0..99 */
#define RTC_REG_A	0xA
#define		RTC_A_UIP	0x80	/* Update in progress. When clear,
					 * no update will occur for 244
					 * micro seconds.
					 */
#define		RTC_A_DV	0x70	/* Divider bits, valid values are: */
#define		    RTC_A_DV_OK	    0x20	/* Normal */
#define		    RTC_A_DV_STOP   0x70	/* Stop, a re-start starts
						 * halfway through a cycle,
						 * i.e. the update occurs after
						 * 500ms.
						 */
#define		RTC_A_RS	0x0F	/* Int. freq */
				    /*  0	None 
				     *  1	 256 Hz
				     *  2	 128 Hz
				     *  3	8192 Hz
				     *  4	4096 Hz
				     *  5	2048 Hz
				     *  6	1024 Hz
				     *  7	 512 Hz
				     *  8	 256 Hz
				     *  9	 128 Hz
				     * 10	  64 Hz
				     * 11	  32 Hz
				     * 12	  16 Hz
				     * 13	   8 Hz
				     * 14	   4 Hz
				     * 15	   2 Hz
				     */
#define		    RTC_A_RS_DEF    6	/* Default freq. */
#define RTC_REG_B	0xB
#define		RTC_B_SET	0x80	/* Inhibit updates */
#define		RTC_B_PIE	0x40	/* Enable periodic interrupts */
#define		RTC_B_AIE	0x20	/* Enable alarm interrupts */
#define		RTC_B_UIE	0x10	/* Enable update ended interrupts */
#define		RTC_B_SQWE	0x08	/* Enable square wave output */
#define		RTC_B_DM_BCD	0x04	/* Data is in BCD (otherwise binary) */
#define		RTC_B_24	0x02	/* Count hours in 24-hour mode */
#define		RTC_B_DSE	0x01	/* Automatic (wrong) daylight savings
					 * updates
					 */
#define RTC_REG_C	0xC

/* Contents of the general purpose CMOS RAM (source IBM reference manual) */
#define CMOS_STATUS	0xE
#define		CS_LOST_POWER	0x80	/* Chip lost power */
#define		CS_BAD_CHKSUM	0x40	/* Checksum is incorrect */
#define		CS_BAD_CONFIG	0x20	/* Bad configuration info */
#define		CS_BAD_MEMSIZE	0x10	/* Wrong memory size of CMOS */
#define		CS_BAD_HD	0x08	/* Harddisk failed */
#define		CS_BAD_TIME	0x04	/* CMOS time is invalid */
					/* bits 0 and 1 are reserved */

/*
 * $PchId: cmos.h,v 1.1 1998/12/16 09:14:21 philip Exp $
 */
