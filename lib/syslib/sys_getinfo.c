
#include <string.h>

#include "syslib.h"

/*===========================================================================*
 *                                sys_getinfo				     *
 *===========================================================================*/
PUBLIC int sys_getinfo(request, ptr, len, ptr2, len2)
int request; 				/* system info requested */
void *ptr;				/* pointer where to store it */
int len;				/* max length of value to get */
void *ptr2;				/* second pointer */
int len2;				/* length or process nr */ 
{
    message m;

    m.I_REQUEST = request;
    m.I_ENDPT = SELF;			/* always store values at caller */
    m.I_VAL_PTR = ptr;
    m.I_VAL_LEN = len;
    m.I_VAL_PTR2 = ptr2;
    m.I_VAL_LEN2_E = len2;

    return(_taskcall(SYSTASK, SYS_GETINFO, &m));
}

/*===========================================================================*
 *                                sys_whoami				     *
 *===========================================================================*/
PUBLIC int sys_whoami(endpoint_t *who_ep, char *who_name, int len)
{
	message m;
	int r;
	int lenmin;

	m.I_REQUEST = GET_WHOAMI;

	if(len < 2)
		return EINVAL;

	if((r = _taskcall(SYSTASK, SYS_GETINFO, &m)) != OK)
		return r;

	lenmin = MIN(len, sizeof(m.GIWHO_NAME)) - 1;

	strncpy(who_name, m.GIWHO_NAME, lenmin);
	who_name[lenmin] = '\0';
	*who_ep = m.GIWHO_EP;

	return OK;
}

