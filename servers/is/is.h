/* Header file for the system information server. 
 *
 * Created:
 *    Jul 13, 2004	by Jorrit N. Herder 
 */

#define _SYSTEM            1    /* get OK and negative error codes */
#define _MINIX             1	/* tell headers to include MINIX stuff */

#include <ansi.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>

#include <minix/callnr.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/utils.h>
#include <minix/keymap.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "proto.h"
#include "glo.h"

