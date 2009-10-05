#define _SYSTEM	1
#define _MINIX 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ds.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int minix_rs_lookup(const char *name, endpoint_t *value)
{
	int r;
	message m;
	size_t len_key;

	len_key = strlen(name)+1;

	m.RS_NAME = (char *) name;
	m.RS_NAME_LEN = len_key;

	r = _taskcall(RS_PROC_NR, RS_LOOKUP, &m);

	if(r == OK) {
		*value = m.RS_ENDPOINT;
	}

	return r;
}

