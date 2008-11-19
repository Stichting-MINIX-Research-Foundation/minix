
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

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <stdlib.h>

#include "proto.h"
#include "util.h"

#define SENDSLOTS _NR_PROCS

PRIVATE asynmsg_t msgtable[SENDSLOTS];
PRIVATE size_t msgtable_n= SENDSLOTS;

PUBLIC int asynsend(dst, mp)
endpoint_t dst;
message *mp;
{
        int i;
        unsigned flags;

        /* Find slot in table */
        for (i= 0; i<msgtable_n; i++)
        {
                flags= msgtable[i].flags;
                if ((flags & (AMF_VALID|AMF_DONE)) == (AMF_VALID|AMF_DONE))
                {
                        if (msgtable[i].result != OK)
                        {
                                printf(
                      "VM: asynsend: found completed entry %d with error %d\n",
                                        i, msgtable[i].result);
                        }
                        break;
                }
                if (flags == AMF_EMPTY)
                        break;
        }
        if (i >= msgtable_n)
                vm_panic("asynsend: should resize table", i);
        msgtable[i].dst= dst;
        msgtable[i].msg= *mp;
        msgtable[i].flags= AMF_VALID;   /* Has to be last. The kernel
                                         * scans this table while we are
                                         * sleeping.
                                         */

        /* Tell the kernel to rescan the table */
        return senda(msgtable, msgtable_n);
}

