
/* Sending requests to VFS and handling the replies.  */

#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/type.h>
#include <minix/bitmap.h>
#include <string.h>
#include <errno.h>
#include <env.h>
#include <unistd.h>
#include <assert.h>
#include <sys/param.h>

#include "proto.h"
#include "glo.h"
#include "util.h"
#include "region.h"
#include "sanitycheck.h"

#define STATELEN 70

static struct vfs_request_node {
	message			reqmsg;
	char			reqstate[STATELEN];
	void			*opaque;
	endpoint_t		who;
	int			req_id;
	vfs_callback_t		callback;
	struct vfs_request_node	*next;
} *first_queued, *active;

static void activate(void)
{
	assert(!active);
	assert(first_queued);

	active = first_queued;
	first_queued = first_queued->next;

	if(asynsend3(VFS_PROC_NR, &active->reqmsg, AMF_NOREPLY) != OK)
		panic("VM: asynsend to VFS failed");
}

#define ID_MAX LONG_MAX

/*===========================================================================*
 *                              vfs_request                                 *
 *===========================================================================*/
int vfs_request(int reqno, int fd, struct vmproc *vmp, u64_t offset, u32_t len,
	vfs_callback_t reply_callback, void *cbarg, void *state, int statelen)
{
/* Perform an asynchronous request to VFS.
 * We send a message of type VFS_VMCALL to VFS. VFS will respond
 * with message type VM_VFS_REPLY. We send the request asynchronously
 * and then handle the reply as it if were a VM_VFS_REPLY request.
 */
 	message *m;
	static int reqid = 0;
	struct vfs_request_node *reqnode;

	reqid++;

	assert(statelen <= STATELEN);

	if(!SLABALLOC(reqnode)) {
		printf("vfs_request: no memory for request node\n");
		return ENOMEM;
	}

	m = &reqnode->reqmsg;
	memset(m, 0, sizeof(*m));
	m->m_type = VFS_VMCALL;
	m->VFS_VMCALL_REQ = reqno;
	m->VFS_VMCALL_FD = fd;
	m->VFS_VMCALL_REQID = reqid;
	m->VFS_VMCALL_ENDPOINT = vmp->vm_endpoint;
	m->VFS_VMCALL_OFFSET = offset;
	m->VFS_VMCALL_LENGTH = len;

	reqnode->who = vmp->vm_endpoint;
	reqnode->req_id = reqid;
	reqnode->next = first_queued;
	reqnode->callback = reply_callback;
	reqnode->opaque = cbarg;
	if(state) memcpy(reqnode->reqstate, state, statelen);
	first_queued = reqnode;

	/* Send the request message if none pending. */
	if(!active)
		activate();

	return OK;
}

/*===========================================================================*
 *                              do_vfs_reply                                 *
 *===========================================================================*/
int do_vfs_reply(message *m)
{
/* VFS has handled a VM request and VFS has replied. It must be the
 * active request.
 */
 	struct vfs_request_node *orignode = active;
 	vfs_callback_t req_callback;
	void *cbarg;
	int n;
	struct vmproc *vmp;

	assert(active);
	assert(active->req_id == m->VMV_REQID);

	/* the endpoint may have exited */
	if(vm_isokendpt(m->VMV_ENDPOINT, &n) != OK)
		vmp = NULL;
	else	vmp = &vmproc[n];

	req_callback = active->callback;
	cbarg = active->opaque;
	active = NULL;

	/* Invoke requested reply-callback within VM. */
	if(req_callback) req_callback(vmp, m, cbarg, orignode->reqstate);

	SLABFREE(orignode);

	/* Send the next request message if any and not re-activated. */
	if(first_queued && !active)
		activate();

	return SUSPEND;	/* don't reply to the reply */
}

