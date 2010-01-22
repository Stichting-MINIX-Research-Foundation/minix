/* This file contains device independent device driver interface.
 *
 * Changes:
 *   Jul 25, 2005   added SYS_SIG type for signals  (Jorrit N. Herder)
 *   Sep 15, 2004   added SYN_ALARM type for timeouts  (Jorrit N. Herder)
 *   Jul 23, 2004   removed kernel dependencies  (Jorrit N. Herder)
 *   Apr 02, 1992   constructed from AT wini and floppy driver  (Kees J. Bot)
 *
 *
 * The drivers support the following operations (using message format m2):
 *
 *    m_type         DEVICE   IO_ENDPT   COUNT   POSITION  HIGHPOS   IO_GRANT
 * ----------------------------------------------------------------------------
 * | DEV_OPEN      | device | proc nr |         |        |        |           |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_CLOSE     | device | proc nr |         |        |        |           |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_READ_S    | device | proc nr |  bytes  | off lo | off hi i buf grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_WRITE_S   | device | proc nr |  bytes  | off lo | off hi | buf grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_GATHER_S  | device | proc nr | iov len | off lo | off hi | iov grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_SCATTER_S | device | proc nr | iov len | off lo | off hi | iov grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | DEV_IOCTL_S   | device | proc nr | request |        |        | buf grant |
 * |---------------+--------+---------+---------+--------+--------+-----------|
 * | CANCEL        | device | proc nr |   r/w   |        |        |           |
 * ----------------------------------------------------------------------------
 *
 * The file contains the following entry points:
 *
 *   driver_task:	called by the device dependent task entry
 *   init_buffer:	initialize a DMA buffer
 *   mq_queue:		queue an incoming message for later processing
 */


#include "../drivers.h"
#include <sys/ioc_disk.h>
#include <minix/mq.h>
#include <minix/endpoint.h>
#include "driver.h"

/* Claim space for variables. */
u8_t *tmp_buf = NULL;		/* the DMA buffer eventually */
phys_bytes tmp_phys;		/* phys address of DMA buffer */

FORWARD _PROTOTYPE( void asyn_reply, (message *mess, int proc_nr, int r) );
FORWARD _PROTOTYPE( int do_rdwt, (struct driver *dr, message *mp) );
FORWARD _PROTOTYPE( int do_vrdwt, (struct driver *dr, message *mp) );

int device_caller;
PRIVATE mq_t *queue_head = NULL;

/*===========================================================================*
 *				asyn_reply				     *
 *===========================================================================*/
PRIVATE void asyn_reply(mess, proc_nr, r)
message *mess;
int proc_nr;
int r;
{
/* Send a reply using the new asynchronous character device protocol.
 */
  message reply_mess;

  switch (mess->m_type) {
  case DEV_OPEN:
	reply_mess.m_type = DEV_REVIVE;
	reply_mess.REP_ENDPT = proc_nr;
	reply_mess.REP_STATUS = r;
	break;

  case DEV_CLOSE:
	reply_mess.m_type = DEV_CLOSE_REPL;
	reply_mess.REP_ENDPT = proc_nr;
	reply_mess.REP_STATUS = r;
	break;

  case DEV_READ_S:
  case DEV_WRITE_S:
	if (r == SUSPEND)
		printf("driver_task: reviving %d with SUSPEND\n", proc_nr);

	reply_mess.m_type = DEV_REVIVE;
	reply_mess.REP_ENDPT = proc_nr;
	reply_mess.REP_IO_GRANT = (cp_grant_id_t) mess->IO_GRANT;
	reply_mess.REP_STATUS = r;
	break;

  case CANCEL:
	/* The original request should send a reply. */
	return;

  case DEV_SELECT:
	reply_mess.m_type = DEV_SEL_REPL1;
	reply_mess.DEV_MINOR = mess->DEVICE;
	reply_mess.DEV_SEL_OPS = r;
	break;

  default:
	reply_mess.m_type = TASK_REPLY;
	reply_mess.REP_ENDPT = proc_nr;
	/* Status is # of bytes transferred or error code. */
	reply_mess.REP_STATUS = r;
	break;
  }

  r= asynsend(device_caller, &reply_mess);
  if (r != OK)
  {
	printf("driver_task: unable to asynsend to %d: %d\n",
		device_caller, r);
  }
}

