
#define _SYSTEM         1       /* get OK and negative error codes */

#define VERBOSE         0       /* display diagnostics */

#include <sys/types.h>
#include <lib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <stdio.h>
#include <minix/callnr.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/log.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/bitmap.h>

#include <minix/fsdriver.h>
#include <minix/libminixfs.h>
#include <minix/bdev.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/param.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/dirent.h>

#include <assert.h>

#define b_data(bp) ((char *) (bp->data))

#include "const.h"
#include "proto.h"
#include "super.h"
#include "glo.h"

