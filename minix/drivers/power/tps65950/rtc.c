#include <minix/ds.h>
#include <minix/drivers.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/log.h>

#include <time.h>

#include "tps65950.h"
#include "rtc.h"

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "tps65950.rtc",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static int bcd_to_dec(int n);
static int dec_to_bcd(int n);

int
rtc_init(void)
{
	int r;
	uint8_t val;
	struct tm t;

	r = i2creg_set_bits8(bus_endpoint, addresses[ID4], RTC_CTRL_REG,
	    (1 << STOP_RTC_BIT));
	if (r != OK) {
		log_warn(&log, "Failed to start RTC\n");
		return -1;
	}

	r = i2creg_read8(bus_endpoint, addresses[ID4], RTC_STATUS_REG, &val);
	if (r != OK) {
		log_warn(&log, "Failed to read RTC_STATUS_REG\n");
		return -1;
	}

	if ((val & (1 << RUN_BIT)) != (1 << RUN_BIT)) {
		log_warn(&log, "RTC did not start. Bad MSECURE?\n");
		return -1;
	}

	log_debug(&log, "RTC Started\n");

	return OK;
}

int
rtc_get_time(struct tm *t, int flags)
{
	int r;
	uint8_t val;

	memset(t, '\0', sizeof(struct tm));

	/* Write GET_TIME_BIT to RTC_CTRL_REG to latch the RTC values into
	 * the RTC registers. This is required before each read.
	 */
	r = i2creg_set_bits8(bus_endpoint, addresses[ID4], RTC_CTRL_REG,
	    (1 << GET_TIME_BIT));
	if (r != OK) {
		return -1;
	}

	/* Read and Convert BCD to binary (default RTC mode). */

	/* Seconds - 0 to 59 */
	r = i2creg_read8(bus_endpoint, addresses[ID4], SECONDS_REG, &val);
	if (r != OK) {
		return -1;
	}
	t->tm_sec = bcd_to_dec(val & 0x7f);

	/* Minutes - 0 to 59 */
	r = i2creg_read8(bus_endpoint, addresses[ID4], MINUTES_REG, &val);
	if (r != OK) {
		return -1;
	}
	t->tm_min = bcd_to_dec(val & 0x7f);

	/* Hours - 0 to 23 */
	r = i2creg_read8(bus_endpoint, addresses[ID4], HOURS_REG, &val);
	if (r != OK) {
		return -1;
	}
	t->tm_hour = bcd_to_dec(val & 0x3f);

	/* Days - 1 to 31 */
	r = i2creg_read8(bus_endpoint, addresses[ID4], DAYS_REG, &val);
	if (r != OK) {
		return -1;
	}
	t->tm_mday = bcd_to_dec(val & 0x3f);

	/* Months - Jan=1 to Dec=12 */
	r = i2creg_read8(bus_endpoint, addresses[ID4], MONTHS_REG, &val);
	if (r != OK) {
		return -1;
	}
	t->tm_mon = bcd_to_dec(val & 0x1f) - 1;

	/* Years - last 2 digits of year */
	r = i2creg_read8(bus_endpoint, addresses[ID4], YEARS_REG, &val);
	if (r != OK) {
		return -1;
	}
	t->tm_year = bcd_to_dec(val & 0x1f) + 100;

	if (t->tm_year == 100) {
		/* Cold start - no date/time set - default to 2013-01-01 */
		t->tm_sec = 0;
		t->tm_min = 0;
		t->tm_hour = 0;
		t->tm_mday = 1;
		t->tm_mon = 0;
		t->tm_year = 113;

		rtc_set_time(t, RTCDEV_NOFLAGS);
	}

	return OK;
}

int
rtc_set_time(struct tm *t, int flags)
{
	int r;

	/* Write the date/time to the RTC registers. */
	r = i2creg_write8(bus_endpoint, addresses[ID4], SECONDS_REG,
	    (dec_to_bcd(t->tm_sec) & 0x7f));
	if (r != OK) {
		return -1;
	}

	r = i2creg_write8(bus_endpoint, addresses[ID4], MINUTES_REG,
	    (dec_to_bcd(t->tm_min) & 0x7f));
	if (r != OK) {
		return -1;
	}

	r = i2creg_write8(bus_endpoint, addresses[ID4], HOURS_REG,
	    (dec_to_bcd(t->tm_hour) & 0x3f));
	if (r != OK) {
		return -1;
	}

	r = i2creg_write8(bus_endpoint, addresses[ID4], DAYS_REG,
	    (dec_to_bcd(t->tm_mday) & 0x3f));
	if (r != OK) {
		return -1;
	}

	r = i2creg_write8(bus_endpoint, addresses[ID4], MONTHS_REG,
	    (dec_to_bcd(t->tm_mon + 1) & 0x1f));
	if (r != OK) {
		return -1;
	}

	r = i2creg_write8(bus_endpoint, addresses[ID4], YEARS_REG,
	    (dec_to_bcd(t->tm_year % 100) & 0xff));
	if (r != OK) {
		return -1;
	}

	return OK;
}

int
rtc_exit(void)
{
	return OK;
}

static int
bcd_to_dec(int n)
{
	return ((n >> 4) & 0x0F) * 10 + (n & 0x0F);
}

static int
dec_to_bcd(int n)
{
	return ((n / 10) << 4) | (n % 10);
}
