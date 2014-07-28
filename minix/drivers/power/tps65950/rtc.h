#ifndef __RTC_H
#define __RTC_H

#include <time.h>

int rtc_init(void);
int rtc_get_time(struct tm *t, int flags); /* read the hardware clock into t */
int rtc_set_time(struct tm *t, int flags); /* set the hardware clock to t */
int rtc_exit(void);

#endif /* __RTC_H */
