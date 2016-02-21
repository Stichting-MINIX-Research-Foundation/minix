#include "fs.h"
#include <minix/vfsif.h>
#include <assert.h>
#include <string.h>

static int sendmsg(struct vmnt *vmp, endpoint_t dst, struct worker_thread *wp);
static int queuemsg(struct vmnt *vmp);

/*===========================================================================*
 *				sendmsg					     *
 *===========================================================================*/
static int sendmsg(struct vmnt *vmp, endpoint_t dst, struct worker_thread *wp)
{
/* This is the low level function that sends requests.
 * Currently to FSes or VM.
 */
  int r, transid;

  if(vmp) vmp->m_comm.c_cur_reqs++;	/* One more request awaiting a reply */
  transid = wp->w_tid + VFS_TRANSID;
  wp->w_sendrec->m_type = TRNS_ADD_ID(wp->w_sendrec->m_type, transid);
  wp->w_task = dst;
  if ((r = asynsend3(dst, wp->w_sendrec, AMF_NOREPLY)) != OK) {
	printf("VFS: sendmsg: error sending message. "
		"dest: %d req_nr: %d err: %d\n", dst,
			wp->w_sendrec->m_type, r);
	util_stacktrace();
	return(r);
  }

  return(r);
}

/*===========================================================================*
 *				send_work				     *
 *===========================================================================*/
void send_work(void)
{
/* Try to send out as many requests as possible */
  struct vmnt *vmp;

  if (sending == 0) return;
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++)
	fs_sendmore(vmp);
}

/*===========================================================================*
 *				fs_cancel				     *
 *===========================================================================*/
void fs_cancel(struct vmnt *vmp)
{
/* Cancel all pending requests for this vmp */
  struct worker_thread *worker;

  while ((worker = vmp->m_comm.c_req_queue) != NULL) {
	vmp->m_comm.c_req_queue = worker->w_next;
	worker->w_next = NULL;
	sending--;
	worker_stop(worker);
  }
}

/*===========================================================================*
 *				fs_sendmore				     *
 *===========================================================================*/
void fs_sendmore(struct vmnt *vmp)
{
  struct worker_thread *worker;

  /* Can we send more requests? */
  if (vmp->m_fs_e == NONE) return;
  if ((worker = vmp->m_comm.c_req_queue) == NULL) /* No process is queued */
	return;
  if (vmp->m_comm.c_cur_reqs >= vmp->m_comm.c_max_reqs)/*No room to send more*/
	return;
  if (vmp->m_flags & VMNT_CALLBACK)	/* Hold off for now */
	return;

  vmp->m_comm.c_req_queue = worker->w_next; /* Remove head */
  worker->w_next = NULL;
  sending--;
  assert(sending >= 0);
  (void) sendmsg(vmp, vmp->m_fs_e, worker);
}

/*===========================================================================*
 *				drv_sendrec				     *
 *===========================================================================*/
int drv_sendrec(endpoint_t drv_e, message *reqmp)
{
	int r;
	struct dmap *dp;

	/* For the CTTY_MAJOR case, we would actually have to lock the device
	 * entry being redirected to.  However, the CTTY major only hosts a
	 * character device while this function is used only for block devices.
	 * Thus, we can simply deny the request immediately.
	 */
	if (drv_e == CTTY_ENDPT) {
		printf("VFS: /dev/tty is not a block device!\n");
		return EIO;
	}

	if ((dp = get_dmap_by_endpt(drv_e)) == NULL)
		panic("driver endpoint %d invalid", drv_e);

	lock_dmap(dp);
	if (dp->dmap_servicing != INVALID_THREAD)
		panic("driver locking inconsistency");
	dp->dmap_servicing = self->w_tid;
	self->w_task = drv_e;
	self->w_drv_sendrec = reqmp;

	if ((r = asynsend3(drv_e, self->w_drv_sendrec, AMF_NOREPLY)) == OK) {
		/* Yield execution until we've received the reply */
		worker_wait();

	} else {
		printf("VFS: drv_sendrec: error sending msg to driver %d: %d\n",
			drv_e, r);
		self->w_drv_sendrec = NULL;
	}

	assert(self->w_drv_sendrec == NULL);
	dp->dmap_servicing = INVALID_THREAD;
	self->w_task = NONE;
	unlock_dmap(dp);
	return(r);
}

/*===========================================================================*
 *				fs_sendrec				     *
 *===========================================================================*/
int fs_sendrec(endpoint_t fs_e, message *reqmp)
{
  struct vmnt *vmp;
  int r;

  if ((vmp = find_vmnt(fs_e)) == NULL) {
	printf("Trying to talk to non-existent FS endpoint %d\n", fs_e);
	return(EIO);
  }
  if (fs_e == fp->fp_endpoint) return(EDEADLK);

  assert(self->w_sendrec == NULL);
  self->w_sendrec = reqmp;	/* Where to store request and reply */

  /* Find out whether we can send right away or have to enqueue */
  if (	!(vmp->m_flags & VMNT_CALLBACK) &&
	vmp->m_comm.c_cur_reqs < vmp->m_comm.c_max_reqs) {
	/* There's still room to send more and no proc is queued */
	r = sendmsg(vmp, vmp->m_fs_e, self);
  } else {
	r = queuemsg(vmp);
  }
  self->w_next = NULL;	/* End of list */

  if (r != OK) return(r);

  worker_wait();	/* Yield execution until we've received the reply. */

  assert(self->w_sendrec == NULL);

  r = reqmp->m_type;
  if (r == ERESTART)	/* ERESTART is used internally, so make sure it is.. */
	r = EIO;	/* ..not delivered as a result from a file system. */
  return(r);
}

/*===========================================================================*
 *				vm_sendrec				     *
 *===========================================================================*/
int vm_sendrec(message *reqmp)
{
  int r;

  assert(self);
  assert(reqmp);

  assert(self->w_sendrec == NULL);
  self->w_sendrec = reqmp;	/* Where to store request and reply */

  r = sendmsg(NULL, VM_PROC_NR, self);

  self->w_next = NULL;	/* End of list */

  if (r != OK) return(r);

  worker_wait();	/* Yield execution until we've received the reply. */

  assert(self->w_sendrec == NULL);

  return(reqmp->m_type);
}


/*===========================================================================*
 *                                vm_vfs_procctl_handlemem                   *
 *===========================================================================*/
int vm_vfs_procctl_handlemem(endpoint_t ep,
        vir_bytes mem, vir_bytes len, int flags)
{
    message m;

    /* main thread can not be suspended */
    if(!self) return EFAULT;

    memset(&m, 0, sizeof(m));

    m.m_type = VM_PROCCTL;
    m.VMPCTL_WHO = ep;
    m.VMPCTL_PARAM = VMPPARAM_HANDLEMEM;
    m.VMPCTL_M1 = mem;
    m.VMPCTL_LEN = len;
    m.VMPCTL_FLAGS = flags;

    return vm_sendrec(&m);
}

/*===========================================================================*
 *				queuemsg				     *
 *===========================================================================*/
static int queuemsg(struct vmnt *vmp)
{
/* Put request on queue for vmnt */

  struct worker_thread *queue;

  if (vmp->m_comm.c_req_queue == NULL) {
	vmp->m_comm.c_req_queue = self;
  } else {
	/* Walk the list ... */
	queue = vmp->m_comm.c_req_queue;
	while (queue->w_next != NULL) queue = queue->w_next;

	/* ... and append this worker */
	queue->w_next = self;
  }

  self->w_next = NULL;	/* End of list */
  sending++;

  return(OK);
}
