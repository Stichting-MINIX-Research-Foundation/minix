
#include <minix/ds.h>
#include <string.h>

#include "syslib.h"

static int do_invoke_ds(message *m, int type, const char *ds_name)
{
	cp_grant_id_t g_key;
	size_t len_key;
	int access, r;

	if(type == DS_CHECK || type == DS_RETRIEVE_LABEL) {
		len_key = DS_MAX_KEYLEN;
		access = CPF_WRITE;
	} else {
		len_key = strlen(ds_name)+1;
		access = CPF_READ;
	}

	/* Grant for key. */
	g_key = cpf_grant_direct(DS_PROC_NR, (vir_bytes) ds_name,
		len_key, access);
	if(!GRANT_VALID(g_key)) 
		return ENOMEM;

	m->m_ds_req.key_grant = g_key;
	m->m_ds_req.key_len = len_key;

	r = _taskcall(DS_PROC_NR, type, m);

	cpf_revoke(g_key);
	return r;
}

int ds_publish_label(const char *ds_name, endpoint_t endpoint, int flags)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.val_in.ep = endpoint;
	m.m_ds_req.flags = DSF_TYPE_LABEL | flags;
	return do_invoke_ds(&m, DS_PUBLISH, ds_name);
}

int ds_publish_u32(const char *ds_name, u32_t value, int flags)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.val_in.u32 = value;
	m.m_ds_req.flags = DSF_TYPE_U32 | flags;
	return do_invoke_ds(&m, DS_PUBLISH, ds_name);
}

static int ds_publish_raw(const char *ds_name, void *vaddr, size_t length,
	int flags)
{
	cp_grant_id_t gid;
	message m;
	int r;

	/* Grant for memory range. */
	gid = cpf_grant_direct(DS_PROC_NR, (vir_bytes)vaddr, length, CPF_READ);
	if(!GRANT_VALID(gid))
		return ENOMEM;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.val_in.grant = gid;
	m.m_ds_req.val_len = length;
	m.m_ds_req.flags = flags;

	r = do_invoke_ds(&m, DS_PUBLISH, ds_name);
	cpf_revoke(gid);

	return r;
}

int ds_publish_str(const char *ds_name, char *value, int flags)
{
	size_t length;
	length = strlen(value) + 1;
	value[length - 1] = '\0';
	return ds_publish_raw(ds_name, value, length, flags | DSF_TYPE_STR);
}

int ds_publish_mem(const char *ds_name, void *vaddr, size_t length, int flags)
{
	return ds_publish_raw(ds_name, vaddr, length, flags | DSF_TYPE_MEM);
}

int ds_retrieve_label_name(char *ds_name, endpoint_t endpoint)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.val_in.ep = endpoint;
	r = do_invoke_ds(&m, DS_RETRIEVE_LABEL, ds_name);
	return r;
}

int ds_retrieve_label_endpt(const char *ds_name, endpoint_t *endpoint)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.flags = DSF_TYPE_LABEL;
	r = do_invoke_ds(&m, DS_RETRIEVE, ds_name);
	*endpoint = m.m_ds_reply.val_out.ep;
	return r;
}

int ds_retrieve_u32(const char *ds_name, u32_t *value)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.flags = DSF_TYPE_U32;
	r = do_invoke_ds(&m, DS_RETRIEVE, ds_name);
	*value = m.m_ds_reply.val_out.u32;
	return r;
}

static int ds_retrieve_raw(const char *ds_name, char *vaddr, size_t *length,
	int flags)
{
	message m;
	cp_grant_id_t gid;
	int r;

	/* Grant for memory range. */
	gid = cpf_grant_direct(DS_PROC_NR, (vir_bytes)vaddr, *length, CPF_WRITE);
	if(!GRANT_VALID(gid))
		return ENOMEM;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.val_in.grant = gid;
	m.m_ds_req.val_len = *length;
	m.m_ds_req.flags = flags;
	r = do_invoke_ds(&m, DS_RETRIEVE, ds_name);
	*length = m.m_ds_reply.val_len;
	cpf_revoke(gid);

	return r;
}

int ds_retrieve_str(const char *ds_name, char *value, size_t len_str)
{
	int r;
	size_t length = len_str + 1;
	r = ds_retrieve_raw(ds_name, value, &length, DSF_TYPE_STR);
	value[length - 1] = '\0';
	return r;
}

int ds_retrieve_mem(const char *ds_name, char *vaddr, size_t *length)
{
	return ds_retrieve_raw(ds_name, vaddr, length, DSF_TYPE_MEM);
}

int ds_delete_u32(const char *ds_name)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.flags = DSF_TYPE_U32;
	return do_invoke_ds(&m, DS_DELETE, ds_name);
}

int ds_delete_str(const char *ds_name)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.flags = DSF_TYPE_STR;
	return do_invoke_ds(&m, DS_DELETE, ds_name);
}

int ds_delete_mem(const char *ds_name)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.flags = DSF_TYPE_MEM;
	return do_invoke_ds(&m, DS_DELETE, ds_name);
}

int ds_delete_label(const char *ds_name)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.flags = DSF_TYPE_LABEL;
	return do_invoke_ds(&m, DS_DELETE, ds_name);
}

int ds_subscribe(const char *regexp, int flags)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_ds_req.flags = flags;
	return do_invoke_ds(&m, DS_SUBSCRIBE, regexp);
}

int ds_check(char *ds_key, int *type, endpoint_t *owner_e)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	r = do_invoke_ds(&m, DS_CHECK, ds_key);
	if(type) *type = m.m_ds_req.flags;
	if(owner_e) *owner_e = m.m_ds_req.owner;
	return r;
}