/*===========================================================================*
 *				driver_task				     *
 *===========================================================================*/
PUBLIC void driver_task(dp, type)
struct driver *dp;	/* Device dependent entry points. */
int type;		/* Driver type (DRIVER_STD or DRIVER_ASYN) */
{
/* Main program of any device driver task. */

  int r, proc_nr;
  message mess;
  sigset_t set;

  /* Init MQ library. */
  mq_init();

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (TRUE) {
	/* Any queued messages? Oldest are at the head. */
	if(queue_head) {
		mq_t *mq;
		mq = queue_head;
		memcpy(&mess, &mq->mq_mess, sizeof(mess));
		queue_head = queue_head->mq_next;
		mq_free(mq);
	} else {
		int s;
		/* Wait for a request to read or write a disk block. */
		if ((s=sef_receive(ANY, &mess)) != OK)
        		panic((*dp->dr_name)(),"sef_receive() failed", s);
	}

	device_caller = mess.m_source;
	proc_nr = mess.IO_ENDPT;

	/* Now carry out the work. */
	if (is_notify(mess.m_type)) {
		switch (_ENDPOINT_P(mess.m_source)) {
			case HARDWARE:
				/* leftover interrupt or expired timer. */
				if(dp->dr_hw_int) {
					(*dp->dr_hw_int)(dp, &mess);
				}
				break;
			case PM_PROC_NR:
				if (getsigset(&set) != 0) break;
				(*dp->dr_signal)(dp, &set);
				break;
			case SYSTEM:
				set = mess.NOTIFY_ARG;
				(*dp->dr_signal)(dp, &set);
				break;
			case CLOCK:
				(*dp->dr_alarm)(dp, &mess);	
				break;
			default:		
				if(dp->dr_other)
					r = (*dp->dr_other)(dp, &mess);
				else	
					r = EINVAL;
				goto send_reply;
		}

		/* done, get a new message */
		continue;
	}
	switch(mess.m_type) {
	case DEV_OPEN:		r = (*dp->dr_open)(dp, &mess);	break;	
	case DEV_CLOSE:		r = (*dp->dr_close)(dp, &mess);	break;
	case DEV_IOCTL_S:	r = (*dp->dr_ioctl)(dp, &mess); break;
	case CANCEL:		r = (*dp->dr_cancel)(dp, &mess);break;
	case DEV_SELECT:	r = (*dp->dr_select)(dp, &mess);break;
	case DEV_READ_S:	
	case DEV_WRITE_S:  	r = do_rdwt(dp, &mess); break;
	case DEV_GATHER_S: 
	case DEV_SCATTER_S: 	r = do_vrdwt(dp, &mess); break;

	default:		
		if(dp->dr_other)
			r = (*dp->dr_other)(dp, &mess);
		else	
			r = EINVAL;
		break;
	}

send_reply:
	/* Clean up leftover state. */
	(*dp->dr_cleanup)();

	/* Finally, prepare and send the reply message. */
	if (r == EDONTREPLY)
		continue;

	switch (type) {
	case DRIVER_STD:
		mess.m_type = TASK_REPLY;
		mess.REP_ENDPT = proc_nr;
		/* Status is # of bytes transferred or error code. */
		mess.REP_STATUS = r;

		/* Changed from sendnb() to asynsend() by dcvmoole on 20091129.
		 * This introduces a potential overflow if a single process is
		 * flooding us with requests, but we need reliable delivery of
		 * reply messages for the 'filter' driver. A possible solution
		 * would be to allow only one pending asynchronous reply to a
		 * single process at any time. FIXME.
		 */
		r= asynsend(device_caller, &mess);
		if (r != OK)
		{
			printf("driver_task: unable to send reply to %d: %d\n",
				device_caller, r);
		}

		break;

	case DRIVER_ASYN:
		asyn_reply(&mess, proc_nr, r);

		break;

	default:
		panic(__FILE__, "unknown driver type", type);
	}
  }
}


