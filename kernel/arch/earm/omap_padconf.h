#ifndef _OMAP_PADCONF_H
#define _OMAP_PADCONF_H

#ifndef __ASSEMBLY__

void arch_padconf_init(void);
int arch_padconf_set(u32_t padconf, u32_t mask, u32_t value);

#endif /* __ASSEMBLY__ */

#endif /* _OMAP_TIMER_H */
