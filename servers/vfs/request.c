/* This file contains the wrapper functions for issuing a request
 * and receiving response from FS processes.
 * Each function builds a request message according to the request
 * parameter, calls the most low-level fs_sendrec, and copies
 * back the response.
 */

#include "fs.h"
#include <minix/com.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <minix/u64.h>
#include <minix/vfsif.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "path.h"
#include "vmnt.h"
#include "vnode.h"


/*===========================================================================*
 *			req_breadwrite_actual				     *
 *===========================================================================*/
static int req_breadwrite_actual(endpoint_t fs_e, endpoint_t user_e, dev_t dev, off_t pos,
        unsigned int num_of_bytes, vir_bytes user_addr, int rw_flag,
        off_t *new_pos, unsigned int *cum_iop, int cpflag)
{
  int r;
  cp_grant_id_t grant_id;
  message m;

  grant_id = cpf_grant_magic(fs_e, user_e, user_addr, num_of_bytes,
			(rw_flag == READING ? CPF_WRITE : CPF_READ) | cpflag);
  if(grant_id == -1)
	  panic("req_breadwrite: cpf_grant_magic failed");

  /* Fill in request message */
  m.m_type = rw_flag == READING ? REQ_BREAD : REQ_BWRITE;
  m.REQ_DEV = dev;
  m.REQ_GRANT = grant_id;
  m.REQ_SEEK_POS = pos;
  m.REQ_NBYTES = num_of_bytes;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);
  if (r != OK) return(r);

  /* Fill in response structure */
  *new_pos = m.RES_SEEK_POS;
  *cum_iop = m.RES_NBYTES;

  return(OK);
}

int req_breadwrite(endpoint_t fs_e, endpoint_t user_e, dev_t dev, off_t pos,
        unsigned int num_of_bytes, vir_bytes user_addr, int rw_flag,
        off_t *new_pos, unsigned int *cum_iop)
{
	int r;

	r = req_breadwrite_actual(fs_e, user_e, dev, pos, num_of_bytes,
		user_addr, rw_flag, new_pos, cum_iop, CPF_TRY);

	if(r == EFAULT) {
		if((r=vm_vfs_procctl_handlemem(user_e, user_addr, num_of_bytes,
			rw_flag == READING)) != OK) {
			return r;
		}

		r = req_breadwrite_actual(fs_e, user_e, dev, pos, num_of_bytes,
			user_addr, rw_flag, new_pos, cum_iop, 0);
	}

	return r;
}

/*===========================================================================*
 *			req_bpeek					     *
 *===========================================================================*/
int req_bpeek(endpoint_t fs_e, dev_t dev, off_t pos, unsigned int num_of_bytes)
{
  message m;

  memset(&m, 0, sizeof(m));

  /* Fill in request message */
  m.m_type = REQ_BPEEK;
  m.REQ_DEV = dev;
  m.REQ_SEEK_POS = pos;
  m.REQ_NBYTES = num_of_bytes;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}

/*===========================================================================*
 *				req_chmod	      			     *
 *===========================================================================*/
int req_chmod(
  endpoint_t fs_e,
  ino_t inode_nr,
  mode_t rmode,
  mode_t *new_modep
)
{
  message m;
  int r;

  /* Fill in request message */
  m.m_type = REQ_CHMOD;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_MODE = (pmode_t) rmode;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  /* Copy back actual mode. */
  *new_modep = (mode_t) m.RES_MODE;

  return(r);
}


/*===========================================================================*
 *				req_chown          			     *
 *===========================================================================*/
int req_chown(
  endpoint_t fs_e,
  ino_t inode_nr,
  uid_t newuid,
  gid_t newgid,
  mode_t *new_modep
)
{
  message m;
  int r;

  /* Fill in request message */
  m.m_type = REQ_CHOWN;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_UID = (puid_t) newuid;
  m.REQ_GID = (pgid_t) newgid;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  /* Return new mode to caller. */
  *new_modep = (mode_t) m.RES_MODE;

  return(r);
}


