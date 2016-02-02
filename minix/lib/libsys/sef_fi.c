
#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>

EXTERN __attribute__((weak)) int edfi_ctl_process_request(void *ctl_request);

EXTERN int do_sef_fi_request(message *m_ptr);

/*===========================================================================*
 *                            do_sef_fi_request             		     *
 *===========================================================================*/
int do_sef_fi_request(message *m_ptr)
{
    /* See if we are simply asked to crash. */
    if (m_ptr->m_lsys_fi_ctl.subtype == RS_FI_CRASH)
        panic("Crash!");

#if SEF_FI_ALLOW_EDFI
    /* Forward the request to the EDFI fault injector, if linked in. */
    if(edfi_ctl_process_request)
        return edfi_ctl_process_request(m_ptr);
#endif

    return ENOSYS;
}

