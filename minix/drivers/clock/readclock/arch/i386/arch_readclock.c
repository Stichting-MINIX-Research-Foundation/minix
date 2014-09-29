/* readclock - read the real time clock		Authors: T. Holm & E. Froese
 *
 * Changed to be user-space driver.
 */

/************************************************************************/
/*									*/
/*   readclock.c							*/
/*									*/
/*		Read the clock value from the 64 byte CMOS RAM		*/
/*		area, then set system time.				*/
/*									*/
/*		If the machine ID byte is 0xFC or 0xF8, the device	*/
/*		/dev/mem exists and can be opened for reading,		*/
/*		and no errors in the CMOS RAM are reported by the	*/
/*		RTC, then the time is read from the clock RAM		*/
/*		area maintained by the RTC.				*/
/*									*/
/*		The clock RAM values are decoded and fed to mktime	*/
/*		to make a time_t value, then stime(2) is called.	*/
/*									*/
/*		This fails if:						*/
/*									*/
/*		If the machine ID does not match 0xFC or 0xF8 (no	*/
/*		error message.)						*/
/*									*/
/*		If the machine ID is 0xFC or 0xF8 and /dev/mem		*/
/*		is missing, or cannot be accessed.			*/
/*									*/
/*		If the RTC reports errors in the CMOS RAM.		*/
/*									*/
/************************************************************************/
/*    origination          1987-Dec-29              efth                */
/*    robustness	   1990-Oct-06		    C. Sylvain		*/
/* incorp. B. Evans ideas  1991-Jul-06		    C. Sylvain		*/
/*    set time & calibrate 1992-Dec-17		    Kees J. Bot		*/
/*    clock timezone	   1993-Oct-10		    Kees J. Bot		*/
/*    set CMOS clock	   1994-Jun-12		    Kees J. Bot		*/
/************************************************************************/

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/com.h>
#include <minix/log.h>
#include <machine/cmos.h>

#include "readclock.h"

#define MACH_ID_ADDR	0xFFFFE	/* BIOS Machine ID at FFFF:000E */

#define PC_AT		   0xFC	/* Machine ID byte for PC/AT,
				 * PC/XT286, and PS/2 Models 50, 60 */
#define PS_386		   0xF8	/* Machine ID byte for PS/2 Model 80 */

/* Manufacturers usually use the ID value of the IBM model they emulate.
 * However some manufacturers, notably HP and COMPAQ, have had different
 * ideas in the past.
 *
 * Machine ID byte information source:
 *	_The Programmer's PC Sourcebook_ by Thom Hogan,
 *	published by Microsoft Press
 */

