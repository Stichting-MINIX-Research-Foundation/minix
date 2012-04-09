/* VBOX driver interface - synchronous version - by D.C. van Moolenbroek */

#include <minix/drivers.h>
#include <minix/vboxtype.h>
#include <minix/vboxif.h>
#include <minix/vbox.h>
#include <minix/ds.h>
#include <assert.h>

static endpoint_t vbox_endpt = NONE;

int vbox_init(void)
{
/* Initialize the library, by resolving the VBOX driver endpoint.
 */
  int r;

  if ((r = ds_retrieve_label_endpt("vbox", &vbox_endpt)) != OK) {
	printf("libvbox: unable to obtain VBOX endpoint (%d)\n", r);

	return EINVAL;
  }

  return OK;
}

vbox_conn_t vbox_open(char *name)
{
/* Open a VirtualBox HGCM connection.
 */
  message m;
  cp_grant_id_t grant;
  size_t len;
  int r;

  if (vbox_endpt == NONE)
	return EDEADSRCDST;

  len = strlen(name) + 1;
  grant = cpf_grant_direct(vbox_endpt, (vir_bytes) name, len, CPF_READ);
  if (!GRANT_VALID(grant))
	return ENOMEM;

  memset(&m, 0, sizeof(m));
  m.m_type = VBOX_OPEN;
  m.VBOX_GRANT = grant;
  m.VBOX_COUNT = len;
  m.VBOX_ID = 0;

  r = sendrec(vbox_endpt, &m);

  cpf_revoke(grant);

  if (r != OK)
	return r;

  if (m.VBOX_ID != 0)
	return EINVAL;

  return m.VBOX_RESULT;
}

int vbox_close(vbox_conn_t conn)
{
/* Close a VirtualBox HGCM connection.
 */
  message m;
  int r;

  if (vbox_endpt == NONE)
	return EDEADSRCDST;

  memset(&m, 0, sizeof(m));
  m.m_type = VBOX_CLOSE;
  m.VBOX_CONN = conn;
  m.VBOX_ID = 0;

  r = sendrec(vbox_endpt, &m);

  if (r != OK)
	return r;

  if (m.VBOX_ID != 0)
	return EINVAL;

  return m.VBOX_RESULT;
}

int vbox_call(vbox_conn_t conn, u32_t function, vbox_param_t *param, int count,
  int *code)
{
/* Call a VirtualBox HGCM function. The caller must set up all buffer grants.
 */
  cp_grant_id_t grant = GRANT_INVALID;
  message m;
  int i, r;

  if (vbox_endpt == NONE) {
	vbox_put(param, count);

	return EDEADSRCDST;
  }

  /* Check whether all parameters are initialized correctly. */
  for (i = 0; i < count; i++) {
	switch (param[i].type) {
	case VBOX_TYPE_U32:
	case VBOX_TYPE_U64:
	case VBOX_TYPE_PTR:
		break;

	default:
		vbox_put(param, count);

		return ENOMEM;
	}
  }

  if (count > 0) {
	grant = cpf_grant_direct(vbox_endpt, (vir_bytes) param,
		sizeof(param[0]) * count, CPF_READ | CPF_WRITE);
	if (!GRANT_VALID(grant)) {
		vbox_put(param, count);

		return ENOMEM;
	}
  }

  memset(&m, 0, sizeof(m));
  m.m_type = VBOX_CALL;
  m.VBOX_CONN = conn;
  m.VBOX_GRANT = grant;
  m.VBOX_COUNT = count;
  m.VBOX_ID = 0;
  m.VBOX_FUNCTION = function;

  r = sendrec(vbox_endpt, &m);

  if (GRANT_VALID(grant))
	cpf_revoke(grant);

  vbox_put(param, count);

  if (r != OK)
	return r;

  if (m.VBOX_ID != 0)
	return EINVAL;

  if (code != NULL)
	*code = m.VBOX_CODE;

  return m.VBOX_RESULT;
}

void vbox_set_u32(vbox_param_t *param, u32_t value)
{
/* Set the given parameter to a 32-bit value.
 */

  param->type = VBOX_TYPE_U32;
  param->u32 = value;
}

void vbox_set_u64(vbox_param_t *param, u64_t value)
{
/* Set the given parameter to a 32-bit value.
 */

  param->type = VBOX_TYPE_U64;
  param->u64 = value;
}

void vbox_set_ptr(vbox_param_t *param, void *ptr, size_t size,
  unsigned int dir)
{
/* Set the given parameter to a grant for the given local pointer.
 */
  cp_grant_id_t grant = GRANT_INVALID;
  int flags;

  flags = 0;
  if (dir & VBOX_DIR_IN) flags |= CPF_WRITE;
  if (dir & VBOX_DIR_OUT) flags |= CPF_READ;

  if (size > 0) {
	grant = cpf_grant_direct(vbox_endpt, (vir_bytes) ptr, size, flags);
	if (!GRANT_VALID(grant)) {
		param->type = VBOX_TYPE_INVALID;

		return;
	}
  }

  param->type = VBOX_TYPE_PTR;
  param->ptr.grant = grant;
  param->ptr.off = 0;
  param->ptr.size = size;
  param->ptr.dir = dir;
}

void vbox_set_grant(vbox_param_t *param, endpoint_t endpt, cp_grant_id_t grant,
  size_t off, size_t size, unsigned int dir)
{
/* Set the given parameter to an indirect grant for the given grant.
 */
  cp_grant_id_t indir_grant;

  /* Unfortunately, the current implementation of indirect grants does not
   * support making smaller subgrants out of larger original grants. Therefore,
   * we are forced to grant more access than we would like.
   */
  indir_grant = cpf_grant_indirect(vbox_endpt, endpt, grant);
  if (!GRANT_VALID(indir_grant)) {
	param->type = VBOX_TYPE_INVALID;

	return;
  }

  param->type = VBOX_TYPE_PTR;
  param->ptr.grant = indir_grant;
  param->ptr.off = off;
  param->ptr.size = size;
  param->ptr.dir = dir;
}

void vbox_put(vbox_param_t *param, int count)
{
/* Free all resources used for the given parameters.
 */

  while (count--) {
	if (param->type == VBOX_TYPE_PTR && GRANT_VALID(param->ptr.grant))
		cpf_revoke(param->ptr.grant);

	param++;
  }
}

u32_t vbox_get_u32(vbox_param_t *param)
{
/* Retrieve the 32-bit value from the given parameter.
 */

  assert(param->type == VBOX_TYPE_U32);
  return param->u32;
}

u64_t vbox_get_u64(vbox_param_t *param)
{
/* Retrieve the 64-bit value from the given parameter.
 */

  assert(param->type == VBOX_TYPE_U64);
  return param->u64;
}
