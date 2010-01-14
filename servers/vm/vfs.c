
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
#include <minix/bitmap.h>

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>

#include "glo.h"
#include "proto.h"
#include "util.h"

/*===========================================================================*
 *				register_callback			     *
 *===========================================================================*/
PRIVATE void register_callback(struct vmproc *for_who, callback_t callback,
	int callback_type)
{
	if(for_who->vm_callback) {
		vm_panic("register_callback: callback already registered",
			for_who->vm_callback_type);
	}
	for_who->vm_callback = callback;
	for_who->vm_callback_type = callback_type;

	return;
}

/*===========================================================================*
 *				vfs_open				     *
 *===========================================================================*/
PUBLIC int vfs_open(struct vmproc *for_who, callback_t callback,
	cp_grant_id_t filename_gid, int filename_len, int flags, int mode)
{
	static message m;
	int r;

	register_callback(for_who, callback, VM_VFS_REPLY_OPEN);

	m.m_type = VM_VFS_OPEN;
	m.VMVO_NAME_GRANT = filename_gid;
	m.VMVO_NAME_LENGTH = filename_len;
	m.VMVO_FLAGS = flags;
	m.VMVO_MODE = mode;
	m.VMVO_ENDPOINT = for_who->vm_endpoint;

	if((r=asynsend(VFS_PROC_NR, &m)) != OK) {
		vm_panic("vfs_open: asynsend failed", r);
	}

	return r;
}

/*===========================================================================*
 *				vfs_close				     *
 *===========================================================================*/
PUBLIC int vfs_close(struct vmproc *for_who, callback_t callback, int fd)
{
	static message m;
	int r;

	register_callback(for_who, callback, VM_VFS_REPLY_CLOSE);

	m.m_type = VM_VFS_CLOSE;
	m.VMVC_ENDPOINT = for_who->vm_endpoint;
	m.VMVC_FD = fd;

	if((r=asynsend(VFS_PROC_NR, &m)) != OK) {
		vm_panic("vfs_close: asynsend failed", r);
	}

	return r;
}

/*===========================================================================*
 *				do_vfs_reply			     	*
 *===========================================================================*/
PUBLIC int do_vfs_reply(message *m)
{
/* Reply to a request has been received from vfs. Handle it. First verify
 * and look up which process, identified by endpoint, this is about.
 * Then call the callback function that was registered when the request
 * was done. Return result to vfs.
 */
	endpoint_t ep;
	struct vmproc *vmp;
	int procno;
	callback_t cb;
	ep = m->VMV_ENDPOINT;
	if(vm_isokendpt(ep, &procno) != OK) {
		printf("VM:do_vfs_reply: reply %d about invalid endpoint %d\n",
			m->m_type, ep);
		vm_panic("do_vfs_reply: invalid endpoint from vfs", NO_NUM);
	}
	vmp = &vmproc[procno];
	if(!vmp->vm_callback) {
		printf("VM:do_vfs_reply: reply %d: endpoint %d not waiting\n",
			m->m_type, ep);
		vm_panic("do_vfs_reply: invalid endpoint from vfs", NO_NUM);
	}
	if(vmp->vm_callback_type != m->m_type) {
		printf("VM:do_vfs_reply: reply %d unexpected for endpoint %d\n"
		  " (expecting %d)\n", m->m_type, ep, vmp->vm_callback_type);
		vm_panic("do_vfs_reply: invalid reply from vfs", NO_NUM);
	}
	if(vmp->vm_flags & VMF_EXITING) {
		/* This is not fatal or impossible, but the callback
		 * function has to realize it shouldn't do any PM or
		 * VFS calls for this process.
		 */
		printf("VM:do_vfs_reply: reply %d for EXITING endpoint %d\n",
		  m->m_type, ep);
	}

	/* All desired callback state has been used, so save and reset
	 * the callback. This allows the callback to register another
	 * one.
	 */
	cb = vmp->vm_callback;
	vmp->vm_callback = NULL;
	cb(vmp, m);
	return SUSPEND;
}

