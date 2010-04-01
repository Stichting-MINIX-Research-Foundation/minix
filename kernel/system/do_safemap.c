/* The kernel call implemented in this file:
 *   m_type:	SYS_SAFEMAP or SYS_SAFEREVMAP or SYS_SAFEUNMAP
 *
 * The parameters for this kernel call are:
 *	SMAP_EP		endpoint of the grantor
 *	SMAP_GID	grant id
 *	SMAP_OFFSET	offset of the grant space
 *	SMAP_SEG	segment
 *	SMAP_ADDRESS	address
 *    	SMAP_BYTES	bytes to be copied
 *    	SMAP_FLAG	access, writable map or not?
 */

#include <assert.h>

#include <minix/type.h>
#include <minix/type.h>
#include <minix/safecopies.h>

#include "kernel/system.h"

#include <signal.h>

struct map_info_s {
	int flag;

	/* Grantor. */
	endpoint_t grantor;
	cp_grant_id_t gid;
	vir_bytes offset;
	vir_bytes address_Dseg; /* seg always is D */

	/* Grantee. */
	endpoint_t grantee;
	int seg;
	vir_bytes address;

	/* Length. */
	vir_bytes bytes;
};

#define MAX_MAP_INFO 20
static struct map_info_s map_info[MAX_MAP_INFO];

/*===========================================================================*
 *				add_info				     *
 *===========================================================================*/
static int add_info(endpoint_t grantor, endpoint_t grantee, cp_grant_id_t gid,
		vir_bytes offset, vir_bytes address_Dseg,
		int seg, vir_bytes address, vir_bytes bytes)
{
	int i;

	for(i = 0; i < MAX_MAP_INFO; i++) {
		if(map_info[i].flag == 0)
			break;
	}
	if(i == MAX_MAP_INFO)
		return EBUSY;

	map_info[i].flag = 1;
	map_info[i].grantor = grantor;
	map_info[i].grantee = grantee;
	map_info[i].gid = gid;
	map_info[i].address_Dseg = address_Dseg;
	map_info[i].offset = offset;
	map_info[i].seg = seg;
	map_info[i].address = address;
	map_info[i].bytes = bytes;

	return OK;
}

/*===========================================================================*
 *				get_revoke_info				     *
 *===========================================================================*/
static struct map_info_s *get_revoke_info(endpoint_t grantor, int flag, int arg)
{
	int i;
	for(i = 0; i < MAX_MAP_INFO; i++) {
		if(map_info[i].flag == 1
			&& map_info[i].grantor == grantor
			&& (flag ? (map_info[i].gid == arg)
				 : (map_info[i].address_Dseg == arg)))
			return &map_info[i];
	}

	return NULL;
}

/*===========================================================================*
 *				get_unmap_info				     *
 *===========================================================================*/
static struct map_info_s *get_unmap_info(endpoint_t grantee, int seg,
	vir_bytes address)
{
	int i;
	for(i = 0; i < MAX_MAP_INFO; i++) {
		if(map_info[i].flag == 1
			&& map_info[i].grantee == grantee
			&& map_info[i].seg == seg
			&& map_info[i].address == address)
			return &map_info[i];
	}

	return NULL;
}

/*===========================================================================*
 *				clear_info				     *
 *===========================================================================*/
static void clear_info(struct map_info_s *p)
{
	p->flag = 0;
}

/*===========================================================================*
 *				map_invoke_vm				     *
 *===========================================================================*/
PUBLIC int map_invoke_vm(struct proc * caller,
			int req_type, /* VMPTYPE_... COWMAP, SMAP, SUNMAP */
			endpoint_t end_d, int seg_d, vir_bytes off_d,
			endpoint_t end_s, int seg_s, vir_bytes off_s,
			size_t size, int flag)
{
	struct proc *src, *dst;
	phys_bytes lin_src, lin_dst;

	src = endpoint_lookup(end_s);
	dst = endpoint_lookup(end_d);

	lin_src = umap_local(src, seg_s, off_s, size);
	lin_dst = umap_local(dst, seg_d, off_d, size);
	if(lin_src == 0 || lin_dst == 0) {
		printf("map_invoke_vm: error in umap_local.\n");
		return EINVAL;
	}

	/* Make sure the linear addresses are both page aligned. */
	if(lin_src % CLICK_SIZE != 0
		|| lin_dst % CLICK_SIZE != 0) {
		printf("map_invoke_vm: linear addresses not page aligned.\n");
		return EINVAL;
	}

	assert(!RTS_ISSET(caller, RTS_VMREQUEST));
	assert(!RTS_ISSET(caller, RTS_VMREQTARGET));
	assert(!RTS_ISSET(dst, RTS_VMREQUEST));
	assert(!RTS_ISSET(dst, RTS_VMREQTARGET));
	RTS_SET(caller, RTS_VMREQUEST);
	RTS_SET(dst, RTS_VMREQTARGET);

	/* Map to the destination. */
	caller->p_vmrequest.req_type = req_type;
	caller->p_vmrequest.target = end_d;		/* destination proc */
	caller->p_vmrequest.params.map.vir_d = lin_dst;	/* destination addr */
	caller->p_vmrequest.params.map.ep_s = end_s;	/* source process */
	caller->p_vmrequest.params.map.vir_s = lin_src;	/* source address */
	caller->p_vmrequest.params.map.length = (vir_bytes) size;
	caller->p_vmrequest.params.map.writeflag = flag;

	caller->p_vmrequest.type = VMSTYPE_MAP;

	/* Connect caller on vmrequest wait queue. */
	if(!(caller->p_vmrequest.nextrequestor = vmrequest))
		send_sig(VM_PROC_NR, SIGKMEM);
	vmrequest = caller;

	return OK;
}

