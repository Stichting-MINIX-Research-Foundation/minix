#ifndef _PROCFS_INC_H
#define _PROCFS_INC_H

#define _POSIX_SOURCE      1
#define _MINIX             1
#define _SYSTEM            1

#include <minix/config.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <lib.h>
#include <timers.h>
#include <a.out.h>
#include <dirent.h>

#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/keymap.h>
#include <minix/bitmap.h>
#include <minix/vfsif.h>
#include <minix/endpoint.h>
#include <minix/sysinfo.h>
#include <minix/u64.h>
#include <minix/sysinfo.h>
#include <minix/type.h>
#include <minix/ipc.h>

#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/type.h"
#include "kernel/proc.h"
#include "pm/mproc.h"
#include "vfs/const.h"
#include "vfs/fproc.h"
#include "vfs/dmap.h"

#include <minix/vtreefs.h>
#include <minix/procfs.h>

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

#endif /* _PROCFS_INC_H */
