/* Header file for the system information server. 
 *
 * Created:
 *    Jul 13, 2004	by Jorrit N. Herder 
 */

#define _SYSTEM		1	/* get OK and negative error codes */

#include <sys/types.h>
#include <limits.h>
#include <errno.h>

#include <minix/callnr.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/sysinfo.h>
#include <minix/sysutil.h>
#include <minix/keymap.h>
#include <minix/bitmap.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <machine/archtypes.h>
#include "proto.h"
#include "glo.h"

