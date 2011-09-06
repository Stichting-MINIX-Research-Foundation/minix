#include "fs.h"
#include "glo.h"
#include "vmnt.h"
#include "fproc.h"
#include <minix/vfsif.h>
#include <assert.h>

FORWARD _PROTOTYPE( int sendmsg, (struct vmnt *vmp, struct fproc *rfp)	);
FORWARD _PROTOTYPE( int queuemsg, (struct vmnt *vmp)			);

/*===========================================================================*
 *				sendmsg					     *
 *===========================================================================*/
PRIVATE int sendmsg(vmp, rfp)
struct vmnt *vmp;
struct fproc *rfp;
{
/* This is the low level function that sends requests to FS processes.
 */
  int r, transid;

  if (vmp->m_fs_e == rfp->fp_endpoint) return(EDEADLK);
  vmp->m_comm.c_cur_reqs++;	/* One more request awaiting a reply */

  transid = rfp->fp_wtid + VFS_TRANSID;
  rfp->fp_sendrec->m_type = TRNS_ADD_ID(rfp->fp_sendrec->m_type, transid);
  rfp->fp_task = vmp->m_fs_e;
  if ((r = asynsend3(vmp->m_fs_e, rfp->fp_sendrec, AMF_NOREPLY)) != OK) {
	printf("VFS: sendmsg: error sending message. "
	       "FS_e: %d req_nr: %d err: %d\n", vmp->m_fs_e,
	       rfp->fp_sendrec->m_type, r);
		util_stacktrace();
	return(r);
  }

  return(r);
}

/*===========================================================================*
 *				send_work				     *
 *===========================================================================*/
PUBLIC void send_work(void)
{
/* Try to send out as many requests as possible */
  struct vmnt *vmp;

  if (sending == 0) return;
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++)
	fs_sendmore(vmp);
}

/*===========================================================================*
 *				fs_sendmore				     *
 *===========================================================================*/
PUBLIC void fs_sendmore(struct vmnt *vmp)
{
  struct worker_thread *worker;

  /* Can we send more requests? */
  if (vmp->m_fs_e == NONE) return;
  if ((worker = vmp->m_comm.c_req_queue) == NULL) /* No process is queued */
	return;
  if (vmp->m_comm.c_cur_reqs >= vmp->m_comm.c_max_reqs)/*No room to send more*/
	return;
  if (vmp->m_flags & VMNT_BACKCALL)	/* Hold off for now */
	return;

  vmp->m_comm.c_req_queue = worker->w_next; /* Remove head */
  worker->w_next = NULL;
  sending--;
  assert(sending >= 0);
  sendmsg(vmp, worker->w_job.j_fp);
}

/*===========================================================================*
 *				fs_sendrec				     *
 *===========================================================================*/
PUBLIC int fs_sendrec(endpoint_t fs_e, message *reqmp)
{
  struct vmnt *vmp;
  int r;

  if ((vmp = find_vmnt(fs_e)) == NULL)
	panic("Trying to talk to non-existent FS");

  if (!force_sync) {
	fp->fp_sendrec = reqmp;	/* Where to store request and reply */

	/* Find out whether we can send right away or have to enqueue */
	if (	!(vmp->m_flags & VMNT_BACKCALL) &&
		vmp->m_comm.c_cur_reqs < vmp->m_comm.c_max_reqs) {
		/* There's still room to send more and no proc is queued */
		r = sendmsg(vmp, fp);
	} else {
		r = queuemsg(vmp);
	}
	self->w_next = NULL;	/* End of list */

	if (r != OK) return(r);

	worker_wait();	/* Yield execution until we've received the reply. */
  } else if (force_sync == 1) {
	int r;
	if (OK != (r = sendrec(fs_e, reqmp))) {
		printf("VFS: sendrec failed: %d\n", r);
		util_stacktrace();
		return(r);
	}
  } else if (force_sync == 2) {
	int r, status;
	if (OK != (r = asynsend(fs_e, reqmp)) ||
	    OK != (r = receive(fs_e, reqmp, &status))) {
		printf("VFS: asynrec failed: %d\n", r);
		util_stacktrace();
		return(r);
	}
  } else if (force_sync == 3) {
	int r, status;
	if (OK != (r = send(fs_e, reqmp)) ||
	    OK != (r = receive(fs_e, reqmp, &status))) {
		printf("VFS: sendreceive failed: %d\n", r);
		util_stacktrace();
		return(r);
	}
  }

  if (reqmp->m_type == -EENTERMOUNT || reqmp->m_type == -ELEAVEMOUNT ||
      reqmp->m_type == -ESYMLINK) {
	reqmp->m_type = -reqmp->m_type;
  } else if (force_sync != 0 && reqmp->m_type > 0) {
	/* XXX: Keep this as long as we're interested in having support
	 * for synchronous communication. */
	nested_fs_call(reqmp);
	return fs_sendrec(fs_e, reqmp);
  }

  return(reqmp->m_type);
}

/*===========================================================================*
 *				queuemsg				     *
 *===========================================================================*/
PRIVATE int queuemsg(struct vmnt *vmp)
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
