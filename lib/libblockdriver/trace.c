/* This file implements block level tracing support. */

#include <minix/drivers.h>
#include <minix/blockdriver_mt.h>
#include <minix/btrace.h>
#include <sys/ioc_block.h>
#include <minix/minlib.h>
#include <assert.h>

#include "const.h"
#include "trace.h"

#define NO_TRACEDEV		((dev_t) -1)
#define NO_TIME			((u32_t) -1)

static int trace_enabled	= FALSE;
static dev_t trace_dev		= NO_TRACEDEV;
static btrace_entry *trace_buf	= NULL;
static size_t trace_size	= 0;
static size_t trace_pos;
static size_t trace_next;
static u64_t trace_tsc;

/* Pointers to in-progress trace entries for each thread (all worker threads,
 * plus one for the main thread). Each pointer is set to NULL whenever no
 * operation is currently being traced for that thread, for whatever reason.
 */
static btrace_entry *trace_ptr[MAX_THREADS + 1] = { NULL };

/*===========================================================================*
 *				trace_gettime				     *
 *===========================================================================*/
static u32_t trace_gettime(void)
{
/* Return the current time, in microseconds since the start of the trace.
 */
  u64_t tsc;

  assert(trace_enabled);

  read_tsc_64(&tsc);

  tsc -= trace_tsc;

  return tsc_64_to_micros(tsc);
}

/*===========================================================================*
 *				trace_ctl				     *
 *===========================================================================*/
int trace_ctl(dev_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant)
{
/* Process a block trace control request.
 */
  size_t size;
  int r, ctl, entries;

  switch (request) {
  case BIOCTRACEBUF:
	/* The size cannot be changed when tracing is enabled. */
	if (trace_enabled) return EBUSY;

	/* Copy in the requested size. */
	if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &size,
		sizeof(size))) != OK)
		return r;

	if (size >= INT_MAX / sizeof(btrace_entry)) return EINVAL;

	/* The size can only be set or reset, not changed. */
	if (size > 0 && trace_size > 0) return EBUSY;

	/* Allocate or free a buffer for tracing data. For future multi-device
	 * tracing support, the buffer is associated with a minor device.
	 */
	if (size == 0) {
		if (trace_dev == NO_TRACEDEV) return OK;

		if (trace_dev != minor) return EINVAL;

		free(trace_buf);

		trace_dev = NO_TRACEDEV;
	} else {
		if ((trace_buf = malloc(size * sizeof(btrace_entry))) == NULL)
			return errno;

		trace_dev = minor;
	}

	trace_size = size;
	trace_pos = 0;
	trace_next = 0;

	return OK;

  case BIOCTRACECTL:
	/* We can only start/stop tracing if the given device has a trace
	 * buffer associated with it.
	 */
	if (trace_dev != minor) return EINVAL;

	/* Copy in the request code. */
	if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &ctl,
		sizeof(ctl))) != OK)
		return r;

	/* Start or stop tracing. */
	switch (ctl) {
	case BTCTL_START:
		if (trace_enabled) return EBUSY;

		read_tsc_64(&trace_tsc);

		trace_enabled = TRUE;

		break;

	case BTCTL_STOP:
		if (!trace_enabled) return EINVAL;

		trace_enabled = FALSE;

		/* Cancel all ongoing trace operations. */
		memset(trace_ptr, 0, sizeof(trace_ptr));

		break;

	default:
		return EINVAL;
	}

	return OK;

  case BIOCTRACEGET:
	/* We can only retrieve tracing entries if the given device has a trace
	 * buffer associated with it.
	 */
	if (trace_dev != minor) return EINVAL;

	if (trace_enabled) return EBUSY;

	/* How much can we copy out? */
	entries = MIN(trace_pos - trace_next,
		_MINIX_IOCTL_SIZE_BIG(request) / sizeof(btrace_entry));

	if (entries == 0)
		return 0;

	if ((r = sys_safecopyto(endpt, grant, 0,
		(vir_bytes) &trace_buf[trace_next],
		entries * sizeof(btrace_entry))) != OK)
		return r;

	trace_next += entries;

	return entries;
  }

  return EINVAL;
}

/*===========================================================================*
 *				trace_start				     *
 *===========================================================================*/
void trace_start(thread_id_t id, message *m_ptr)
{
/* Start creating a trace entry.
 */
  btrace_entry *entry;
  int req;
  u64_t pos;
  size_t size;
  int flags;

  if (!trace_enabled || trace_dev != m_ptr->m_lbdev_lblockdriver_msg.minor) return;

  assert(id >= 0 && id < MAX_THREADS + 1);

  if (trace_pos == trace_size)
	return;

  switch (m_ptr->m_type) {
  case BDEV_OPEN:	req = BTREQ_OPEN;	break;
  case BDEV_CLOSE:	req = BTREQ_CLOSE;	break;
  case BDEV_READ:	req = BTREQ_READ;	break;
  case BDEV_WRITE:	req = BTREQ_WRITE;	break;
  case BDEV_GATHER:	req = BTREQ_GATHER;	break;
  case BDEV_SCATTER:	req = BTREQ_SCATTER;	break;
  case BDEV_IOCTL:	req = BTREQ_IOCTL;	break;
  default:		return;
  }

  switch (m_ptr->m_type) {
  case BDEV_OPEN:
  case BDEV_CLOSE:
	pos = 0;
	size = m_ptr->m_lbdev_lblockdriver_msg.access;
	flags = 0;

	break;

  case BDEV_READ:
  case BDEV_WRITE:
  case BDEV_GATHER:
  case BDEV_SCATTER:
	pos = m_ptr->m_lbdev_lblockdriver_msg.pos;
	size = m_ptr->m_lbdev_lblockdriver_msg.count;
	flags = m_ptr->m_lbdev_lblockdriver_msg.flags;

	break;

  case BDEV_IOCTL:
	pos = 0;
	size = m_ptr->m_lbdev_lblockdriver_msg.request;
	flags = 0;

	/* Do not log trace control requests. */
	switch (size) {
	case BIOCTRACEBUF:
	case BIOCTRACECTL:
	case BIOCTRACEGET:
		return;
	}

	break;

  default:
	/* Do not log any other messages. */
	return;
  }

  entry = &trace_buf[trace_pos];
  entry->request = req;
  entry->size = size;
  entry->position = pos;
  entry->flags = flags;
  entry->result = BTRES_INPROGRESS;
  entry->start_time = trace_gettime();
  entry->finish_time = NO_TIME;

  trace_ptr[id] = entry;
  trace_pos++;
}

/*===========================================================================*
 *				trace_setsize				     *
 *===========================================================================*/
void trace_setsize(thread_id_t id, size_t size)
{
/* Set the current trace entry's actual (byte) size, for vector requests.
 */
  btrace_entry *entry;

  if (!trace_enabled) return;

  assert(id >= 0 && id < MAX_THREADS + 1);

  if ((entry = trace_ptr[id]) == NULL) return;

  entry->size = size;
}

/*===========================================================================*
 *				trace_finish				     *
 *===========================================================================*/
void trace_finish(thread_id_t id, int result)
{
/* Finish a trace entry.
 */
  btrace_entry *entry;

  if (!trace_enabled) return;

  assert(id >= 0 && id < MAX_THREADS + 1);

  if ((entry = trace_ptr[id]) == NULL) return;

  entry->result = result;
  entry->finish_time = trace_gettime();

  trace_ptr[id] = NULL;
}
