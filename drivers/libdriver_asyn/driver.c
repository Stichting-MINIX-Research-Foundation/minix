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
 *    m_type      DEVICE    IO_ENDPT    COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_READ  | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DEV_GATHER | device  | proc nr | iov len |  offset | iov ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DEV_SCATTER| device  | proc nr | iov len |  offset | iov ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_IOCTL | device  | proc nr |func code|         | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  CANCEL    | device  | proc nr | r/w     |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  HARD_STOP |         |         |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_*_S   | variants using safecopies of above              |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   driver_task:	called by the device dependent task entry
 */


#include "../drivers.h"
#include <sys/ioc_disk.h>
#include <minix/mq.h>
#include "driver.h"

/* Claim space for variables. */
u8_t *tmp_buf = NULL;		/* the DMA buffer eventually */
phys_bytes tmp_phys;		/* phys address of DMA buffer */

FORWARD _PROTOTYPE( void init_buffer, (void) );
FORWARD _PROTOTYPE( int do_rdwt, (struct driver *dr, message *mp, int safe) );
FORWARD _PROTOTYPE( int do_vrdwt, (struct driver *dr, message *mp, int safe) );

_PROTOTYPE( int asynsend, (endpoint_t dst, message *mp));

int device_caller;
PRIVATE mq_t *queue_head = NULL;

/*===========================================================================*
 *				driver_task				     *
 *===========================================================================*/
PUBLIC void driver_task(dp)
struct driver *dp;	/* Device dependent entry points. */
{
/* Main program of any device driver task. */

  int r, proc_nr;
  message mess, reply_mess;

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
		if ((s=receive(ANY, &mess)) != OK)
        		panic((*dp->dr_name)(),"receive() failed", s);
	}

	device_caller = mess.m_source;
	proc_nr = mess.IO_ENDPT;

#if 0
	if (mess.m_type != SYN_ALARM && mess.m_type != DEV_PING &&
		mess.m_type != 4105 /* notify from TTY */ &&
		mess.m_type != DEV_SELECT &&
		mess.m_type != DEV_READ_S &&
		mess.m_type != DIAGNOSTICS_S &&
		mess.m_type != CANCEL)
	{
		printf("libdriver_asyn`driver_task: msg %d / 0x%x from %d\n",
			mess.m_type, mess.m_type, mess.m_source);
	}
