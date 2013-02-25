#define _SYSTEM		1	/* tell headers that this is the kernel */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/ucred.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <minix/config.h>
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
#include <minix/vtreefs.h>

#include "glo.h"
#include "proto.h"
#include "inode.h"
