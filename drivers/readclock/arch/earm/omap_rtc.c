#include <minix/syslib.h>
#include <minix/drvlib.h>
#include <minix/log.h>
#include <minix/mmio.h>
#include <minix/clkconf.h>
#include <minix/sysutil.h>
#include <minix/board.h>

#include <sys/mman.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include "omap_rtc.h"
#include "readclock.h"

/* defines the set of register */

typedef struct omap_rtc_registers
{
	vir_bytes RTC_SS_SECONDS_REG;
	vir_bytes RTC_SS_MINUTES_REG;
	vir_bytes RTC_SS_HOURS_REG;
	vir_bytes RTC_SS_DAYS_REG;
	vir_bytes RTC_SS_MONTHS_REG;
	vir_bytes RTC_SS_YEARS_REG;
	vir_bytes RTC_SS_WEEKS_REG;
	vir_bytes RTC_SS_ALARM_SECONDS_REG;
	vir_bytes RTC_SS_ALARM_MINUTES_REG;
	vir_bytes RTC_SS_ALARM_HOURS_REG;
	vir_bytes RTC_SS_ALARM_DAYS_REG;
	vir_bytes RTC_SS_ALARM_MONTHS_REG;
	vir_bytes RTC_SS_ALARM_YEARS_REG;
	vir_bytes RTC_SS_RTC_CTRL_REG;
	vir_bytes RTC_SS_RTC_STATUS_REG;
	vir_bytes RTC_SS_RTC_INTERRUPTS_REG;
	vir_bytes RTC_SS_RTC_COMP_LSB_REG;
	vir_bytes RTC_SS_RTC_COMP_MSB_REG;
	vir_bytes RTC_SS_RTC_OSC_REG;
	vir_bytes RTC_SS_RTC_SCRATCH0_REG;
	vir_bytes RTC_SS_RTC_SCRATCH1_REG;
	vir_bytes RTC_SS_RTC_SCRATCH2_REG;
	vir_bytes RTC_SS_KICK0R;
	vir_bytes RTC_SS_KICK1R;
	vir_bytes RTC_SS_RTC_REVISION;
	vir_bytes RTC_SS_RTC_SYSCONFIG;
	vir_bytes RTC_SS_RTC_IRQWAKEEN;
	vir_bytes RTC_SS_ALARM2_SECONDS_REG;
	vir_bytes RTC_SS_ALARM2_MINUTES_REG;
	vir_bytes RTC_SS_ALARM2_HOURS_REG;
	vir_bytes RTC_SS_ALARM2_DAYS_REG;
	vir_bytes RTC_SS_ALARM2_MONTHS_REG;
	vir_bytes RTC_SS_ALARM2_YEARS_REG;
	vir_bytes RTC_SS_RTC_PMIC;
	vir_bytes RTC_SS_RTC_DEBOUNCE;
} omap_rtc_registers_t;

typedef struct omap_rtc_clock
{
	enum rtc_clock_type
	{ am335x } clock_type;
	phys_bytes mr_base;
	phys_bytes mr_size;
	vir_bytes mapped_addr;
	omap_rtc_registers_t *regs;
} omap_rtc_clock_t;

/* Define the registers for each chip */

