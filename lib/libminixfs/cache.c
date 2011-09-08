
#define _SYSTEM

#include <minix/libminixfs.h>
#include <minix/dmap.h>
#include <minix/u64.h>
#include <sys/param.h>
#include <errno.h>

u32_t fs_bufs_heuristic(int minbufs, u32_t btotal, u32_t bfree, 
         int blocksize, dev_t majordev)
{
  struct vm_stats_info vsi;
  int bufs;
  u32_t kbytes_used_fs, kbytes_total_fs, kbcache, kb_fsmax;
  u32_t kbytes_remain_mem, bused;

  bused = btotal-bfree;

  /* but we simply need minbufs no matter what, and we don't
   * want more than that if we're a memory device
   */
  if(majordev == MEMORY_MAJOR) {
	return minbufs;
  }

  /* set a reasonable cache size; cache at most a certain
   * portion of the used FS, and at most a certain %age of remaining
   * memory
   */
  if((vm_info_stats(&vsi) != OK)) {
	bufs = 1024;
	printf("fslib: heuristic info fail: default to %d bufs\n", bufs);
	return bufs;
  }

  kbytes_remain_mem = div64u(mul64u(vsi.vsi_free, vsi.vsi_pagesize), 1024);

  /* check fs usage. */
  kbytes_used_fs = div64u(mul64u(bused, blocksize), 1024);
  kbytes_total_fs = div64u(mul64u(btotal, blocksize), 1024);

  /* heuristic for a desired cache size based on FS usage;
   * but never bigger than half of the total filesystem
   */
  kb_fsmax = sqrt_approx(kbytes_used_fs)*40;
  kb_fsmax = MIN(kb_fsmax, kbytes_total_fs/2);

  /* heuristic for a maximum usage - 10% of remaining memory */
  kbcache = MIN(kbytes_remain_mem/10, kb_fsmax);
  bufs = kbcache * 1024 / blocksize;

  /* but we simply need MINBUFS no matter what */
  if(bufs < minbufs)
	bufs = minbufs;

  return bufs;
}

