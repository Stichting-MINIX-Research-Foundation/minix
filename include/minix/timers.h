#ifndef _MINIX_TIMERS_H
#define _MINIX_TIMERS_H

/* Timers abstraction for system processes. This would be in minix/sysutil.h
 * if it weren't for naming conflicts.
 */

#include <timers.h>

void init_timer(timer_t *tp);
void set_timer(timer_t *tp, int ticks, tmr_func_t watchdog, int arg);
void cancel_timer(timer_t *tp);
void expire_timers(clock_t now);

#endif /* _MINIX_TIMERS_H */