/*===========================================================================*
 *				init_buffer				     *
 *===========================================================================*/
PUBLIC void init_buffer(void)
{
/* Select a buffer that can safely be used for DMA transfers.  It may also
 * be used to read partition tables and such.  Its absolute address is
 * 'tmp_phys', the normal address is 'tmp_buf'.
 */

  if(!(tmp_buf = alloc_contig(2*DMA_BUF_SIZE, AC_ALIGN4K, &tmp_phys)))
	panic(__FILE__, "can't allocate tmp_buf", DMA_BUF_SIZE);
}

/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
PRIVATE int do_rdwt(dp, mp)
struct driver *dp;		/* device dependent entry points */
message *mp;			/* pointer to read or write message */
{
/* Carry out a single read or write request. */
  iovec_t iovec1;
  int r, opcode;
  u64_t position;

  /* Disk address?  Address and length of the user buffer? */
  if (mp->COUNT < 0) return(EINVAL);

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);

  /* Create a one element scatter/gather vector for the buffer. */
  if(mp->m_type == DEV_READ_S) opcode = DEV_GATHER_S;
  else	opcode =  DEV_SCATTER_S;

  iovec1.iov_addr = (vir_bytes) mp->IO_GRANT;
  iovec1.iov_size = mp->COUNT;

  /* Transfer bytes from/to the device. */
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*dp->dr_transfer)(mp->IO_ENDPT, opcode, position, &iovec1, 1);

  /* Return the number of bytes transferred or an error code. */
  return(r == OK ? (mp->COUNT - iovec1.iov_size) : r);
}

/*==========================================================================*
 *				do_vrdwt				    *
 *==========================================================================*/
PRIVATE int do_vrdwt(dp, mp)
struct driver *dp;	/* device dependent entry points */
message *mp;		/* pointer to read or write message */
{
/* Carry out an device read or write to/from a vector of user addresses.
 * The "user addresses" are assumed to be safe, i.e. FS transferring to/from
 * its own buffers, so they are not checked.
 */
  static iovec_t iovec[NR_IOREQS];
  phys_bytes iovec_size;
  unsigned nr_req;
  int r, opcode;
  u64_t position;

  nr_req = mp->COUNT;	/* Length of I/O vector */

  /* Copy the vector from the caller to kernel space. */
  if (nr_req > NR_IOREQS) nr_req = NR_IOREQS;
  iovec_size = (phys_bytes) (nr_req * sizeof(iovec[0]));

  if (OK != sys_safecopyfrom(mp->m_source, (vir_bytes) mp->IO_GRANT, 
		0, (vir_bytes) iovec, iovec_size, D)) {
	panic((*dp->dr_name)(),"bad I/O vector by", mp->m_source);
  }

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);

  /* Transfer bytes from/to the device. */
  opcode = mp->m_type;
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*dp->dr_transfer)(mp->IO_ENDPT, opcode, position, iovec, nr_req);

  /* Copy the I/O vector back to the caller. */
  if (OK != sys_safecopyto(mp->m_source, (vir_bytes) mp->IO_GRANT, 
		0, (vir_bytes) iovec, iovec_size, D)) {
	panic((*dp->dr_name)(),"couldn't return I/O vector", mp->m_source);
  }

  return(r);
}

/*===========================================================================*
 *				no_name					     *
 *===========================================================================*/
PUBLIC char *no_name()
{
/* Use this default name if there is no specific name for the device. This was
 * originally done by fetching the name from the task table for this process: 
 * "return(tasktab[proc_number(proc_ptr) + NR_TASKS].name);", but currently a
 * real "noname" is returned. Perhaps, some system information service can be
 * queried for a name at a later time.
 */
  static char name[] = "noname";
  return name;
}

/*============================================================================*
 *				do_nop					      *
 *============================================================================*/
