#include <stdint.h>

#include "syslib.h"

int sys_padconf(u32_t padconf, u32_t mask, u32_t value)
{
        message m;

	m.PADCONF_PADCONF = padconf;
	m.PADCONF_MASK = mask;
	m.PADCONF_VALUE = value;

        return(_kernel_call(SYS_PADCONF, &m));
}