/*===========================================================================*
 *				req_create				     *
 *===========================================================================*/
int req_create(
  endpoint_t fs_e,
  ino_t inode_nr,
  int omode,
  uid_t uid,
  gid_t gid,
  char *path,
  node_details_t *res
)
{
  int r;
  cp_grant_id_t grant_id;
  size_t len;
  message m;
  struct vmnt *vmp;

  vmp = find_vmnt(fs_e);

  len = strlen(path) + 1;
  grant_id = cpf_grant_direct(fs_e, (vir_bytes) path, len, CPF_READ);
  if (grant_id == -1)
	panic("req_create: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_CREATE;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_MODE = (pmode_t) omode;
  m.REQ_UID = (puid_t) uid;
  m.REQ_GID = (pgid_t) gid;
  m.REQ_GRANT = grant_id;
  m.REQ_PATH_LEN = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);
  if (r != OK) return(r);

  /* Fill in response structure */
  res->fs_e	= m.m_source;
  res->inode_nr	= (ino_t) m.RES_INODE_NR;
  res->fmode	= (mode_t) m.RES_MODE;
  res->fsize    = m.RES_FILE_SIZE;
  res->uid	= (uid_t) m.RES_UID;
  res->gid	= (gid_t) m.RES_GID;
  res->dev	= NO_DEV;

  return(OK);
}


/*===========================================================================*
 *				req_flush	      			     *
 *===========================================================================*/
