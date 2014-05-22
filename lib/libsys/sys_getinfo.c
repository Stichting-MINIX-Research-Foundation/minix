
#include <string.h>
#include <sys/param.h>
#include "syslib.h"

/*===========================================================================*
 *                                sys_getinfo				     *
 *===========================================================================*/
int sys_getinfo(request, ptr, len, ptr2, len2)
int request; 				/* system info requested */
void *ptr;				/* pointer where to store it */
int len;				/* max length of value to get */
void *ptr2;				/* second pointer */
int len2;				/* length or process nr */ 
{
    message m;

    m.m_lsys_krn_sys_getinfo.request = request;
    m.m_lsys_krn_sys_getinfo.endpt = SELF;	/* always store values at caller */
    m.m_lsys_krn_sys_getinfo.val_ptr = ptr;
    m.m_lsys_krn_sys_getinfo.val_len = len;
    m.m_lsys_krn_sys_getinfo.val_ptr2 = ptr2;
    m.m_lsys_krn_sys_getinfo.val_len2_e = len2;

    return(_kernel_call(SYS_GETINFO, &m));
}

/*===========================================================================*
 *                                sys_whoami				     *
 *===========================================================================*/
int sys_whoami(endpoint_t *who_ep, char *who_name, int len,
	int *priv_flags)
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

	return OK;
}

