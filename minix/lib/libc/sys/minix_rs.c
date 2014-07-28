#define _SYSTEM	1
#define _MINIX_SYSTEM	1

#include <sys/cdefs.h>

#include <minix/com.h>
#include <minix/config.h>
#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ds.h>
#include <minix/rs.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <lib.h>

int minix_rs_lookup(const char *name, endpoint_t *value)
{
	message m;
	size_t len_key;

	len_key = strlen(name)+1;

	memset(&m, 0, sizeof(m));
	m.m_rs_req.name = name;
	m.m_rs_req.name_len = len_key;

	if (_syscall(RS_PROC_NR, RS_LOOKUP, &m) != -1) {
		*value = m.m_rs_req.endpoint;
		return OK;
	}

	return -1;
}

