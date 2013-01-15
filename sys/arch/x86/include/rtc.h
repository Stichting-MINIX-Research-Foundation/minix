/*      $NetBSD: rtc.h,v 1.1 2009/06/16 21:05:34 bouyer Exp $    */

#include <dev/clock_subr.h>

void rtc_register(void);
int  rtc_get_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);
int  rtc_set_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);