/*===========================================================================*
 *				do_safemap				     *
 *===========================================================================*/
PUBLIC int do_safemap(struct proc * caller, message * m_ptr)
{
	endpoint_t grantor	= m_ptr->SMAP_EP;
	cp_grant_id_t gid	= (cp_grant_id_t) m_ptr->SMAP_GID;
	vir_bytes offset	= (vir_bytes) m_ptr->SMAP_OFFSET;
	int seg			= (int) m_ptr->SMAP_SEG;
	vir_bytes address	= (vir_bytes) m_ptr->SMAP_ADDRESS;
	vir_bytes bytes		= (vir_bytes) m_ptr->SMAP_BYTES;
	int flag		= m_ptr->SMAP_FLAG;

	vir_bytes offset_result;
	endpoint_t new_grantor;
	int r;
	int access = CPF_MAP | CPF_READ;

	/* Check the grant. We currently support safemap with both direct and
	 * indirect grants, as verify_grant() stores the original grantor
	 * transparently in new_grantor below. However, we maintain the original
	 * semantics associated to indirect grants only here at safemap time.
	 * After the mapping has been set up, if a process part of the chain
	 * of trust crashes or exits without revoking the mapping, the mapping
	 * can no longer be manually or automatically revoked for any of the
	 * processes lower in the chain. This solution reduces complexity but
	 * could be improved if we make the assumption that only one process in
	 * the chain of trust can effectively map the original memory region.
	 */
	if(flag != 0)
		access |= CPF_WRITE;
	r = verify_grant(grantor, caller->p_endpoint, gid, bytes, access,
		offset, &offset_result, &new_grantor);
	if(r != OK) {
		printf("verify_grant for gid %d from %d to %d failed: %d\n",
			gid, grantor, caller->p_endpoint, r);
		return r;
	}

	/* Add map info. */
	r = add_info(new_grantor, caller->p_endpoint, gid, offset,
			offset_result, seg, address, bytes);
	if(r != OK)
		return r;

	/* Invoke VM. */
	return map_invoke_vm(caller, VMPTYPE_SMAP,
		caller->p_endpoint, seg, address, new_grantor, D, offset_result, bytes,flag);
}

/*===========================================================================*
 *				safeunmap				     *
 *===========================================================================*/
PRIVATE int safeunmap(struct proc * caller, struct map_info_s *p)
{
	vir_bytes offset_result;
	endpoint_t new_grantor;
	int r;

	r = verify_grant(p->grantor, p->grantee, p->gid, p->bytes,
			CPF_MAP, p->offset, &offset_result, &new_grantor);
	if(r != OK) {
	    printf("safeunmap: error in verify_grant.\n");
		return r;
	}

	r = map_invoke_vm(caller, VMPTYPE_SUNMAP,
		p->grantee, p->seg, p->address,
		new_grantor, D, offset_result,
		p->bytes, 0);
	clear_info(p);
	if(r != OK) {
		printf("safeunmap: error in map_invoke_vm.\n");
		return r;
	}
	return OK;
}

/*===========================================================================*
 *				do_saferevmap				     *
 *===========================================================================*/
PUBLIC int do_saferevmap(struct proc * caller, message * m_ptr)
{
	struct map_info_s *p;
	int flag = m_ptr->SMAP_FLAG;
	int arg = m_ptr->SMAP_GID; /* gid or address_Dseg */
	int r;

	while((p = get_revoke_info(caller->p_endpoint, flag, arg)) != NULL) {
		if((r = safeunmap(caller, p)) != OK)
			return r;
	}
	return OK;
}

/*===========================================================================*
 *				do_safeunmap				     *
 *===========================================================================*/
PUBLIC int do_safeunmap(struct proc * caller, message * m_ptr)
{
	vir_bytes address = (vir_bytes) m_ptr->SMAP_ADDRESS;
	int seg = (int)m_ptr->SMAP_SEG;
	struct map_info_s *p;
	int r;

	while((p = get_unmap_info(caller->p_endpoint, seg, address)) != NULL) {
		if((r = safeunmap(caller, p)) != OK)
			return r;
	}
	return OK;
}

