#include <sys/types.h>

void
read_tsc(u32_t *hi, u32_t *lo)
{
/* Read Clock Cycle Counter (CCNT). Intel calls it Time Stamp Counter (TSC) */
	u32_t ccnt;

	/* Get value from the Performance Monitors Cycle Counter Register.
	 * See ARM Architecture Reference Manual B5.1.113.
	 */
	asm volatile ("MRC p15, 0, %0, c9, c13, 0\t\n" : "=r" (ccnt) : : "%0");

	/* The ARMv7-A clock cycle counter is only 32-bits, but read_tsc is
	 * expected to return a 64-bit value. hi is therefore always 0.
	 */
	*hi = 0;
	*lo = ccnt;
}

