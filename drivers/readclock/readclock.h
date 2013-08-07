#ifndef __READCLOCK_H
#define __READCLOCK_H

#include <time.h>

/* implementations provided by arch/${MACHINE_ARCH}/arch_readclock.c */
struct rtc {
	int (*init)(void);
	int (*get_time)(struct tm *t, int flags);
	int (*set_time)(struct tm *t, int flags);
	int (*pwr_off)(void);
	void (*exit)(void);
};

int arch_setup(struct rtc *r);

/* utility functions provided by readclock.c */
int bcd_to_dec(int n);
int dec_to_bcd(int n);

#endif /* __READCLOCK_H */
