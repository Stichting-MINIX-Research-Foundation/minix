
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>	
#include <minix/config.h>
#include <minix/type.h>
#include <minix/callnr.h>
#include <minix/safecopies.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/const.h>
#include <sys/ptrace.h>
#include <sys/svrctl.h>
#include <dirent.h>
#include <timers.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../kernel/arch/i386/include/archtypes.h"
#include "../../kernel/const.h"
#include "../../kernel/type.h"
#include "../../kernel/config.h"
#include "../../kernel/debug.h"
#include "../../kernel/proc.h"
#include "../../kernel/ipc.h"

#define SLOTS (NR_TASKS + NR_PROCS)
struct proc proc[SLOTS];


int write_seg(int fd, off_t off, endpoint_t proc_e, int seg,
	off_t seg_off, phys_bytes seg_bytes)
{
  int r, block_size, fl;
  off_t n, o, b_off;
  block_t b;
  struct buf *bp;
  ssize_t w;
  static char buf[1024];

  for (o= seg_off; o < seg_off+seg_bytes; o += sizeof(buf))
  {
	/* Copy a chunk from user space to the block buffer. */
	if(sys_vircopy(proc_e, seg, (phys_bytes) o,
	SELF, D, (vir_bytes) buf, (phys_bytes) sizeof(buf)) != OK) {
		printf("write_seg: sys_vircopy failed\n");
		return 1;
	}

	if((w=write(fd, buf, sizeof(buf))) != sizeof(buf)) {
		if(w < 0) printf("write error: %s\n", strerror(errno));
		printf("write_seg: write failed: %d/%d\n", w, sizeof(buf));
		return 1;
	}
  }

  return OK;
}


int dumpcore(endpoint_t proc_e)
{
	int r, seg, exists, fd;
	mode_t omode;
	vir_bytes len;
	off_t off, seg_off;
	long trace_off, trace_data;
	struct mem_map segs[NR_LOCAL_SEGS];
	struct proc procentry;
	int proc_s;
	ssize_t w;
	char core_name[200];

	if(sys_getproctab(proc) != OK) {
		printf( "Couldn't get proc tab.\n");
		return 1;
	}

	for(proc_s = 0; proc_s < SLOTS; proc_s++)
		if(proc[proc_s].p_endpoint == proc_e &&
			!(proc[proc_s].p_rts_flags & SLOT_FREE))
			break;

	if(proc_s >= SLOTS) {
		printf( "endpoint %d not found.\n", proc_e);
		return 1;
	}

	if(proc_s < 0 || proc_s >= SLOTS) {
		printf( "Slot out of range (internal error).\n");
		return 1;
	}

	if(proc[proc_s].p_rts_flags & SLOT_FREE) {
		printf( "slot %d is no process (internal error).\n",
			proc_s);
		return 1;
	}

	sprintf(core_name, "/tmp/core.%d", proc_e);

	if((fd = open(core_name,
		O_CREAT|O_WRONLY|O_EXCL|O_NONBLOCK, 0600)) < 0) {
		printf("couldn't open %s (%s)\n",
			core_name, strerror(errno));
		return 1;
	}

	proc[proc_s].p_name[P_NAME_LEN-1] = '\0';

	memcpy(segs, proc[proc_s].p_memmap, sizeof(segs));

	off= 0;
	if((w=write(fd, segs, sizeof(segs))) != sizeof(segs)) {
		if(w < 0) printf("write error: %s\n", strerror(errno));
		printf( "segs write failed: %d/%d\n", w, sizeof(segs));
		return 1;
	}
	off += sizeof(segs);

	/* Write out the whole kernel process table entry to get the regs. */
	for (trace_off= 0;; trace_off += sizeof(long))
	{
		r= sys_trace(T_GETUSER, proc_e, trace_off, &trace_data);
		if  (r != OK) 
		{
			break;
		}
		r= write(fd, &trace_data, sizeof(trace_data));
		if (r != sizeof(trace_data)) {
			printf( "trace_data write failed\n");
			return 1;
		}
		off += sizeof(trace_data);
	}

	/* Loop through segments and write the segments themselves out. */
	for (seg = 0; seg < NR_LOCAL_SEGS; seg++) {
		len= segs[seg].mem_len << CLICK_SHIFT;
		seg_off= segs[seg].mem_vir << CLICK_SHIFT;
		r= write_seg(fd, off, proc_e, seg, seg_off, len);
		if (r != OK)
		{
			printf( "write failed\n");
			return 1;
		}
		off += len;
	}

	close(fd);

	return 0;
}

main(int argc, char *argv[])
{
	if(argc != 2) {
		printf("usage: %s <endpoint>\n", argv[0]);
		return 1;
	}
	dumpcore(atoi(argv[1]));
	return 1;
}

