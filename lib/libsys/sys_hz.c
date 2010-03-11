
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <minix/u64.h>
#include <minix/config.h>
#include <minix/const.h>

#include "sysutil.h"

static u32_t Hz;

u32_t
sys_hz(void)
{
	if(Hz <= 0) {
		int r;
		/* Get HZ. */
		if((r=sys_getinfo(GET_HZ, &Hz, sizeof(Hz), 0, 0)) != OK) {
			Hz = DEFAULT_HZ;
			printf("sys_hz: can not get HZ: error %d.\nUsing default HZ = %u\n",
			    r, (unsigned int) Hz);
		}
	}

	return Hz;
}

u32_t
micros_to_ticks(u32_t micros)
{
        u32_t ticks;

        ticks = div64u(mul64u(micros, sys_hz()), 1000000);
        if(ticks < 1) ticks = 1;

        return ticks;
}

