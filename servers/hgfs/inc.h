
#define _POSIX_SOURCE 1			/* for signal handling */
#define _SYSTEM 1			/* for negative error values */
#define _MINIX 1

#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/safecopies.h>
#include <minix/vfsif.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>

#if DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/queue.h>

#include "libhgfs/hgfs.h"

#include "type.h"
#include "const.h"
#include "proto.h"
#include "glo.h"

#include "inode.h"
