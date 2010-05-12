/* dumpcore - create core file of running process */

#include <fcntl.h>
#include <unistd.h>	
#include <minix/config.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/const.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include <timers.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <machine/archtypes.h>
#include "kernel/proc.h"

#define CLICK_WORDS (CLICK_SIZE / sizeof(unsigned long))

int adjust_stack(pid_t pid, struct mem_map *seg)
{
  static unsigned long buf[CLICK_WORDS];
  struct ptrace_range pr;
  size_t off, top, bottom;
  int i;

  /* FIXME: kernel/VM strangeness */
  seg->mem_vir -= seg->mem_len - 1;

  /* Scan the stack, top to bottom, to find the lowest accessible region.
   * In practice that will be at 64MB, so we also scan for the lowest non-zero
   * region in order to keep the core file size managable.
   * Portability note: this code assumes that the stack grows down.
   */
  top = seg->mem_vir + seg->mem_len;

  pr.pr_space = TS_DATA;
  pr.pr_addr = (top - 1) << CLICK_SHIFT;
  pr.pr_size = sizeof(buf);
  pr.pr_ptr = buf;

  for (off = top - 1; off >= seg->mem_vir; off--) {
	if (ptrace(T_GETRANGE, pid, (long) &pr, 0)) {
		if (errno == EFAULT)
			break;

		perror("ptrace(T_GETRANGE)");
		return 1;
	}

	for (i = 0; i < CLICK_WORDS; i += sizeof(buf[0]))
		if (buf[i] != 0)
			bottom = off;

	pr.pr_addr -= sizeof(buf);
  }

  /* Add one extra zero page as margin. */
  if (bottom > off && bottom > seg->mem_vir)
	bottom--;

  seg->mem_len -= bottom - seg->mem_vir;
  seg->mem_vir = bottom;

  return 0;
}

int write_seg(int fd, pid_t pid, int seg, off_t seg_off, phys_bytes seg_bytes)
{
  ssize_t w;
  static char buf[CLICK_SIZE];
  struct ptrace_range pr;

  pr.pr_space = (seg == T) ? TS_INS : TS_DATA;
  pr.pr_addr = seg_off;
  pr.pr_size = sizeof(buf);
  pr.pr_ptr = buf;

  for ( ; pr.pr_addr < seg_off + seg_bytes; pr.pr_addr += sizeof(buf))
  {
	/* Copy a chunk from user space to the block buffer. */
	if (ptrace(T_GETRANGE, pid, (long) &pr, 0)) {
		/* Create holes for inaccessible areas. */
		if (errno == EFAULT) {
			lseek(fd, sizeof(buf), SEEK_CUR);
			continue;
		}

		perror("ptrace(T_GETRANGE)");
		return 1;
	}

	if((w=write(fd, buf, sizeof(buf))) != sizeof(buf)) {
		if(w < 0) printf("write error: %s\n", strerror(errno));
		printf("write_seg: write failed: %d/%d\n", w, sizeof(buf));
		return 1;
	}
  }

  return 0;
}

int dumpcore(pid_t pid)
{
	int r, seg, fd;
	vir_bytes len;
	off_t off, seg_off;
	long data;
	struct mem_map segs[NR_LOCAL_SEGS];
	struct proc procentry;
	ssize_t w;
	char core_name[PATH_MAX];

	/* Get the process table entry for this process. */
	len = sizeof(struct proc) / sizeof(long);
	for (off = 0; off < len; off++)
	{
		errno = 0;
		data = ptrace(T_GETUSER, pid, off * sizeof(long), 0);
		if  (data == -1 && errno != 0) 
		{
			perror("ptrace(T_GETUSER)");
			return 1;
		}

		((long *) &procentry)[off] = data;
	}

	memcpy(segs, procentry.p_memmap, sizeof(segs));

	/* Correct and reduce the stack segment. */
	r = adjust_stack(pid, &segs[S]);
	if (r != 0)
		goto error;

	/* Create a core file with a temporary, unique name. */
	sprintf(core_name, "core.%d", pid);

	if((fd = open(core_name, O_CREAT|O_EXCL|O_WRONLY, 0600)) < 0) {
		fprintf(stderr, "couldn't open %s (%s)\n", core_name,
			strerror(errno));
		return 1;
	}

	/* Write out the process's segments. */
	if((w=write(fd, segs, sizeof(segs))) != sizeof(segs)) {
		if(w < 0) printf("write error: %s\n", strerror(errno));
		printf( "segs write failed: %d/%d\n", w, sizeof(segs));
		goto error;
	}

	/* Write out the whole kernel process table entry to get the regs. */
	if((w=write(fd, &procentry, sizeof(procentry))) != sizeof(procentry)) {
		if(w < 0) printf("write error: %s\n", strerror(errno));
		printf( "proc write failed: %d/%d\n", w, sizeof(procentry));
		goto error;
	}

	/* Loop through segments and write the segments themselves out. */
	for (seg = 0; seg < NR_LOCAL_SEGS; seg++) {
		len= segs[seg].mem_len << CLICK_SHIFT;
		seg_off= segs[seg].mem_vir << CLICK_SHIFT;
		r= write_seg(fd, pid, seg, seg_off, len);
		if (r != 0)
			goto error;
	}

	/* Give the core file its final name. */ 
	if (rename(core_name, "core")) {
		perror("rename");
		goto error;
	}

	close(fd);

	return 0;

error:
	close(fd);

	unlink(core_name);

	return 1;
}

int main(int argc, char *argv[])
{
	pid_t pid;
	int r, status;

	if(argc != 2) {
		printf("usage: %s <pid>\n", argv[0]);
		return 1;
	}

	pid = atoi(argv[1]);

	if (ptrace(T_ATTACH, pid, 0, 0) != 0) {
		perror("ptrace(T_ATTACH)");
		return 1;
	}

	if (waitpid(pid, &status, 0) != pid) {
		perror("waitpid");
		return 1;
	}

	while (WIFSTOPPED(status) && WSTOPSIG(status) != SIGSTOP) {
		/* whatever happens here is fine */
		ptrace(T_RESUME, pid, 0, WSTOPSIG(status));

		if (waitpid(pid, &status, 0) != pid) {
			perror("waitpid");
			return 1;
		}
	}

	if (!WIFSTOPPED(status)) {
		fprintf(stderr, "process died while attaching\n");
		return 1;
	}

	r = dumpcore(pid);

	if (ptrace(T_DETACH, pid, 0, 0)) {
		fprintf(stderr, "warning, detaching failed (%s)\n",
			strerror(errno));
	}

	return r;
}
