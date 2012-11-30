#include <sys/types.h>
#include <minix/sysutil.h>
#include <errno.h>

u32_t sys_jiffies(void)
{
	clock_t ticks;

	if (getuptime(&ticks) == OK)
		return  ticks;
	else
		panic("getuptime() failed\n");
}

u32_t sys_now(void)
{
	static u32_t hz;
	u32_t jiffs;

	if (!hz)
		hz = sys_hz();

	jiffs = sys_jiffies();

	return jiffs * (1000 / hz);
}

