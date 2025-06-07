#ifndef __CLOCK_H__
#define __CLOCK_H__

#include "kernel/kernel.h"
#include "arch_clock.h"

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h> // For KASSERT_PLACEHOLDER if used in headers this includes
#include <klib/include/kstring.h> // Precautionary
#include <klib/include/kmemory.h> // Precautionary


int boot_cpu_init_timer(unsigned freq);
int app_cpu_init_timer(unsigned freq);

int timer_int_handler(void);

int init_local_timer(unsigned freq);
/* stop the local timer ticking */
void stop_local_timer(void);
/* let the time tick again with the original settings after it was stopped */
void restart_local_timer(void);
int register_local_timer_handler(irq_handler_t handler);

u64_t ms_2_cpu_time(unsigned ms);
unsigned cpu_time_2_ms(u64_t cpu_time);

#endif /* __CLOCK_H__ */
