#ifndef __READCLOCK_H
#define __READCLOCK_H

#include <time.h>

/* implementations provided by arch/${MACHINE_ARCH}/arch_readclock.c */
int arch_init(void); /* setup */
int arch_get_time(struct tm *t, int flags); /* read the hardware clock into t */
int arch_set_time(struct tm *t, int flags); /* set the hardware clock to t */
int arch_pwr_off(void); /* set the power off alarm to 5 sec from now. */
void arch_exit(void); /* clean up */

/* arch specific driver related functions */
int arch_sef_cb_lu_state_save(int);
int arch_lu_state_restore(void);
void arch_announce(void);

/* utility functions provided by readclock.c */
int bcd_to_dec(int n);
int dec_to_bcd(int n);

#endif /* __READCLOCK_H */