/* used for logging */
static struct log log = {
	.name = "cmos_clock",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static int read_register(int reg_addr);
static int write_register(int reg_addr, int value);

static int arch_init(void);
static int arch_get_time(struct tm *t, int flags);
static int arch_set_time(struct tm *t, int flags);
static int arch_pwr_off(void);
static void arch_exit(void);

int
arch_setup(struct rtc *r)
{
	r->init = arch_init;
	r->get_time = arch_get_time;
	r->set_time = arch_set_time;
	r->pwr_off = arch_pwr_off;
	r->exit = arch_exit;

	return OK;
}

static int
arch_init(void)
{
	int s;
	unsigned char mach_id, cmos_state;

	if ((s = sys_readbios(MACH_ID_ADDR, &mach_id, sizeof(mach_id))) != OK) {
		log_warn(&log, "sys_readbios failed: %d.\n", s);

		return -1;
	}

	if (mach_id != PS_386 && mach_id != PC_AT) {
		log_warn(&log, "Machine ID unknown.");
		log_warn(&log, "Machine ID byte = %02x\n", mach_id);

		return -1;
	}

	cmos_state = read_register(CMOS_STATUS);

	if (cmos_state & (CS_LOST_POWER | CS_BAD_CHKSUM | CS_BAD_TIME)) {
		log_warn(&log, "CMOS RAM error(s) found...");
		log_warn(&log, "CMOS state = 0x%02x\n", cmos_state);

		if (cmos_state & CS_LOST_POWER)
			log_warn(&log,
			    "RTC lost power. Reset CMOS RAM with SETUP.");
		if (cmos_state & CS_BAD_CHKSUM)
			log_warn(&log, "CMOS RAM checksum is bad. Run SETUP.");
		if (cmos_state & CS_BAD_TIME)
			log_warn(&log,
			    "Time invalid in CMOS RAM. Reset clock.");
		return -1;
	}

	return OK;
}

/***********************************************************************/
/*                                                                     */
/*    arch_get_time( time )                                            */
/*                                                                     */
/*    Update the structure pointed to by time with the current time    */
/*    as read from CMOS RAM of the RTC.				       */
/*    If necessary, the time is converted into a binary format before  */
/*    being stored in the structure.                                   */
/*                                                                     */
/***********************************************************************/

static int
arch_get_time(struct tm *t, int flags)
{
	int osec, n;

	do {
		osec = -1;
		n = 0;
		do {
			/* Clock update in progress? */
			if (read_register(RTC_REG_A) & RTC_A_UIP)
				continue;

			t->tm_sec = read_register(RTC_SEC);
			if (t->tm_sec != osec) {
				/* Seconds changed.  First from -1, then because the
				 * clock ticked, which is what we're waiting for to
				 * get a precise reading.
				 */
				osec = t->tm_sec;
				n++;
			}
		} while (n < 2);

		/* Read the other registers. */
		t->tm_min = read_register(RTC_MIN);
		t->tm_hour = read_register(RTC_HOUR);
		t->tm_mday = read_register(RTC_MDAY);
		t->tm_mon = read_register(RTC_MONTH);
		t->tm_year = read_register(RTC_YEAR);

		/* Time stable? */
	} while (read_register(RTC_SEC) != t->tm_sec
	    || read_register(RTC_MIN) != t->tm_min
	    || read_register(RTC_HOUR) != t->tm_hour
	    || read_register(RTC_MDAY) != t->tm_mday
	    || read_register(RTC_MONTH) != t->tm_mon
	    || read_register(RTC_YEAR) != t->tm_year);

	if ((read_register(RTC_REG_B) & RTC_B_DM_BCD) == 0) {
		/* Convert BCD to binary (default RTC mode). */
		t->tm_year = bcd_to_dec(t->tm_year);
		t->tm_mon = bcd_to_dec(t->tm_mon);
		t->tm_mday = bcd_to_dec(t->tm_mday);
		t->tm_hour = bcd_to_dec(t->tm_hour);
		t->tm_min = bcd_to_dec(t->tm_min);
		t->tm_sec = bcd_to_dec(t->tm_sec);
	}
	t->tm_mon--;		/* Counts from 0. */

	/* Correct the year, good until 2080. */
	if (t->tm_year < 80)
		t->tm_year += 100;

	if ((flags & RTCDEV_Y2KBUG) == RTCDEV_Y2KBUG) {
		/* Clock with Y2K bug, interpret 1980 as 2000, good until 2020. */
		if (t->tm_year < 100)
			t->tm_year += 20;
	}

	return OK;
}

static int
read_register(int reg_addr)
{
	u32_t r;

	if (sys_outb(RTC_INDEX, reg_addr) != OK) {
		log_warn(&log, "outb failed of %x\n", RTC_INDEX);
		return -1;
	}
	if (sys_inb(RTC_IO, &r) != OK) {
		log_warn(&log, "inb failed of %x (index %x) failed\n", RTC_IO,
		    reg_addr);
		return -1;
	}
	return r;
}

/***********************************************************************/
/*                                                                     */
/*    arch_set_time( time )                                            */
/*                                                                     */
/*    Set the CMOS RTC to the time found in the structure.             */
/*                                                                     */
/***********************************************************************/

static int
arch_set_time(struct tm *t, int flags)
{
	int regA, regB;

	if ((flags & RTCDEV_CMOSREG) == RTCDEV_CMOSREG) {
		/* Set A and B registers to their proper values according to the AT
		 * reference manual.  (For if it gets messed up, but the BIOS doesn't
		 * repair it.)
		 */
		write_register(RTC_REG_A, RTC_A_DV_OK | RTC_A_RS_DEF);
		write_register(RTC_REG_B, RTC_B_24);
	}

	/* Inhibit updates. */
	regB = read_register(RTC_REG_B);
	write_register(RTC_REG_B, regB | RTC_B_SET);

	t->tm_mon++;		/* Counts from 1. */

	if ((flags & RTCDEV_Y2KBUG) == RTCDEV_Y2KBUG) {
		/* Set the clock back 20 years to avoid Y2K bug, good until 2020. */
		if (t->tm_year >= 100)
			t->tm_year -= 20;
	}

	if ((regB & 0x04) == 0) {
		/* Convert binary to BCD (default RTC mode) */
		t->tm_year = dec_to_bcd(t->tm_year % 100);
		t->tm_mon = dec_to_bcd(t->tm_mon);
		t->tm_mday = dec_to_bcd(t->tm_mday);
		t->tm_hour = dec_to_bcd(t->tm_hour);
		t->tm_min = dec_to_bcd(t->tm_min);
		t->tm_sec = dec_to_bcd(t->tm_sec);
	}
	write_register(RTC_YEAR, t->tm_year);
	write_register(RTC_MONTH, t->tm_mon);
	write_register(RTC_MDAY, t->tm_mday);
	write_register(RTC_HOUR, t->tm_hour);
	write_register(RTC_MIN, t->tm_min);
	write_register(RTC_SEC, t->tm_sec);

	/* Stop the clock. */
	regA = read_register(RTC_REG_A);
	write_register(RTC_REG_A, regA | RTC_A_DV_STOP);

	/* Allow updates and restart the clock. */
	write_register(RTC_REG_B, regB);
	write_register(RTC_REG_A, regA);

	return OK;
}

static int
write_register(int reg_addr, int value)
{
	if (sys_outb(RTC_INDEX, reg_addr) != OK) {
		log_warn(&log, "outb failed of %x\n", RTC_INDEX);
		return -1;
	}
	if (sys_outb(RTC_IO, value) != OK) {
		log_warn(&log, "outb failed of %x (index %x)\n", RTC_IO,
		    reg_addr);
		return -1;
	}

	return OK;
}

static int
arch_pwr_off(void)
{
	/* Not Implemented */
	return ENOSYS;
}

static void
arch_exit(void)
{
	/* Nothing to clean up here */
	log_debug(&log, "Exiting...");
}

