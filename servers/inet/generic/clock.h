/*
clock.h

Copyright 1995 Philip Homburg
*/

#ifndef CLOCK_H
#define CLOCK_H

struct timer;

typedef void (*timer_func_t) ARGS(( int fd, struct timer *timer ));

typedef struct timer
{
	struct timer *tim_next;
	timer_func_t tim_func;
	int tim_ref;
	time_t tim_time;
	int tim_active;
} timer_t;

extern int clck_call_expire;	/* Call clck_expire_timer from the mainloop */

void clck_init ARGS(( void ));
void set_time ARGS(( time_t time ));
time_t get_time ARGS(( void ));
void reset_time ARGS(( void ));
/* set a timer to go off at the time specified by timeout */
void clck_timer ARGS(( struct timer *timer, time_t timeout, timer_func_t func,
								int fd ));
void clck_untimer ARGS(( struct timer *timer ));
void clck_expire_timers ARGS(( void ));

#endif /* CLOCK_H */

/*
 * $PchId: clock.h,v 1.5 1995/11/21 06:45:27 philip Exp $
 */
