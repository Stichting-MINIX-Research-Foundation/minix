
#include <minix/config.h>
#include <assert.h> 
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>

#include <stdlib.h>
#include <unistd.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/sys_config.h>

#include <limits.h>
#include <errno.h>

#define ASYN_NR	(2*_NR_PROCS)
static asynmsg_t msgtable[ASYN_NR];
static int first_slot = 0, next_slot = 0;
static int initialized = 0;

#define DEBUG 0

/*===========================================================================*
 *				asynsend3				     *
 *===========================================================================*/
int asynsend3(dst, mp, fl)
endpoint_t dst;
message *mp;
int fl;
{
  int i, r, src_ind, dst_ind;
  unsigned flags;
  static int inside = 0;
  int len, needack = 0;

  /* Debug printf() causes asynchronous sends? */
  if (inside)	/* Panic will not work either then, so exit */
	exit(1);

  inside = 1;

  if(!initialized) {
  	/* Initialize table by marking all entries empty */
	for (i = 0; i < ASYN_NR; i++) msgtable[i].flags = AMF_EMPTY;

	initialized = 1;
  }

  /* Update first_slot. That is, find the first not-completed slot by the
   * kernel since the last time we sent this table (e.g., the receiving end of
   * the message wasn't ready yet).
   */
  for (; first_slot < next_slot; first_slot++) {
	flags = msgtable[first_slot].flags;
	if ((flags & (AMF_VALID|AMF_DONE)) == (AMF_VALID|AMF_DONE)) {
		/* Marked in use by us (VALID) and processed by the kernel */
		if (msgtable[first_slot].result != OK) {
#if DEBUG
			printf("asynsend: found entry %d with error %d\n",
				first_slot, msgtable[first_slot].result);
#endif
			needack = (flags & (AMF_NOTIFY|AMF_NOTIFY_ERR));
		}
		continue;
	}

	if (flags != AMF_EMPTY) 
		/* Found first not-completed table entry */
		break;
  }

  /* Reset to the beginning of the table when all messages are completed */
  if (first_slot >= next_slot && !needack)
	next_slot = first_slot = 0;

  /* Can the table handle one more message? */
  if (next_slot >= ASYN_NR) {
	/* We're full; tell the kernel to stop processing for now */
	if ((r = ipc_senda(NULL, 0)) != OK)
		panic("asynsend: ipc_senda failed: %d", r);

	/* Move all unprocessed messages to the beginning */
	dst_ind = 0;
	for (src_ind = first_slot; src_ind < next_slot; src_ind++) {
		flags = msgtable[src_ind].flags;

		/* Skip empty entries */
		if (flags == AMF_EMPTY) continue;

		/* and completed entries only if result is OK or if error 
		 * doesn't need to be acknowledged */
		if ((flags & (AMF_VALID|AMF_DONE)) == (AMF_VALID|AMF_DONE)) {
			if (msgtable[src_ind].result == OK)
				continue;
			else {
#if DEBUG
				printf(
				 "asynsend: found entry %d with error %d\n",
					src_ind, msgtable[src_ind].result);
#endif
				if (!(flags & (AMF_NOTIFY|AMF_NOTIFY_ERR)))
					/* Don't need to ack this error */
					 continue;
			}
		}


		/* Copy/move in use entry */
#if DEBUG
		printf("asynsend: copying entry %d to %d\n", src_ind, dst_ind);
#endif
		if (src_ind != dst_ind) msgtable[dst_ind] = msgtable[src_ind];
			dst_ind++;
	}

	/* Mark unused entries empty */
	for (i = dst_ind; i < ASYN_NR; i++) msgtable[i].flags = AMF_EMPTY;

	first_slot = 0;
	next_slot = dst_ind;
	if (next_slot >= ASYN_NR)	/* Cleanup failed */
		panic("asynsend: msgtable full");
  }

  fl |= AMF_VALID;	/* Mark in use */
  msgtable[next_slot].dst = dst;
  msgtable[next_slot].msg = *mp;
  msgtable[next_slot].flags = fl;		/* Has to be last. The kernel 
					 	 * scans this table while we
						 * are sleeping.
					 	 */
  next_slot++;

  assert(next_slot >= first_slot);
  len = next_slot - first_slot;
  assert(first_slot + len <= ASYN_NR);
  assert(len >= 0);

  inside = 0;

  /* Tell the kernel to rescan the table */
  return ipc_senda(&msgtable[first_slot], len);
}

/*===========================================================================*
 *				asyn_geterror				     *
 *===========================================================================*/
int asyn_geterror(endpoint_t *dst, message *msg, int *err)
{
  int src_ind, flags, result;

  if (!initialized) return(0);

  for (src_ind = 0; src_ind < next_slot; src_ind++) {
	flags = msgtable[src_ind].flags;
	result = msgtable[src_ind].result;

	/* Find a message that has been completed with an error */
	if ((flags & (AMF_VALID|AMF_DONE)) == (AMF_VALID|AMF_DONE)) {
		if (result != OK && (flags & (AMF_NOTIFY|AMF_NOTIFY_ERR))) {
			/* Found one */
			if (dst != NULL) *dst = msgtable[src_ind].dst;
			if (msg != NULL) *msg = msgtable[src_ind].msg;
			if (err != NULL) *err = result;

			/* Acknowledge error so it can be cleaned up upon next
			 * asynsend */
			msgtable[src_ind].result = OK; 

			return(1);
		}
	}
  }

  return(0);
}