#endif

	if (mess.m_type == DEV_SELECT)
	{
		static int first= 1;
		if (first)
		{
			first= 0;
#if 0
			printf(
	"libdriver_asyn`driver_task: first DEV_SELECT: minor 0x%x, ops 0x%x\n",
				mess.DEVICE, mess.IO_ENDPT);
#endif
		}
	}

	/* Now carry out the work. */
	switch(mess.m_type) {
	case DEV_OPEN:		r = (*dp->dr_open)(dp, &mess);	break;	
	case DEV_CLOSE:		r = (*dp->dr_close)(dp, &mess);	break;
#ifdef DEV_IOCTL
	case DEV_IOCTL:		r = (*dp->dr_ioctl)(dp, &mess, 0); break;
#endif
	case DEV_IOCTL_S:	r = (*dp->dr_ioctl)(dp, &mess, 1); break;
	case CANCEL:		r = (*dp->dr_cancel)(dp, &mess);break;
	case DEV_SELECT:	r = (*dp->dr_select)(dp, &mess);break;
#ifdef DEV_READ
	case DEV_READ:	
	case DEV_WRITE:	  	r = do_rdwt(dp, &mess, 0); break;
#endif
	case DEV_READ_S:	
	case DEV_WRITE_S:  	r = do_rdwt(dp, &mess, 1); break;
#ifdef DEV_GATHER
	case DEV_GATHER: 
	case DEV_SCATTER: 	r = do_vrdwt(dp, &mess, 0); break;
#endif
	case DEV_GATHER_S: 
	case DEV_SCATTER_S: 	r = do_vrdwt(dp, &mess, 1); break;

	case HARD_INT:		/* leftover interrupt or expired timer. */
				if(dp->dr_hw_int) {
					(*dp->dr_hw_int)(dp, &mess);
				}
				continue;
	case PROC_EVENT:
	case SYS_SIG:		(*dp->dr_signal)(dp, &mess);
				continue;	/* don't reply */
	case SYN_ALARM:		(*dp->dr_alarm)(dp, &mess);	
				continue;	/* don't reply */
	case DEV_PING:		notify(mess.m_source);
				continue;
	default:		
		if(dp->dr_other)
			r = (*dp->dr_other)(dp, &mess, 0);
		else	
			r = EINVAL;
		break;
	}

	/* Clean up leftover state. */
	(*dp->dr_cleanup)();

	/* Finally, prepare and send the reply message. */
	if (r != EDONTREPLY) {
		if (mess.m_type == DEV_OPEN)
		{
			reply_mess.m_type = DEV_REVIVE;
			reply_mess.REP_ENDPT = proc_nr;
			reply_mess.REP_STATUS = r;	
		}
		else if (mess.m_type == DEV_CLOSE)
		{
			reply_mess.m_type = DEV_CLOSE_REPL;
			reply_mess.REP_ENDPT = proc_nr;
			reply_mess.REP_STATUS = r;	
		}
		else if (mess.m_type == DEV_READ_S ||
			mess.m_type == DEV_WRITE_S)
		{
			if (r == SUSPEND)
			{
				printf(
				"driver_task: reviving %d with SUSPEND\n",
					proc_nr);
			}
			reply_mess.m_type = DEV_REVIVE;
			reply_mess.REP_ENDPT = proc_nr;
			reply_mess.REP_IO_GRANT = (cp_grant_id_t)mess.ADDRESS;
			reply_mess.REP_STATUS = r;	
		}
		else if (mess.m_type == CANCEL)
		{
			continue;	/* The original request should send a
					 * reply.
					 */
		}
		else if (mess.m_type == DEV_SELECT)
		{
			reply_mess.m_type = DEV_SEL_REPL1;
			reply_mess.DEV_MINOR = mess.DEVICE;
			reply_mess.DEV_SEL_OPS = r;	
		}
		else if (mess.m_type == DIAGNOSTICS_S)
		{
#if 0
			if (device_caller == FS_PROC_NR)
				printf("driver_task: sending DIAG_REPL to FS\n");
#endif
			reply_mess.m_type = DIAG_REPL;
			reply_mess.REP_STATUS = r;	
		}
		else
		{
#if 0
			printf("driver_task: TASK_REPLY to req %d\n",
				mess.m_type);
#endif
			reply_mess.m_type = TASK_REPLY;
			reply_mess.REP_ENDPT = proc_nr;
			/* Status is # of bytes transferred or error code. */
			reply_mess.REP_STATUS = r;	
		}
		r= asynsend(device_caller, &reply_mess);
		if (r != OK)
		{
			printf("driver_task: unable to asynsend to %d: %d\n",
				device_caller, r);
		}
	}
  }
}


/*===========================================================================*
 *				init_buffer				     *
 *===========================================================================*/
PRIVATE void init_buffer()
{
/* Select a buffer that can safely be used for DMA transfers.  It may also
 * be used to read partition tables and such.  Its absolute address is
 * 'tmp_phys', the normal address is 'tmp_buf'.
 */

  unsigned left;

  if(!(tmp_buf = alloc_contig(2*DMA_BUF_SIZE, 0, &tmp_phys))) {
	panic(__FILE__, "can't allocate tmp_buf", NO_NUM);
  }
}

