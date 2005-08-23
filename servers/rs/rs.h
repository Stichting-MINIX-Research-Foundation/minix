/* Header file for the system service manager server. 
 *
 * Created:
 *    Jul 22, 2005	by Jorrit N. Herder 
 */

#define _SYSTEM            1    /* get OK and negative error codes */
#define _MINIX             1	/* tell headers to include MINIX stuff */

#define VERBOSE		0	/* display diagnostics */

#include <ansi.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <minix/callnr.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/keymap.h>
#include <minix/bitmap.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "proto.h"

