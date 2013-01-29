#ifndef _OMAP_TIMER_H
#define _OMAP_TIMER_H

#include "omap_timer_registers.h"

#ifndef __ASSEMBLY__

void omap3_timer_init(unsigned freq);
void omap3_timer_stop(void);
void omap3_frclock_init(void);
void omap3_frclock_stop(void);
int omap3_register_timer_handler(const irq_handler_t handler);
void omap3_timer_int_handler(void);

#endif /* __ASSEMBLY__ */

#endif /* _OMAP_TIMER_H */