/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
PRIVATE int do_rdwt(dp, mp, safe)
struct driver *dp;		/* device dependent entry points */
message *mp;			/* pointer to read or write message */
int safe;			/* use safecopies? */
{
/* Carry out a single read or write request. */
  iovec_t iovec1;
  int r, opcode;
  phys_bytes phys_addr;
  u64_t position;

  /* Disk address?  Address and length of the user buffer? */
  if (mp->COUNT < 0) return(EINVAL);

  /* Check the user buffer (not relevant for safe copies). */
  if(!safe) {
	  sys_umap(mp->IO_ENDPT, D, (vir_bytes) mp->ADDRESS, mp->COUNT, &phys_addr);
	  if (phys_addr == 0) return(EFAULT);
  }

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);

  /* Create a one element scatter/gather vector for the buffer. */
  if(
#ifdef DEV_READ
  mp->m_type == DEV_READ || 
#endif
  mp->m_type == DEV_READ_S) opcode = DEV_GATHER_S;
  else	opcode =  DEV_SCATTER_S;

  iovec1.iov_addr = (vir_bytes) mp->ADDRESS;
  iovec1.iov_size = mp->COUNT;

  /* Transfer bytes from/to the device. */
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*dp->dr_transfer)(mp->IO_ENDPT, opcode, position, &iovec1, 1, safe);

  /* Return the number of bytes transferred or an error code. */
  return(r == OK ? (mp->COUNT - iovec1.iov_size) : r);
}

/*==========================================================================*
 *				do_vrdwt				    *
 *==========================================================================*/
PRIVATE int do_vrdwt(dp, mp, safe)
struct driver *dp;	/* device dependent entry points */
message *mp;		/* pointer to read or write message */
int safe;		/* use safecopies? */
{
/* Carry out an device read or write to/from a vector of user addresses.
 * The "user addresses" are assumed to be safe, i.e. FS transferring to/from
 * its own buffers, so they are not checked.
 */
  static iovec_t iovec[NR_IOREQS];
  iovec_t *iov;
  phys_bytes iovec_size;
  unsigned nr_req;
  int r, j, opcode;
  u64_t position;

  nr_req = mp->COUNT;	/* Length of I/O vector */

  {
    /* Copy the vector from the caller to kernel space. */
    if (nr_req > NR_IOREQS) nr_req = NR_IOREQS;
    iovec_size = (phys_bytes) (nr_req * sizeof(iovec[0]));

    if(safe) {
	    if (OK != sys_safecopyfrom(mp->m_source, (vir_bytes) mp->IO_GRANT, 
    			0, (vir_bytes) iovec, iovec_size, D)) {
        	panic((*dp->dr_name)(),"bad (safe) I/O vector by", mp->m_source);
	    }
    } else {
	    if (OK != sys_datacopy(mp->m_source, (vir_bytes) mp->ADDRESS, 
    			SELF, (vir_bytes) iovec, iovec_size)) {
        	panic((*dp->dr_name)(),"bad I/O vector by", mp->m_source);
	    }
    }

    iov = iovec;
  }

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);

  /* Transfer bytes from/to the device. */
  opcode = mp->m_type;
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*dp->dr_transfer)(mp->IO_ENDPT, opcode, position, iov,
	nr_req, safe);

  /* Copy the I/O vector back to the caller. */
  if(safe) {
    if (OK != sys_safecopyto(mp->m_source, (vir_bytes) mp->IO_GRANT, 
    		0, (vir_bytes) iovec, iovec_size, D)) {
        panic((*dp->dr_name)(),"couldn't return I/O vector", mp->m_source);
    }
  } else {
    sys_datacopy(SELF, (vir_bytes) iovec, 
    	mp->m_source, (vir_bytes) mp->ADDRESS, iovec_size);
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
#ifdef DEV_IOCTL
  case DEV_IOCTL:	return(ENOTTY);
#endif
  default:		printf("nop: ignoring code %d\n", mp->m_type); return(EIO);
  }
}

/*============================================================================*
 *				nop_ioctl				      *
 *============================================================================*/
PUBLIC int nop_ioctl(dp, mp, safe)
struct driver *dp;
message *mp;
int safe;
{
  return(ENOTTY);
}

/*============================================================================*
 *				nop_signal			  	      *
 *============================================================================*/