static omap_rtc_registers_t am335x_rtc_regs = {
	.RTC_SS_SECONDS_REG = AM335X_RTC_SS_SECONDS_REG,
	.RTC_SS_MINUTES_REG = AM335X_RTC_SS_MINUTES_REG,
	.RTC_SS_HOURS_REG = AM335X_RTC_SS_HOURS_REG,
	.RTC_SS_DAYS_REG = AM335X_RTC_SS_DAYS_REG,
	.RTC_SS_MONTHS_REG = AM335X_RTC_SS_MONTHS_REG,
	.RTC_SS_YEARS_REG = AM335X_RTC_SS_YEARS_REG,
	.RTC_SS_WEEKS_REG = AM335X_RTC_SS_WEEKS_REG,
	.RTC_SS_ALARM_SECONDS_REG = AM335X_RTC_SS_ALARM_SECONDS_REG,
	.RTC_SS_ALARM_MINUTES_REG = AM335X_RTC_SS_ALARM_MINUTES_REG,
	.RTC_SS_ALARM_HOURS_REG = AM335X_RTC_SS_ALARM_HOURS_REG,
	.RTC_SS_ALARM_DAYS_REG = AM335X_RTC_SS_ALARM_DAYS_REG,
	.RTC_SS_ALARM_MONTHS_REG = AM335X_RTC_SS_ALARM_MONTHS_REG,
	.RTC_SS_ALARM_YEARS_REG = AM335X_RTC_SS_ALARM_YEARS_REG,
	.RTC_SS_RTC_CTRL_REG = AM335X_RTC_SS_RTC_CTRL_REG,
	.RTC_SS_RTC_STATUS_REG = AM335X_RTC_SS_RTC_STATUS_REG,
	.RTC_SS_RTC_INTERRUPTS_REG = AM335X_RTC_SS_RTC_INTERRUPTS_REG,
	.RTC_SS_RTC_COMP_LSB_REG = AM335X_RTC_SS_RTC_COMP_LSB_REG,
	.RTC_SS_RTC_COMP_MSB_REG = AM335X_RTC_SS_RTC_COMP_MSB_REG,
	.RTC_SS_RTC_OSC_REG = AM335X_RTC_SS_RTC_OSC_REG,
	.RTC_SS_RTC_SCRATCH0_REG = AM335X_RTC_SS_RTC_SCRATCH0_REG,
	.RTC_SS_RTC_SCRATCH1_REG = AM335X_RTC_SS_RTC_SCRATCH1_REG,
	.RTC_SS_RTC_SCRATCH2_REG = AM335X_RTC_SS_RTC_SCRATCH2_REG,
	.RTC_SS_KICK0R = AM335X_RTC_SS_KICK0R,
	.RTC_SS_KICK1R = AM335X_RTC_SS_KICK1R,
	.RTC_SS_RTC_REVISION = AM335X_RTC_SS_RTC_REVISION,
	.RTC_SS_RTC_SYSCONFIG = AM335X_RTC_SS_RTC_SYSCONFIG,
	.RTC_SS_RTC_IRQWAKEEN = AM335X_RTC_SS_RTC_IRQWAKEEN,
	.RTC_SS_ALARM2_SECONDS_REG = AM335X_RTC_SS_ALARM2_SECONDS_REG,
	.RTC_SS_ALARM2_MINUTES_REG = AM335X_RTC_SS_ALARM2_MINUTES_REG,
	.RTC_SS_ALARM2_HOURS_REG = AM335X_RTC_SS_ALARM2_HOURS_REG,
	.RTC_SS_ALARM2_DAYS_REG = AM335X_RTC_SS_ALARM2_DAYS_REG,
	.RTC_SS_ALARM2_MONTHS_REG = AM335X_RTC_SS_ALARM2_MONTHS_REG,
	.RTC_SS_ALARM2_YEARS_REG = AM335X_RTC_SS_ALARM2_YEARS_REG,
	.RTC_SS_RTC_PMIC = AM335X_RTC_SS_RTC_PMIC,
	.RTC_SS_RTC_DEBOUNCE = AM335X_RTC_SS_RTC_DEBOUNCE
};

static omap_rtc_clock_t rtc = {
	am335x, AM335X_RTC_SS_BASE, AM335X_RTC_SS_SIZE, 0, &am335x_rtc_regs
};

