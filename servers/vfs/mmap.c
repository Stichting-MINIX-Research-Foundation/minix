/* mmap implementation in VFS
 *
 * The entry points into this file are
 *   do_vm_mmap:	VM calls VM_VFS_MMAP
 */

#include "fs.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/u64.h>
#include "file.h"
#include "fproc.h"
#include "lock.h"
#include "param.h"
#include <dirent.h>
#include <assert.h>
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

/*===========================================================================*
 *				do_vm_mmap				     *
 *===========================================================================*/
PUBLIC int do_vm_mmap()
{
}