PUBLIC void nop_signal(dp, mp)
struct driver *dp;
message *mp;
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
PUBLIC int do_diocntl(dp, mp, safe)
struct driver *dp;
message *mp;			/* pointer to ioctl request */
int safe;			/* addresses or grants? */
{
/* Carry out a partition setting/getting request. */
  struct device *dv;
  struct partition entry;
  int s;

  if (mp->REQUEST != DIOCSETP && mp->REQUEST != DIOCGETP) {
  	if(dp->dr_other) {
  		return dp->dr_other(dp, mp, safe);
  	} else return(ENOTTY);
  }

  /* Decode the message parameters. */
  if ((dv = (*dp->dr_prepare)(mp->DEVICE)) == NIL_DEV) return(ENXIO);

  if (mp->REQUEST == DIOCSETP) {
	/* Copy just this one partition table entry. */
	if(safe) {
	  s=sys_safecopyfrom(mp->IO_ENDPT, (vir_bytes) mp->IO_GRANT, 
    			0, (vir_bytes) &entry, sizeof(entry), D);
	} else{
	  s=sys_datacopy(mp->IO_ENDPT, (vir_bytes) mp->ADDRESS,
		SELF, (vir_bytes) &entry, sizeof(entry));
	}
	if(s != OK)
	    return s;
	dv->dv_base = entry.base;
	dv->dv_size = entry.size;
  } else {
	/* Return a partition table entry and the geometry of the drive. */
	entry.base = dv->dv_base;
	entry.size = dv->dv_size;
	(*dp->dr_geometry)(&entry);
	if(safe) {
	  s=sys_safecopyto(mp->IO_ENDPT, (vir_bytes) mp->IO_GRANT, 
    			0, (vir_bytes) &entry, sizeof(entry), D);
	} else {
	  s=sys_datacopy(SELF, (vir_bytes) &entry,
		mp->IO_ENDPT, (vir_bytes) mp->ADDRESS, sizeof(entry));
	}
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

#if 0

#define ASYN_NR	100
PRIVATE asynmsg_t msgtable[ASYN_NR];
PRIVATE int first_slot= 0, next_slot= 0;

PUBLIC int asynsend(dst, mp)
endpoint_t dst;
message *mp;
{
	int r, src_ind, dst_ind;
	unsigned flags;

	/* Update first_slot */
	for (; first_slot < next_slot; first_slot++)
	{
		flags= msgtable[first_slot].flags;
		if ((flags & (AMF_VALID|AMF_DONE)) == (AMF_VALID|AMF_DONE))
		{
			if (msgtable[first_slot].result != OK)
			{
				printf(
			"asynsend: found completed entry %d with error %d\n",
					first_slot,
					msgtable[first_slot].result);
			}
			continue;
		}
		if (flags != AMF_EMPTY)
			break;
	}

	if (first_slot >= next_slot)
	{
		/* Reset first_slot and next_slot */
		next_slot= first_slot= 0;
	}

	if (next_slot >= ASYN_NR)
	{
		/* Tell the kernel to stop processing */
		r= senda(NULL, 0);
		if (r != OK)
			panic(__FILE__, "asynsend: senda failed", r);

		dst_ind= 0;
		for (src_ind= first_slot; src_ind<next_slot; src_ind++)
		{
			flags= msgtable[src_ind].flags;
			if ((flags & (AMF_VALID|AMF_DONE)) ==
				(AMF_VALID|AMF_DONE))
			{
				if (msgtable[src_ind].result != OK)
				{
					printf(
			"asynsend: found completed entry %d with error %d\n",
						src_ind,
						msgtable[src_ind].result);
				}
				continue;
			}
			if (flags == AMF_EMPTY)
				continue;
#if 0
			printf("asynsend: copying entry %d to %d\n",
				src_ind, dst_ind);
#endif
			if (src_ind != dst_ind)
				msgtable[dst_ind]= msgtable[src_ind];
			dst_ind++;
		}
		first_slot= 0;
		next_slot= dst_ind;
		if (next_slot >= ASYN_NR)
			panic(__FILE__, "asynsend: msgtable full", NO_NUM);
	}

	msgtable[next_slot].dst= dst;
	msgtable[next_slot].msg= *mp;
	msgtable[next_slot].flags= AMF_VALID;	/* Has to be last. The kernel 
					 	 * scans this table while we
						 * are sleeping.
					 	 */
	next_slot++;

	/* Tell the kernel to rescan the table */
	return senda(msgtable+first_slot, next_slot-first_slot);
}

#endif