/* used for logging */
static struct log log = {
	.name = "omap_rtc",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static u32_t use_count = 0;
static u32_t pwr_off_in_progress = 0;

static void omap_rtc_unlock(void);
static void omap_rtc_lock(void);
static int omap_rtc_clkconf(void);

/* Helper Functions for Register Access */
#define reg_read(a) (*(volatile uint32_t *)(rtc.mapped_addr + a))
#define reg_write(a,v) (*(volatile uint32_t *)(rtc.mapped_addr + a) = (v))
#define reg_set_bit(a,v) reg_write((a), reg_read((a)) | (1<<v))
#define reg_clear_bit(a,v) reg_write((a), reg_read((a)) & ~(1<<v))
#define RTC_IS_BUSY (reg_read(rtc.regs->RTC_SS_RTC_STATUS_REG) & (1<<RTC_BUSY_BIT))

/* When the RTC is running, writes should not happen when the RTC is busy.
 * This macro waits until the RTC is free before doing the write.
 */
#define safe_reg_write(a,v) do { while (RTC_IS_BUSY) {micro_delay(1);} reg_write((a),(v)); } while (0)
#define safe_reg_set_bit(a,v) safe_reg_write((a), reg_read((a)) | (1<<v))
#define safe_reg_clear_bit(a,v) safe_reg_write((a), reg_read((a)) & ~(1<<v))

static void
omap_rtc_unlock(void)
{
	/* Specific bit patterns need to be written to specific registers in a 
	 * specific order to enable writing to RTC_SS registers. 
	 */
	reg_write(rtc.regs->RTC_SS_KICK0R, AM335X_RTC_SS_KICK0R_UNLOCK_MASK);
	reg_write(rtc.regs->RTC_SS_KICK1R, AM335X_RTC_SS_KICK1R_UNLOCK_MASK);
}

static void
omap_rtc_lock(void)
{
	/* Write garbage to the KICK registers to enable write protect. */
	reg_write(rtc.regs->RTC_SS_KICK0R, AM335X_RTC_SS_KICK0R_LOCK_MASK);
	reg_write(rtc.regs->RTC_SS_KICK1R, AM335X_RTC_SS_KICK1R_LOCK_MASK);
}

static int
omap_rtc_clkconf(void)
{
	int r;

	/* Configure the clocks need to run the RTC */
	r = clkconf_init();
	if (r != OK) {
		return r;
	}

	r = clkconf_set(CM_RTC_RTC_CLKCTRL, 0xffffffff,
	    CM_RTC_RTC_CLKCTRL_MASK);
	if (r != OK) {
		return r;
	}

	r = clkconf_set(CM_RTC_CLKSTCTRL, 0xffffffff, CM_RTC_CLKSTCTRL_MASK);
	if (r != OK) {
		return r;
	}

	r = clkconf_release();
	if (r != OK) {
		return r;
	}

	return OK;
}

int
omap_rtc_init(void)
{
	int r;
	int rtc_rev, major, minor;
	struct minix_mem_range mr;

	struct machine  machine ;
	sys_getmachine(&machine);

	if(! BOARD_IS_BB(machine.board_id)){
		/* Only the am335x (BeagleBone & BeagleBone Black) is supported ATM.
		 * The dm37xx (BeagleBoard-xM) doesn't have a real time clock
		 * built-in. Instead, it uses the RTC on the PMIC. A driver for
		 * the BeagleBoard-xM's PMIC still needs to be developed.
		 */
		log_warn(&log, "unsupported processor\n");
		return ENOSYS;
	}

	if (pwr_off_in_progress)
		return EINVAL;

	use_count++;
	if (rtc.mapped_addr != 0) {
		/* already intialized */
		return OK;
	}

	/* Enable Clocks */
	r = omap_rtc_clkconf();
	if (r != OK) {
		log_warn(&log, "Failed to enable clocks for RTC.\n");
		return r;
	}

	/*
	 * Map RTC_SS Registers
	 */

	/* Configure memory access */
	mr.mr_base = rtc.mr_base;	/* start addr */
	mr.mr_limit = mr.mr_base + rtc.mr_size;	/* end addr */

	/* ask for privileges to access the RTC_SS memory range */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
		log_warn(&log,
		    "Unable to obtain RTC memory range privileges.");
		return EPERM;
	}

	/* map the memory into this process */
	rtc.mapped_addr = (vir_bytes) vm_map_phys(SELF,
	    (void *) rtc.mr_base, rtc.mr_size);
	if (rtc.mapped_addr == (vir_bytes) MAP_FAILED) {
		log_warn(&log, "Unable to map RTC registers\n");
		return EPERM;
	}

	rtc_rev = reg_read(rtc.regs->RTC_SS_RTC_REVISION);
	major = (rtc_rev & 0x0700) >> 8;
	minor = (rtc_rev & 0x001f);
	log_debug(&log, "omap rtc rev %d.%d\n", major, minor);

	/* Disable register write protect */
	omap_rtc_unlock();

	/* Set NOIDLE */
	reg_write(rtc.regs->RTC_SS_RTC_SYSCONFIG, (1 << NOIDLE_BIT));

	/* Enable 32kHz clock */
	reg_set_bit(rtc.regs->RTC_SS_RTC_OSC_REG, EN_32KCLK_BIT);

	/* Setting the stop bit starts the RTC running */
	reg_set_bit(rtc.regs->RTC_SS_RTC_CTRL_REG, RTC_STOP_BIT);

	/* Re-enable Write Protection */
	omap_rtc_lock();

	log_debug(&log, "OMAP RTC Initialized\n");

	return OK;
}

/*
 * These are the ranges used by the real time clock and struct tm.
 *
 * Field		OMAP RTC		struct tm
 * -----		--------		---------
 * seconds		0 to 59 (Mask 0x7f)	0 to 59 (60 for leap seconds)
 * minutes		0 to 59 (Mask 0x7f)	0 to 59
 * hours		0 to 23	(Mask 0x3f)	0 to 23
 * day			1 to 31	(Mask 0x3f)	1 to 31
 * month		1 to 12	(Mask 0x1f)	0 to 11
 * year			last 2 digits of year	X + 1900
 */

