
#include <string.h>

#include "fs.h"
#include "file.h"

/*===========================================================================*
 *				do_gcov_flush				     *
 *===========================================================================*/
int do_gcov_flush(void)
{
/* A userland tool has requested the gcov data from another
 * process (possibly vfs itself). Grant the target process
 * access to the supplied buffer, and perform the call that
 * makes the target copy its buffer to the caller (incl vfs
 * itself).
 */
  char label[LABEL_MAX];
  vir_bytes labeladdr, buf;
  size_t labellen, size;
  endpoint_t endpt;
  cp_grant_id_t grantid;
  int r;
  message m;

  /*
   * Something as sensitive as system service coverage information must be
   * call to the target service, and so it is not impossible to deadlock the
   * system with this call.
   */
  if (!super_user) return(EPERM);

  labeladdr = job_m_in.m_lc_vfs_gcov.label;
  labellen = job_m_in.m_lc_vfs_gcov.labellen;
  buf = job_m_in.m_lc_vfs_gcov.buf;
  size = job_m_in.m_lc_vfs_gcov.buflen;

  /* Retrieve and look up the target label. */
  if (labellen >= sizeof(label))
	return EINVAL;
  if ((r = sys_datacopy_wrapper(who_e, labeladdr, SELF, (vir_bytes)label,
    labellen)) != OK)
	return r;
  label[labellen - 1] = '\0';

  if ((r = ds_retrieve_label_endpt(label, &endpt)) != OK)
	return r;

  /* Hack: init is the only non-system process with a valid label. */
  if (endpt == INIT_PROC_NR)
	return ENOENT;

  /* Grant target process to requestor's buffer. */
  if ((grantid = cpf_grant_magic(endpt, who_e, buf, size, CPF_WRITE)) < 0) {
	printf("VFS: gcov_flush: grant failed\n");
	return(ENOMEM);
  }

  if (endpt == VFS_PROC_NR) {
	/* Request is for VFS itself. */
	r = gcov_flush(VFS_PROC_NR, grantid, size);
  } else {
	/* Perform generic GCOV request. */
	memset(&m, 0, sizeof(m));
	m.m_vfs_lsys_gcov.grant = grantid;
	m.m_vfs_lsys_gcov.size = size;
	r = _taskcall(endpt, COMMON_REQ_GCOV_DATA, &m);
  }

  cpf_revoke(grantid);

  return(r);
}