PUBLIC int do_nop(dp, mp)
struct driver *dp;
message *mp;
{
/* Nothing there, or nothing to do. */

  switch (mp->m_type) {
  case DEV_OPEN:	return(ENODEV);
  case DEV_CLOSE:	return(OK);
  case DEV_IOCTL_S:	
  default:		printf("nop: ignoring code %d\n", mp->m_type);
			return(EIO);
  }
}

/*============================================================================*
 *				nop_ioctl				      *
 *============================================================================*/
PUBLIC int nop_ioctl(dp, mp)
struct driver *dp;
message *mp;
{
  return(ENOTTY);
}

/*============================================================================*
 *				nop_signal			  	      *
 *============================================================================*/
PUBLIC void nop_signal(dp, set)
struct driver *dp;
sigset_t *set;
{
/* Default action for signal is to ignore. */
}

/*============================================================================*
 *				nop_alarm				      *
 *============================================================================*/
PUBLIC void nop_alarm(dp, mp)
struct driver *dp;
message *mp;
{
/* Ignore the leftover alarm. */
}

/*===========================================================================*
 *				nop_prepare				     *
 *===========================================================================*/
PUBLIC struct device *nop_prepare(device)
{
/* Nothing to prepare for. */
  return(NIL_DEV);
}

/*===========================================================================*
 *				nop_cleanup				     *
 *===========================================================================*/
PUBLIC void nop_cleanup()
{
/* Nothing to clean up. */
}

/*===========================================================================*
 *				nop_cancel				     *
 *===========================================================================*/
PUBLIC int nop_cancel(struct driver *dr, message *m)
{
/* Nothing to do for cancel. */
   return(OK);
}

/*===========================================================================*
 *				nop_select				     *
 *===========================================================================*/
PUBLIC int nop_select(struct driver *dr, message *m)
{
/* Nothing to do for select. */
   return(OK);
}

/*============================================================================*
 *				do_diocntl				      *
 *============================================================================*/
PUBLIC int do_diocntl(dp, mp)
struct driver *dp;
message *mp;			/* pointer to ioctl request */
{
/* Carry out a partition setting/getting request. */
  struct device *dv;
  struct partition entry;
  int s;

  if (mp->REQUEST != DIOCSETP && mp->REQUEST != DIOCGETP) {
  	if(dp->dr_other) {
  		return dp->dr_other(dp, mp);
  	} else return(ENOTTY);
  }

  /* Decode the message parameters. */
  if ((dv = (*dp->dr_prepare)(mp->DEVICE)) == NIL_DEV) return(ENXIO);

  if (mp->REQUEST == DIOCSETP) {
	/* Copy just this one partition table entry. */
	s=sys_safecopyfrom(mp->IO_ENDPT, (vir_bytes) mp->IO_GRANT, 
		0, (vir_bytes) &entry, sizeof(entry), D);
	if(s != OK)
	    return s;
	dv->dv_base = entry.base;
	dv->dv_size = entry.size;
  } else {
	/* Return a partition table entry and the geometry of the drive. */
	entry.base = dv->dv_base;
	entry.size = dv->dv_size;
	(*dp->dr_geometry)(&entry);
	s=sys_safecopyto(mp->IO_ENDPT, (vir_bytes) mp->IO_GRANT, 
		0, (vir_bytes) &entry, sizeof(entry), D);
        if (OK != s) 
	    return s;
  }
  return(OK);
}

/*===========================================================================*
 *				mq_queue				     *
 *===========================================================================*/
PUBLIC int mq_queue(message *m)
{
	mq_t *mq, *mi;

	if(!(mq = mq_get()))
        	panic("libdriver","mq_queue: mq_get failed", NO_NUM);
	memcpy(&mq->mq_mess, m, sizeof(mq->mq_mess));
	mq->mq_next = NULL;
	if(!queue_head) {
		queue_head = mq;
	} else {
		for(mi = queue_head; mi->mq_next; mi = mi->mq_next)
			;
		mi->mq_next = mq;
	}

	return OK;
}

