#ifndef __VFS_COMM_H__
#define __VFS_COMM_H__

/* VFS<->FS communication */

typedef struct {
  int c_max_reqs;	/* Max requests an FS can handle simultaneously */
  int c_cur_reqs;	/* Number of requests the FS is currently handling */
  struct worker_thread *c_req_queue;/* Queue of procs waiting to send a message */
} comm_t;

#endif
