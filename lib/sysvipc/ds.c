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

int mini_ds_retrieve_u32(char *ds_name, u32_t *value)
{
	int r;
	message m;
	size_t len_key;

	len_key = strlen(ds_name)+1;

	m.DS_KEY_GRANT = ds_name;
	m.DS_KEY_LEN = len_key;
	m.DS_FLAGS = DS_TYPE_U32;

	r = _taskcall(DS_PROC_NR, DS_RETRIEVE_LIBC, &m);

	if(r == OK) {
		/* Assign u32 value. */
		*value = m.DS_VAL;
	}

	return r;
}

