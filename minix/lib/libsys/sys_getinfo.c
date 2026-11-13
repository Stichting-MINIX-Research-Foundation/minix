
#include <string.h>
#include <sys/param.h>
#include "syslib.h"

/*===========================================================================*
 *                                sys_getinfo				     *
 *===========================================================================*/
int sys_getinfo(int request, void *ptr, int len, void *ptr2, int len2)
{
    message m;

    m.m_lsys_krn_sys_getinfo.request = request;
    m.m_lsys_krn_sys_getinfo.endpt = SELF;	/* always store values at caller */
    m.m_lsys_krn_sys_getinfo.val_ptr = (vir_bytes)ptr;
    m.m_lsys_krn_sys_getinfo.val_len = len;
    m.m_lsys_krn_sys_getinfo.val_ptr2 = (vir_bytes)ptr2;
    m.m_lsys_krn_sys_getinfo.val_len2_e = len2;

    return(_kernel_call(SYS_GETINFO, &m));
}

/*===========================================================================*
 *                                sys_whoami				     *
 *===========================================================================*/
int sys_whoami(endpoint_t *who_ep, char *who_name, int len,
	int *priv_flags, int *init_flags)
{
	message m;
	int r;
	int lenmin;

	m.m_lsys_krn_sys_getinfo.request = GET_WHOAMI;

	if(len < 2)
		return EINVAL;

	if((r = _kernel_call(SYS_GETINFO, &m)) != OK)
		return r;

	lenmin = MIN((size_t) len, sizeof(m.m_krn_lsys_sys_getwhoami.name)) - 1;

	strncpy(who_name, m.m_krn_lsys_sys_getwhoami.name, lenmin);
	who_name[lenmin] = '\0';
	*who_ep = m.m_krn_lsys_sys_getwhoami.endpt;
	*priv_flags = m.m_krn_lsys_sys_getwhoami.privflags;
	*init_flags = m.m_krn_lsys_sys_getwhoami.initflags;

	return OK;
}

