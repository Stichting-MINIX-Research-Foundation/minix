
#include <minix/ds.h>
#include <string.h>

#include "syslib.h"

#define GRANTBAD -1001

int
ds_subscribe(ds_name_regexp, type, flags)
char *ds_name_regexp;
int type;
int flags;
{
	int r;
	message m;
	cp_grant_id_t g;
	size_t len;

	len = strlen(ds_name_regexp)+1;
	g = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) ds_name_regexp, len, CPF_READ);

	if(!GRANT_VALID(g)) 
		return GRANTBAD;

	flags &= DS_INITIAL;

	m.DS_KEY_GRANT = (char *) g;
	m.DS_KEY_LEN = len;
	m.DS_FLAGS = flags | (type & DS_TYPE_MASK);

	r = _taskcall(DS_PROC_NR, DS_SUBSCRIBE, &m);

	cpf_revoke(g);

	return r;
}

int ds_publish_u32(ds_name, value)
char *ds_name;
u32_t value;
{
	int r;
	message m;
	cp_grant_id_t g;
	size_t len;

	len = strlen(ds_name)+1;
	g = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) ds_name, len, CPF_READ);

	if(!GRANT_VALID(g)) 
		return GRANTBAD;

	m.DS_KEY_GRANT = (char *) g;
	m.DS_KEY_LEN = len;
	m.DS_FLAGS = DS_TYPE_U32;
	m.DS_VAL = value;
	m.DS_VAL_LEN = sizeof(value);

	r = _taskcall(DS_PROC_NR, DS_PUBLISH, &m);

	cpf_revoke(g);

	return r;
}

int ds_publish_str(ds_name, value)
char *ds_name;
char *value;
{
	int r;
	message m;
	cp_grant_id_t g_key, g_str;
	size_t len_key, len_str;

	/* Grant for key. */
	len_key = strlen(ds_name)+1;
	g_key = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) ds_name, len_key, CPF_READ);
	if(!GRANT_VALID(g_key)) 
		return GRANTBAD;

	/* Grant for value. */
	len_str = strlen(value)+1;
	g_str = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) value, len_str, CPF_READ);

	if(!GRANT_VALID(g_str))  {
		cpf_revoke(g_key);
		return GRANTBAD;
	}

	m.DS_KEY_GRANT = (char *) g_key;
	m.DS_KEY_LEN = len_key;
	m.DS_FLAGS = DS_TYPE_STR;
	m.DS_VAL = g_str;
	m.DS_VAL_LEN = len_str;

	r = _taskcall(DS_PROC_NR, DS_PUBLISH, &m);

	cpf_revoke(g_key);
	cpf_revoke(g_str);

	return r;
}

int ds_retrieve_u32(ds_name, value)
char *ds_name;
u32_t *value;
{
	int r;
	message m;
	cp_grant_id_t g_key;
	size_t len_key;

	/* Grant for key. */
	len_key = strlen(ds_name)+1;
	g_key = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) ds_name, len_key, CPF_READ);
	if(!GRANT_VALID(g_key)) 
		return GRANTBAD;

	/* Do request. */
	m.DS_KEY_GRANT = (char *) g_key;
	m.DS_KEY_LEN = len_key;
	m.DS_FLAGS = DS_TYPE_U32;

	r = _taskcall(DS_PROC_NR, DS_RETRIEVE, &m);

	cpf_revoke(g_key);

	/* Assign u32 value. */
	*value = m.DS_VAL;

	return r;
}

int ds_retrieve_str(ds_name, value, len_str)
char *ds_name;
char *value;
size_t len_str;
{
	int r;
	message m;
	cp_grant_id_t g_key, g_str;
	size_t len_key;

	/* Grant for key. */
	len_key = strlen(ds_name)+1;
	g_key = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) ds_name, len_key, CPF_READ);
	if(!GRANT_VALID(g_key)) 
		return GRANTBAD;

	/* Grant for value. */
	g_str = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) value, len_str, CPF_WRITE);

	if(!GRANT_VALID(g_str))  {
		cpf_revoke(g_key);
		return GRANTBAD;
	}

	/* Do request. */

	m.DS_KEY_GRANT = (char *) g_key;
	m.DS_KEY_LEN = len_key;
	m.DS_FLAGS = DS_TYPE_STR;
	m.DS_VAL = g_str;
	m.DS_VAL_LEN = len_str;

	r = _taskcall(DS_PROC_NR, DS_RETRIEVE, &m);

	cpf_revoke(g_key);
	cpf_revoke(g_str);

	return r;
}

int ds_check_str(ds_key, len_key, value, len_str)
char *ds_key;
size_t len_key;
char *value;
size_t len_str;
{
	int r;
	message m;
	cp_grant_id_t g_key, g_str;

	if(len_key < 1 || len_str < 1) return -1002;

	/* Grant for key. */
	g_key = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) ds_key, len_key, CPF_WRITE);
	if(!GRANT_VALID(g_key)) 
		return GRANTBAD;

	/* Grant for value. */
	g_str = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) value, len_str, CPF_WRITE);

	if(!GRANT_VALID(g_str))  {
		cpf_revoke(g_key);
		return GRANTBAD;
	}

	/* Do request. */

	m.DS_KEY_GRANT = (char *) g_key;
	m.DS_KEY_LEN = len_key;
	m.DS_FLAGS = DS_TYPE_STR;
	m.DS_VAL = g_str;
	m.DS_VAL_LEN = len_str;

	r = _taskcall(DS_PROC_NR, DS_CHECK, &m);

	cpf_revoke(g_key);
	cpf_revoke(g_str);

	ds_key[len_key-1] = '\0';
	value[len_str-1] = '\0';

	return r;
}

int ds_check_u32(ds_key, len_key, value)
char *ds_key;
size_t len_key;
u32_t *value;
{
	int r;
	message m;
	cp_grant_id_t g_key;

	if(len_key < 1) return -1;

	/* Grant for key. */
	g_key = cpf_grant_direct(DS_PROC_NR,
		(vir_bytes) ds_key, len_key, CPF_WRITE);
	if(!GRANT_VALID(g_key)) 
		return GRANTBAD;

	/* Do request. */
	m.DS_KEY_GRANT = (char *) g_key;
	m.DS_KEY_LEN = len_key;
	m.DS_FLAGS = DS_TYPE_U32;

	r = _taskcall(DS_PROC_NR, DS_CHECK, &m);

	cpf_revoke(g_key);

	ds_key[len_key-1] = '\0';

	/* Assign u32 value. */
	*value = m.DS_VAL;

	return r;
}