int
omap_rtc_get_time(struct tm *t, int flags)
{
	int r;

	if (pwr_off_in_progress)
		return EINVAL;

	memset(t, '\0', sizeof(struct tm));

	/* Read and Convert BCD to binary (default RTC mode). */
	t->tm_sec = bcd_to_dec(reg_read(rtc.regs->RTC_SS_SECONDS_REG) & 0x7f);
	t->tm_min = bcd_to_dec(reg_read(rtc.regs->RTC_SS_MINUTES_REG) & 0x7f);
	t->tm_hour = bcd_to_dec(reg_read(rtc.regs->RTC_SS_HOURS_REG) & 0x3f);
	t->tm_mday = bcd_to_dec(reg_read(rtc.regs->RTC_SS_DAYS_REG) & 0x3f);
	t->tm_mon =
	    bcd_to_dec(reg_read(rtc.regs->RTC_SS_MONTHS_REG) & 0x1f) - 1;
	t->tm_year =
	    bcd_to_dec(reg_read(rtc.regs->RTC_SS_YEARS_REG) & 0xff) + 100;

	if (t->tm_year == 100) {
		/* Cold start - no date/time set - default to 2013-01-01 */
		t->tm_sec = 0;
		t->tm_min = 0;
		t->tm_hour = 0;
		t->tm_mday = 1;
		t->tm_mon = 0;
		t->tm_year = 113;

		omap_rtc_set_time(t, RTCDEV_NOFLAGS);
	}

	return OK;
}

int
omap_rtc_set_time(struct tm *t, int flags)
{
	int r;

	if (pwr_off_in_progress)
		return EINVAL;

	/* Disable Write Protection */
	omap_rtc_unlock();

	/* Write the date/time to the RTC registers. */
	safe_reg_write(rtc.regs->RTC_SS_SECONDS_REG,
	    (dec_to_bcd(t->tm_sec) & 0x7f));
	safe_reg_write(rtc.regs->RTC_SS_MINUTES_REG,
	    (dec_to_bcd(t->tm_min) & 0x7f));
	safe_reg_write(rtc.regs->RTC_SS_HOURS_REG,
	    (dec_to_bcd(t->tm_hour) & 0x3f));
	safe_reg_write(rtc.regs->RTC_SS_DAYS_REG,
	    (dec_to_bcd(t->tm_mday) & 0x3f));
	safe_reg_write(rtc.regs->RTC_SS_MONTHS_REG,
	    (dec_to_bcd(t->tm_mon + 1) & 0x1f));
	safe_reg_write(rtc.regs->RTC_SS_YEARS_REG,
	    (dec_to_bcd(t->tm_year % 100) & 0xff));

	/* Re-enable Write Protection */
	omap_rtc_lock();

	return OK;
}

int
omap_rtc_pwr_off(void)
{
	int r;
	struct tm t;

	if (pwr_off_in_progress)
		return EINVAL;

	/* wait until 3 seconds can be added without overflowing tm_sec */
	do {
		omap_rtc_get_time(&t, RTCDEV_NOFLAGS);
		micro_delay(250000);
	} while (t.tm_sec >= 57);

	/* set the alarm for 3 seconds from now */
	t.tm_sec += 3;

	/* Disable register write protect */
	omap_rtc_unlock();

	/* enable power-off via ALARM2 by setting the PWR_ENABLE_EN bit. */
	safe_reg_set_bit(rtc.regs->RTC_SS_RTC_PMIC, PWR_ENABLE_EN_BIT);

	/* Write the date/time to the RTC registers. */
	safe_reg_write(rtc.regs->RTC_SS_ALARM2_SECONDS_REG,
	    (dec_to_bcd(t.tm_sec) & 0x7f));
	safe_reg_write(rtc.regs->RTC_SS_ALARM2_MINUTES_REG,
	    (dec_to_bcd(t.tm_min) & 0x7f));
	safe_reg_write(rtc.regs->RTC_SS_ALARM2_HOURS_REG,
	    (dec_to_bcd(t.tm_hour) & 0x3f));
	safe_reg_write(rtc.regs->RTC_SS_ALARM2_DAYS_REG,
	    (dec_to_bcd(t.tm_mday) & 0x3f));
	safe_reg_write(rtc.regs->RTC_SS_ALARM2_MONTHS_REG,
	    (dec_to_bcd(t.tm_mon + 1) & 0x1f));
	safe_reg_write(rtc.regs->RTC_SS_ALARM2_YEARS_REG,
	    (dec_to_bcd(t.tm_year % 100) & 0xff));

	/* enable interrupt to trigger POWER_EN to go low when alarm2 hits. */
	safe_reg_set_bit(rtc.regs->RTC_SS_RTC_INTERRUPTS_REG, IT_ALARM2_BIT);

	/* pause the realtime clock. the kernel will enable it when safe. */
	reg_clear_bit(rtc.regs->RTC_SS_RTC_CTRL_REG, RTC_STOP_BIT);

	/* Set this flag to block all other operations so that the clock isn't
	 * accidentally re-startered and so write protect isn't re-enabled. */
	pwr_off_in_progress = 1;

	/* Make the kernel's job easier by not re-enabling write protection */

	return OK;
}

void
omap_rtc_exit(void)
{
	use_count--;
	if (use_count == 0) {
		vm_unmap_phys(SELF, (void *) rtc.mapped_addr, rtc.mr_size);
		rtc.mapped_addr = 0;
	}
	log_debug(&log, "Exiting\n");
}
