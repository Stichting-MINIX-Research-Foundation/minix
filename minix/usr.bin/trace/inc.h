#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <assert.h>

#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <machine/stackframe.h>

#include <netinet/in.h>

#include "proc.h"
#include "type.h"
#include "proto.h"
