/* msync(2) for MINIX.
 *
 * MAP_SHARED file mappings share the VM page cache pages in place (see
 * minix/servers/vm/mmap.c), so every process mapping or read(2)ing the file
 * sees writes at once: there is no separate copy to synchronize.  Getting the
 * cached data onto the disk is up to the file system's normal flushing
 * (writeback of mapped pages at msync/munmap time is not implemented), so
 * msync only validates its arguments.
 */

#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

int
msync(void *addr, size_t len, int flags)
{
	(void)len;
	if ((uintptr_t)addr % (uintptr_t)getpagesize() != 0 ||
	    (flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0 ||
	    (flags & (MS_ASYNC | MS_SYNC)) == (MS_ASYNC | MS_SYNC)) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}