int req_flush(endpoint_t fs_e, dev_t dev)
{
  message m;

  /* Fill in request message */
  m.m_type = REQ_FLUSH;
  m.REQ_DEV = dev;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_statvfs	    			     *
 *===========================================================================*/
int req_statvfs(endpoint_t fs_e, struct statvfs *buf)
{
  int r;
  cp_grant_id_t grant_id;
  message m;

  grant_id = cpf_grant_direct(fs_e, (vir_bytes) buf, sizeof(struct statvfs),
			CPF_WRITE);
  if(grant_id == GRANT_INVALID)
	panic("req_statvfs: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_STATVFS;
  m.REQ_GRANT = grant_id;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_ftrunc	     			     *
 *===========================================================================*/
int req_ftrunc(endpoint_t fs_e, ino_t inode_nr, off_t start, off_t end)
{
  message m;
  struct vmnt *vmp;

  vmp = find_vmnt(fs_e);

  /* Fill in request message */
  m.m_type = REQ_FTRUNC;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_TRC_START = start;
  m.REQ_TRC_END = end;

  if (!(vmp->m_fs_flags & RES_64BIT) &&
	((start > INT_MAX) || (end > INT_MAX))) {
	/* FS does not support 64-bit off_t and 32 bits is not enough */
	return EINVAL;
  }

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_getdents_actual    			     *
 *===========================================================================*/
static int req_getdents_actual(
  endpoint_t fs_e,
  ino_t inode_nr,
  off_t pos,
  char *buf,
  size_t size,
  off_t *new_pos,
  int direct,
  int cpflag
)
{
  int r;
  message m;
  cp_grant_id_t grant_id;
  struct vmnt *vmp;

  vmp = find_vmnt(fs_e);
  assert(vmp != NULL);

  if (direct) {
	grant_id = cpf_grant_direct(fs_e, (vir_bytes) buf, size, CPF_WRITE);
  } else {
	grant_id = cpf_grant_magic(fs_e, who_e, (vir_bytes) buf, size,
				   CPF_WRITE | cpflag);
  }

  if (grant_id < 0)
	panic("req_getdents: cpf_grant_direct/cpf_grant_magic failed: %d",
								grant_id);

  m.m_type = REQ_GETDENTS;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_GRANT = grant_id;
  m.REQ_MEM_SIZE = size;
  m.REQ_SEEK_POS = pos;
  if (!(vmp->m_fs_flags & RES_64BIT) && (pos > INT_MAX)) {
	/* FS does not support 64-bit off_t and 32 bits is not enough */
	return EINVAL;
  }

  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  if (r == OK) {
	*new_pos = m.RES_SEEK_POS;
	r = m.RES_NBYTES;
  }

  return(r);
}

/*===========================================================================*
 *				req_getdents	     			     *
 *===========================================================================*/
int req_getdents(
  endpoint_t fs_e,
  ino_t inode_nr,
  off_t pos,
  char *buf,
  size_t size,
  off_t *new_pos,
  int direct)
{
	int r;

	r = req_getdents_actual(fs_e, inode_nr, pos, buf, size, new_pos,
		direct, CPF_TRY);

	if(r == EFAULT && !direct) {
		if((r=vm_vfs_procctl_handlemem(who_e, (vir_bytes) buf,
			size, 1)) != OK) {
			return r;
		}

		r = req_getdents_actual(fs_e, inode_nr, pos, buf, size,
			new_pos, direct, 0);
	}

	return r;
}

/*===========================================================================*
 *				req_inhibread	  			     *
 *===========================================================================*/
int req_inhibread(endpoint_t fs_e, ino_t inode_nr)
{
  message m;

  /* Fill in request message */
  m.m_type = REQ_INHIBREAD;
  m.REQ_INODE_NR = (pino_t) inode_nr;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_link	       			     *
 *===========================================================================*/
int req_link(
  endpoint_t fs_e,
  ino_t link_parent,
  char *lastc,
  ino_t linked_file
)
{
  int r;
  cp_grant_id_t grant_id;
  const size_t len = strlen(lastc) + 1;
  message m;

  grant_id = cpf_grant_direct(fs_e, (vir_bytes)lastc, len, CPF_READ);
  if(grant_id == -1)
	  panic("req_link: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_LINK;
  m.REQ_INODE_NR = (pino_t) linked_file;
  m.REQ_DIR_INO = (pino_t) link_parent;
  m.REQ_GRANT = grant_id;
  m.REQ_PATH_LEN = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_lookup	                   	     *
 *===========================================================================*/
int req_lookup(
  endpoint_t fs_e,
  ino_t dir_ino,
  ino_t root_ino,
  uid_t uid,
  gid_t gid,
  struct lookup *resolve,
  lookup_res_t *res,
  struct fproc *rfp
)
{
  message m;
  vfs_ucred_t credentials;
  int r, flags;
  size_t len;
  struct vmnt *vmp;
  cp_grant_id_t grant_id=0, grant_id2=0;

  vmp = find_vmnt(fs_e);

  grant_id = cpf_grant_direct(fs_e, (vir_bytes) resolve->l_path, PATH_MAX,
			      CPF_READ | CPF_WRITE);
  if(grant_id == -1)
	  panic("req_lookup: cpf_grant_direct failed");

  flags = resolve->l_flags;
  len = strlen(resolve->l_path) + 1;

  m.m_type			= REQ_LOOKUP;
  m.m_vfs_fs_lookup.grant_path	= grant_id;
  m.m_vfs_fs_lookup.path_len 	= len;
  m.m_vfs_fs_lookup.path_size 	= PATH_MAX + 1;
  m.m_vfs_fs_lookup.dir_ino 	= dir_ino;
  m.m_vfs_fs_lookup.root_ino 	= root_ino;

  if(rfp->fp_ngroups > 0) { /* Is the process member of multiple groups? */
	/* In that case the FS has to copy the uid/gid credentials */
	int i;

	/* Set credentials */
	credentials.vu_uid = rfp->fp_effuid;
	credentials.vu_gid = rfp->fp_effgid;
	credentials.vu_ngroups = rfp->fp_ngroups;
	for (i = 0; i < rfp->fp_ngroups; i++)
		credentials.vu_sgroups[i] = rfp->fp_sgroups[i];

	grant_id2 = cpf_grant_direct(fs_e, (vir_bytes) &credentials,
				     sizeof(credentials), CPF_READ);
	if(grant_id2 == -1)
		panic("req_lookup: cpf_grant_direct failed");

	m.m_vfs_fs_lookup.grant_ucred	= grant_id2;
	m.m_vfs_fs_lookup.ucred_size	= sizeof(credentials);
	flags		|= PATH_GET_UCRED;
  } else {
	/* When there's only one gid, we can send it directly */
	m.m_vfs_fs_lookup.uid = uid;
	m.m_vfs_fs_lookup.gid = gid;
	flags		&= ~PATH_GET_UCRED;
  }

  m.m_vfs_fs_lookup.flags = flags;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);
  if(rfp->fp_ngroups > 0) cpf_revoke(grant_id2);

  /* Fill in response according to the return value */
  res->fs_e = m.m_source;

  switch (r) {
  case OK:
	res->inode_nr = m.m_fs_vfs_lookup.inode;
	res->fmode = m.m_fs_vfs_lookup.mode;
	res->fsize = m.m_fs_vfs_lookup.file_size;
	res->dev = m.m_fs_vfs_lookup.device;
	res->uid = m.m_fs_vfs_lookup.uid;
	res->gid = m.m_fs_vfs_lookup.gid;
	break;
  case EENTERMOUNT:
	res->inode_nr = m.m_fs_vfs_lookup.inode;
	res->char_processed = m.m_fs_vfs_lookup.offset;
	res->symloop = m.m_fs_vfs_lookup.symloop;
	break;
  case ELEAVEMOUNT:
	res->char_processed = m.m_fs_vfs_lookup.offset;
	res->symloop = m.m_fs_vfs_lookup.symloop;
	break;
  case ESYMLINK:
	res->char_processed = m.m_fs_vfs_lookup.offset;
	res->symloop = m.m_fs_vfs_lookup.symloop;
	break;
  default:
	break;
  }

  return(r);
}


/*===========================================================================*
 *				req_mkdir	      			     *
 *===========================================================================*/
int req_mkdir(
  endpoint_t fs_e,
  ino_t inode_nr,
  char *lastc,
  uid_t uid,
  gid_t gid,
  mode_t dmode
)
{
  int r;
  cp_grant_id_t grant_id;
  size_t len;
  message m;

  len = strlen(lastc) + 1;
  grant_id = cpf_grant_direct(fs_e, (vir_bytes)lastc, len, CPF_READ);
  if(grant_id == -1)
	  panic("req_mkdir: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_MKDIR;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_MODE = (pmode_t) dmode;
  m.REQ_UID = (puid_t) uid;
  m.REQ_GID = (pgid_t) gid;
  m.REQ_GRANT = grant_id;
  m.REQ_PATH_LEN = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_mknod	      			     *
 *===========================================================================*/
int req_mknod(
  endpoint_t fs_e,
  ino_t inode_nr,
  char *lastc,
  uid_t uid,
  gid_t gid,
  mode_t dmode,
  dev_t dev
)
{
  int r;
  size_t len;
  cp_grant_id_t grant_id;
  message m;

  len = strlen(lastc) + 1;
  grant_id = cpf_grant_direct(fs_e, (vir_bytes)lastc, len, CPF_READ);
  if(grant_id == -1)
	  panic("req_mknod: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_MKNOD;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_MODE = (pmode_t) dmode;
  m.REQ_DEV = dev;
  m.REQ_UID = (puid_t) uid;
  m.REQ_GID = (pgid_t) gid;
  m.REQ_GRANT = grant_id;
  m.REQ_PATH_LEN = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_mountpoint	                 	     *
 *===========================================================================*/
int req_mountpoint(endpoint_t fs_e, ino_t inode_nr)
{
  message m;

  /* Fill in request message */
  m.m_type = REQ_MOUNTPOINT;
  m.REQ_INODE_NR = (pino_t) inode_nr;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_newnode	      			     *
 *===========================================================================*/
int req_newnode(
  endpoint_t fs_e,
  uid_t uid,
  gid_t gid,
  mode_t dmode,
  dev_t dev,
  struct node_details *res
)
{
  struct vmnt *vmp;
  int r;
  message m;

  vmp = find_vmnt(fs_e);

  /* Fill in request message */
  m.m_type = REQ_NEWNODE;
  m.REQ_MODE = (pmode_t) dmode;
  m.REQ_DEV = dev;
  m.REQ_UID = (puid_t) uid;
  m.REQ_GID = (pgid_t) gid;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  res->fs_e	= m.m_source;
  res->inode_nr = (ino_t) m.RES_INODE_NR;
  res->fmode	= (mode_t) m.RES_MODE;
  res->fsize    = m.RES_FILE_SIZE;
  res->dev	= m.RES_DEV;
  res->uid	= (uid_t) m.RES_UID;
  res->gid	= (gid_t) m.RES_GID;

  return(r);
}


/*===========================================================================*
 *				req_newdriver          			     *
 *===========================================================================*/
int req_newdriver(
  endpoint_t fs_e,
  dev_t dev,
  char *label
)
{
  cp_grant_id_t grant_id;
  size_t len;
  message m;
  int r;

  /* Grant access to label */
  len = strlen(label) + 1;
  grant_id = cpf_grant_direct(fs_e, (vir_bytes) label, len, CPF_READ);
  if (grant_id == -1)
	panic("req_newdriver: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_NEW_DRIVER;
  m.REQ_DEV = dev;
  m.REQ_GRANT = grant_id;
  m.REQ_PATH_LEN = len;

  /* Issue request */
  r = fs_sendrec(fs_e, &m);

  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_putnode				     *
 *===========================================================================*/
int req_putnode(fs_e, inode_nr, count)
int fs_e;
ino_t inode_nr;
int count;
{
  message m;

  /* Fill in request message */
  m.m_type = REQ_PUTNODE;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_COUNT = count;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_rdlink_actual     			     *
 *===========================================================================*/
static int req_rdlink_actual(endpoint_t fs_e, ino_t inode_nr,
	endpoint_t proc_e, vir_bytes buf, size_t len,
	int direct, /* set to 1 to use direct grants instead of magic grants */
	int cpflag)
{
  message m;
  int r;
  cp_grant_id_t grant_id;

  if (direct) {
	grant_id = cpf_grant_direct(fs_e, buf, len, CPF_WRITE);
  } else {
	grant_id = cpf_grant_magic(fs_e, proc_e, buf, len, CPF_WRITE | cpflag);
  }
  if (grant_id == -1)
	  panic("req_rdlink: cpf_grant_magic failed");

  /* Fill in request message */
  m.m_type = REQ_RDLINK;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_GRANT = grant_id;
  m.REQ_MEM_SIZE = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  if (r == OK) r = m.RES_NBYTES;

  return(r);
}

/*===========================================================================*
 *				req_rdlink	     			     *
 *===========================================================================*/
int req_rdlink(endpoint_t fs_e, ino_t inode_nr, endpoint_t proc_e,
	vir_bytes buf, size_t len,
	int direct /* set to 1 to use direct grants instead of magic grants */
)
{
	int r;

	r = req_rdlink_actual(fs_e, inode_nr, proc_e, buf, len, direct,
		CPF_TRY);

	if(r == EFAULT && !direct) {
		if((r=vm_vfs_procctl_handlemem(proc_e, buf, len, 1)) != OK) {
			return r;
		}

		r = req_rdlink_actual(fs_e, inode_nr, proc_e, buf, len,
			direct, 0);
	}

	return r;
}

/*===========================================================================*
 *				req_readsuper	                  	     *
 *===========================================================================*/
int req_readsuper(
  struct vmnt *vmp,
  char *label,
  dev_t dev,
  int readonly,
  int isroot,
  struct node_details *res,
  unsigned int *fs_flags
)
{
  int r;
  cp_grant_id_t grant_id;
  size_t len;
  message m;
  endpoint_t fs_e;

  fs_e = vmp->m_fs_e;

  len = strlen(label)+1;
  grant_id = cpf_grant_direct(fs_e, (vir_bytes) label, len, CPF_READ);
  if (grant_id == -1)
	  panic("req_readsuper: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_READSUPER;
  m.m_vfs_fs_readsuper.flags = 0;
  if(readonly) m.m_vfs_fs_readsuper.flags |= REQ_RDONLY;
  if(isroot)   m.m_vfs_fs_readsuper.flags |= REQ_ISROOT;
  m.m_vfs_fs_readsuper.grant = grant_id;
  m.m_vfs_fs_readsuper.device = dev;
  m.m_vfs_fs_readsuper.path_len = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  if(r == OK) {
	/* Fill in response structure */
	res->fs_e = m.m_source;
	res->inode_nr = m.m_fs_vfs_readsuper.inode;
	res->fmode = m.m_fs_vfs_readsuper.mode;
	res->fsize = m.m_fs_vfs_readsuper.file_size;
	res->uid = m.m_fs_vfs_readsuper.uid;
	res->gid = m.m_fs_vfs_readsuper.gid;
	*fs_flags = m.m_fs_vfs_readsuper.flags;
  }

  return(r);
}


/*===========================================================================*
 *				req_readwrite_actual			     *
 *===========================================================================*/
static int req_readwrite_actual(endpoint_t fs_e, ino_t inode_nr, off_t pos,
	int rw_flag, endpoint_t user_e, vir_bytes user_addr,
	unsigned int num_of_bytes, off_t *new_posp, unsigned int *cum_iop,
	int cpflag)
{
  struct vmnt *vmp;
  int r;
  cp_grant_id_t grant_id;
  message m;

  vmp = find_vmnt(fs_e);

  grant_id = cpf_grant_magic(fs_e, user_e, user_addr, num_of_bytes,
			     (rw_flag==READING ? CPF_WRITE:CPF_READ) | cpflag);
  if (grant_id == -1)
	  panic("req_readwrite: cpf_grant_magic failed");

  /* Fill in request message */
  m.m_type = rw_flag == READING ? REQ_READ : REQ_WRITE;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_GRANT = grant_id;
  m.REQ_SEEK_POS = pos;
  if ((!(vmp->m_fs_flags & RES_64BIT)) && (pos > INT_MAX)) {
	return EINVAL;
  }
  m.REQ_NBYTES = num_of_bytes;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  if (r == OK) {
	/* Fill in response structure */
	*new_posp = m.RES_SEEK_POS;
	*cum_iop = m.RES_NBYTES;
  }

  return(r);
}

/*===========================================================================*
 *				req_readwrite				     *
 *===========================================================================*/
int req_readwrite(endpoint_t fs_e, ino_t inode_nr, off_t pos,
	int rw_flag, endpoint_t user_e, vir_bytes user_addr,
	unsigned int num_of_bytes, off_t *new_posp, unsigned int *cum_iop)
{
	int r;

	r = req_readwrite_actual(fs_e, inode_nr, pos, rw_flag, user_e,
		user_addr, num_of_bytes, new_posp, cum_iop, CPF_TRY);

	if(r == EFAULT) {
		if((r=vm_vfs_procctl_handlemem(user_e, (vir_bytes) user_addr, num_of_bytes,
			rw_flag == READING)) != OK) {
			return r;
		}

		r = req_readwrite_actual(fs_e, inode_nr, pos, rw_flag, user_e,
			user_addr, num_of_bytes, new_posp, cum_iop, 0);
	}

	return r;
}

/*===========================================================================*
 *				req_peek				     *
 *===========================================================================*/
int req_peek(endpoint_t fs_e, ino_t inode_nr, off_t pos, unsigned int bytes)
{
  message m;

  memset(&m, 0, sizeof(m));

  if (ex64hi(pos) != 0)
	  panic("req_peek: pos too large");

  /* Fill in request message */
  m.m_type = REQ_PEEK;
  m.REQ_INODE_NR = inode_nr;
  m.REQ_GRANT = -1;
  m.REQ_SEEK_POS = pos;
  m.REQ_NBYTES = bytes;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}

/*===========================================================================*
 *				req_rename	     			     *
 *===========================================================================*/
int req_rename(fs_e, old_dir, old_name, new_dir, new_name)
endpoint_t fs_e;
ino_t old_dir;
char *old_name;
ino_t new_dir;
char *new_name;
{
  int r;
  cp_grant_id_t gid_old, gid_new;
  size_t len_old, len_new;
  message m;

  len_old = strlen(old_name) + 1;
  gid_old = cpf_grant_direct(fs_e, (vir_bytes) old_name, len_old, CPF_READ);
  if(gid_old == -1)
	  panic("req_rename: cpf_grant_direct failed");

  len_new = strlen(new_name) + 1;
  gid_new = cpf_grant_direct(fs_e, (vir_bytes) new_name, len_new, CPF_READ);
  if(gid_new == -1)
	  panic("req_rename: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_RENAME;
  m.REQ_REN_OLD_DIR = (pino_t) old_dir;
  m.REQ_REN_NEW_DIR = (pino_t) new_dir;
  m.REQ_REN_GRANT_OLD = gid_old;
  m.REQ_REN_LEN_OLD = len_old;
  m.REQ_REN_GRANT_NEW = gid_new;
  m.REQ_REN_LEN_NEW = len_new;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(gid_old);
  cpf_revoke(gid_new);

  return(r);
}


/*===========================================================================*
 *				req_rmdir	      			     *
 *===========================================================================*/
int req_rmdir(fs_e, inode_nr, lastc)
endpoint_t fs_e;
ino_t inode_nr;
char *lastc;
{
  int r;
  cp_grant_id_t grant_id;
  size_t len;
  message m;

  len = strlen(lastc) + 1;
  grant_id = cpf_grant_direct(fs_e, (vir_bytes) lastc, len, CPF_READ);
  if(grant_id == -1)
	  panic("req_rmdir: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_RMDIR;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_GRANT = grant_id;
  m.REQ_PATH_LEN = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_slink_actual      			     *
 *===========================================================================*/
static int req_slink_actual(
  endpoint_t fs_e,
  ino_t inode_nr,
  char *lastc,
  endpoint_t proc_e,
  vir_bytes path_addr,
  size_t path_length,
  uid_t uid,
  gid_t gid,
  int cpflag
)
{
  int r;
  size_t len;
  cp_grant_id_t gid_name, gid_buf;
  message m;

  len = strlen(lastc) + 1;
  gid_name = cpf_grant_direct(fs_e, (vir_bytes) lastc, len, CPF_READ);
  if (gid_name == GRANT_INVALID)
	  panic("req_slink: cpf_grant_direct failed");

  gid_buf = cpf_grant_magic(fs_e, proc_e, path_addr, path_length,
	CPF_READ | cpflag);

  if (gid_buf == GRANT_INVALID) {
	  cpf_revoke(gid_name);
	  panic("req_slink: cpf_grant_magic failed");
  }

  /* Fill in request message */
  m.m_type = REQ_SLINK;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_UID = (puid_t) uid;
  m.REQ_GID = (pgid_t) gid;
  m.REQ_GRANT = gid_name;
  m.REQ_PATH_LEN = len;
  m.REQ_GRANT3 = gid_buf;
  m.REQ_MEM_SIZE = path_length;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(gid_name);
  cpf_revoke(gid_buf);

  return(r);
}

/*===========================================================================*
 *				req_slink	      			     *
 *===========================================================================*/
int req_slink(
  endpoint_t fs_e,
  ino_t inode_nr,
  char *lastc,
  endpoint_t proc_e,
  vir_bytes path_addr,
  size_t path_length,
  uid_t uid,
  gid_t gid
)
{
	int r;

	r = req_slink_actual(fs_e, inode_nr, lastc, proc_e, path_addr,
		path_length, uid, gid, CPF_TRY);

	if(r == EFAULT) {
		if((r=vm_vfs_procctl_handlemem(proc_e, (vir_bytes) path_addr,
			path_length, 0)) != OK) {
			return r;
		}

		r = req_slink_actual(fs_e, inode_nr, lastc, proc_e, path_addr,
			path_length, uid, gid, 0);
	}

	return r;
}

/*===========================================================================*
 *				req_stat_actual	       			     *
 *===========================================================================*/
int req_stat_actual(endpoint_t fs_e, ino_t inode_nr, endpoint_t proc_e,
	vir_bytes buf, int cpflag)
{
  cp_grant_id_t grant_id;
  int r;
  message m;

  /* Grant FS access to copy straight into user provided buffer */
  grant_id = cpf_grant_magic(fs_e, proc_e, buf, sizeof(struct stat),
	CPF_WRITE | cpflag);

  if (grant_id < 0)
	panic("req_stat: cpf_grant_* failed");

  /* Fill in request message */
  m.m_type = REQ_STAT;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_GRANT = grant_id;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_stat	       			     *
 *===========================================================================*/
int req_stat(endpoint_t fs_e, ino_t inode_nr, endpoint_t proc_e,
	vir_bytes buf)
{
	int r;

	r = req_stat_actual(fs_e, inode_nr, proc_e, buf, CPF_TRY);

	if(r == EFAULT) {
		if((r=vm_vfs_procctl_handlemem(proc_e, (vir_bytes) buf,
			sizeof(struct stat), 1)) != OK) {
			return r;
		}

		r = req_stat_actual(fs_e, inode_nr, proc_e, buf, 0);
	}

	return r;
}

/*===========================================================================*
 *				req_sync	       			     *
 *===========================================================================*/
int req_sync(fs_e)
endpoint_t fs_e;
{
  message m;

  /* Fill in request message */
  m.m_type = REQ_SYNC;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_unlink	     			     *
 *===========================================================================*/
int req_unlink(fs_e, inode_nr, lastc)
endpoint_t fs_e;
ino_t inode_nr;
char *lastc;
{
  cp_grant_id_t grant_id;
  size_t len;
  int r;
  message m;

  len = strlen(lastc) + 1;
  grant_id = cpf_grant_direct(fs_e, (vir_bytes) lastc, len, CPF_READ);
  if(grant_id == -1)
	  panic("req_unlink: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_UNLINK;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_GRANT = grant_id;
  m.REQ_PATH_LEN = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_unmount	    			     *
 *===========================================================================*/
int req_unmount(fs_e)
endpoint_t fs_e;
{
  message m;

  /* Fill in request message */
  m.m_type = REQ_UNMOUNT;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_utime	      			     *
 *===========================================================================*/
int req_utime(endpoint_t fs_e, ino_t inode_nr, struct timespec * actimespec,
	struct timespec * modtimespec)
{
  message m;

  assert(actimespec != NULL);
  assert(modtimespec != NULL);

  /* Fill in request message */
  m.m_type = REQ_UTIME;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_INODE_NR = inode_nr;
  m.REQ_ACTIME = actimespec->tv_sec;
  m.REQ_MODTIME = modtimespec->tv_sec;
  m.REQ_ACNSEC = actimespec->tv_nsec;
  m.REQ_MODNSEC = modtimespec->tv_nsec;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}
