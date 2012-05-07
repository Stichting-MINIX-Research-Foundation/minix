
#include "syslib.h"

#include <minix/safecopies.h>

/*===========================================================================*
 *				sys_safemap				     *
 *===========================================================================*/
int sys_safemap(endpoint_t grantor, cp_grant_id_t grant,
	vir_bytes grant_offset, vir_bytes my_address,
	size_t bytes, int writable)
{
/* Map a block of data for which the other process has previously
 * granted permission. 
 */

	message copy_mess;

	copy_mess.SMAP_EP = grantor;
	copy_mess.SMAP_GID = grant;
	copy_mess.SMAP_OFFSET = grant_offset;
	copy_mess.SMAP_ADDRESS = my_address;
	copy_mess.SMAP_BYTES = bytes;
	copy_mess.SMAP_FLAG = writable;

	copy_mess.SMAP_SEG_OBSOLETE = (void *) D_OBSOLETE;

	return(_kernel_call(SYS_SAFEMAP, &copy_mess));

}

/*===========================================================================*
 *			     sys_saferevmap_gid				     *
 *===========================================================================*/
int sys_saferevmap_gid(cp_grant_id_t grant)
{
/* Grantor revokes safemap by grant id. */
	message copy_mess;

	copy_mess.SMAP_FLAG = 1;
	copy_mess.SMAP_GID = grant;

	return(_kernel_call(SYS_SAFEREVMAP, &copy_mess));
}

/*===========================================================================*
 *			    sys_saferevmap_addr				     *
 *===========================================================================*/
int sys_saferevmap_addr(vir_bytes addr)
{
/* Grantor revokes safemap by address. */
	message copy_mess;

	copy_mess.SMAP_FLAG = 0;
	copy_mess.SMAP_GID = addr;

	return(_kernel_call(SYS_SAFEREVMAP, &copy_mess));
}

/*===========================================================================*
 *				sys_safeunmap				     *
 *===========================================================================*/
int sys_safeunmap(vir_bytes my_address)
{
/* Requestor unmaps safemap. */
	message copy_mess;

	copy_mess.SMAP_ADDRESS = my_address;

	copy_mess.SMAP_SEG_OBSOLETE = (void *) D_OBSOLETE;

	return(_kernel_call(SYS_SAFEUNMAP, &copy_mess));
}

