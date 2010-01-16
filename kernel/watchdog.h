#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include "kernel.h"
#include "arch/i386/watchdog.h"

extern int watchdog_enabled; /* if set to non-zero the watch dog is enabled */
extern unsigned watchdog_local_timer_ticks; /* is timer still ticking? */

/*
 * as the implementation is not only architecture dependent but like in x86 case
 * very much model specific, we need to keep a collection of methods that
 * implement it in runtime after the correct arch/model was detected
 */

typedef void (* arch_watchdog_method_t)(int);

struct arch_watchdog {
	arch_watchdog_method_t	init;	/* initial setup */
	arch_watchdog_method_t	reinit;	/* reinitialization after a tick */
	unsigned		resetval;
};

extern struct arch_watchdog *watchdog;

/* let the arch code do whatever it needs to setup the watchdog */
int arch_watchdog_init(void);
/* if the watchdog detects lockup, let the arch code to handle it */
void arch_watchdog_lockup(struct nmi_frame * frame);

/* generic NMI handler. Takes one agument which points to where the arch
 * specific low level handler dumped CPU information and can be inspected by the
 * arch specific code of the watchdog implementaion */
void nmi_watchdog_handler(struct nmi_frame * frame);

#endif /* __WATCHDOG_H__ */
