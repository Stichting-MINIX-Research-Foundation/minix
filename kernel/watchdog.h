#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include "kernel/kernel.h"
#include "arch_watchdog.h"

extern int watchdog_enabled; /* if set to non-zero the watch dog is enabled */
extern unsigned watchdog_local_timer_ticks; /* is timer still ticking? */

/*
 * as the implementation is not only architecture dependent but like in x86 case
 * very much model specific, we need to keep a collection of methods that
 * implement it in runtime after the correct arch/model was detected
 */

typedef void (* arch_watchdog_method_t)(const unsigned);
typedef int (* arch_watchdog_profile_init_t)(const unsigned);

struct arch_watchdog {
	arch_watchdog_method_t		init;	/* initial setup */
	arch_watchdog_method_t		reinit;	/* reinit after a tick */
	arch_watchdog_profile_init_t	profile_init;
	u64_t				resetval;
	u64_t				watchdog_resetval;
	u64_t				profile_resetval;
};

extern struct arch_watchdog *watchdog;

/* let the arch code do whatever it needs to setup or quit the watchdog */
int arch_watchdog_init(void);
void arch_watchdog_stop(void);
/* if the watchdog detects lockup, let the arch code to handle it */
void arch_watchdog_lockup(const struct nmi_frame * frame);

/* generic NMI handler. Takes one agument which points to where the arch
 * specific low level handler dumped CPU information and can be inspected by the
 * arch specific code of the watchdog implementaion */
void nmi_watchdog_handler(struct nmi_frame * frame);

/*
 * start and stop profiling using the NMI watchdog
 */
int nmi_watchdog_start_profiling(const unsigned freq);
void nmi_watchdog_stop_profiling(void);

#endif /* __WATCHDOG_H__ */
