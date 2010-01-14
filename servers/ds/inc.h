/* Header file including all needed system headers. */

#define _SYSTEM            1    /* get OK and negative error codes */
#define _MINIX             1	/* tell headers to include MINIX stuff */

#include <ansi.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include <regex.h>

#include <minix/callnr.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/ds.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/keymap.h>
#include <minix/bitmap.h>
#include <minix/rs.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "proto.h"
#include "glo.h"

