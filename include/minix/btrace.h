#ifndef _MINIX_BTRACE_H
#define _MINIX_BTRACE_H

/* Control directives. */
enum {
  BTCTL_START,
  BTCTL_STOP
};

/* Request codes. */
enum {
  BTREQ_OPEN,
  BTREQ_CLOSE,
  BTREQ_READ,
  BTREQ_WRITE,
  BTREQ_GATHER,
  BTREQ_SCATTER,
  BTREQ_IOCTL
};

/* Special result codes. */
#define BTRES_INPROGRESS	(-997)

/* Block trace entry. */
typedef struct {
  u32_t request;		/* request code; one of BTR_xxx */
  u32_t size;			/* request size, ioctl request, or access */
  u64_t position;		/* starting disk position */
  u32_t flags;			/* transfer flags */
  i32_t result;			/* request result; OK, bytes, or error */
  u32_t start_time;		/* request service start time (us) */
  u32_t finish_time;		/* request service completion time (us) */
} btrace_entry;			/* (32 bytes) */

/* This is the number of btrace_entry structures copied out at once using the
 * BIOCTRACEGET ioctl call.
 */
#define BTBUF_SIZE	1024

#endif /* _MINIX_BTRACE_H */
