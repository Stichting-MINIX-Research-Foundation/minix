
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/safecopies.h>

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <fcntl.h>

#include "../glo.h"
#include "../proto.h"
#include "../util.h"

/*===========================================================================*
 *				arch_handle_pagefaults	     		     *
 *===========================================================================*/
PUBLIC int arch_get_pagefault(who, addr, err)
endpoint_t *who;
vir_bytes *addr;
u32_t *err;
{
	return sys_vmctl_get_pagefault_i386(who, addr, err);
}

