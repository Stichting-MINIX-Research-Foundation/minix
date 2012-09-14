#include "fs.h"
#include "glo.h"
#include "vmnt.h"
#include "fproc.h"
#include <minix/vfsif.h>
#include <assert.h>

static int sendmsg(struct vmnt *vmp, struct worker_thread *wp);
static int queuemsg(struct vmnt *vmp);

/*===========================================================================*
 *				sendmsg					     *
 *===========================================================================*/
static int sendmsg(struct vmnt *vmp, struct worker_thread *wp)
{
/* This is the low level function that sends requests to FS processes.
 */
  int r, transid;

  vmp->m_comm.c_cur_reqs++;	/* One more request awaiting a reply */
  transid = wp->w_tid + VFS_TRANSID;
  wp->w_fs_sendrec->m_type = TRNS_ADD_ID(wp->w_fs_sendrec->m_type, transid);
  wp->w_task = vmp->m_fs_e;
  if ((r = asynsend3(vmp->m_fs_e, wp->w_fs_sendrec, AMF_NOREPLY)) != OK) {
	printf("VFS: sendmsg: error sending message. "
		"FS_e: %d req_nr: %d err: %d\n", vmp->m_fs_e,
		wp->w_fs_sendrec->m_type, r);
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
  (void) sendmsg(vmp, worker);
}

/*===========================================================================*
 *				drv_sendrec				     *
 *===========================================================================*/
int drv_sendrec(endpoint_t drv_e, message *reqmp)
{
	int r;
	struct dmap *dp;

	if ((dp = get_dmap(drv_e)) == NULL)
		panic("driver endpoint %d invalid", drv_e);

	lock_dmap(dp);
	if (dp->dmap_servicing != NONE)
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
		util_stacktrace();
	}

	dp->dmap_servicing = NONE;
	self->w_task = NONE;
	self->w_drv_sendrec = NULL;
	unlock_dmap(dp);
	return(OK);
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

  self->w_fs_sendrec = reqmp;	/* Where to store request and reply */

  /* Find out whether we can send right away or have to enqueue */
  if (	!(vmp->m_flags & VMNT_CALLBACK) &&
	vmp->m_comm.c_cur_reqs < vmp->m_comm.c_max_reqs) {
	/* There's still room to send more and no proc is queued */
	r = sendmsg(vmp, self);
  } else {
	r = queuemsg(vmp);
  }
  self->w_next = NULL;	/* End of list */

  if (r != OK) return(r);

  worker_wait();	/* Yield execution until we've received the reply. */

  return(reqmp->m_type);
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
