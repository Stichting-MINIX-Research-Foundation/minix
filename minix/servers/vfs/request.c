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
        off_t *new_pos, size_t *cum_iop, int cpflag)
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
  m.m_vfs_fs_breadwrite.device = dev;
  m.m_vfs_fs_breadwrite.grant = grant_id;
  m.m_vfs_fs_breadwrite.seek_pos = pos;
  m.m_vfs_fs_breadwrite.nbytes = num_of_bytes;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  if (cpf_revoke(grant_id) == GRANT_FAULTED) return(ERESTART);

  if (r != OK) return(r);

  /* Fill in response structure */
  *new_pos = m.m_fs_vfs_breadwrite.seek_pos;
  *cum_iop = m.m_fs_vfs_breadwrite.nbytes;

  return(OK);
}

int req_breadwrite(endpoint_t fs_e, endpoint_t user_e, dev_t dev, off_t pos,
        unsigned int num_of_bytes, vir_bytes user_addr, int rw_flag,
        off_t *new_pos, size_t *cum_iop)
{
	int r;

	r = req_breadwrite_actual(fs_e, user_e, dev, pos, num_of_bytes,
		user_addr, rw_flag, new_pos, cum_iop, CPF_TRY);

	if (r == ERESTART) {
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
  m.m_vfs_fs_breadwrite.device = dev;
  m.m_vfs_fs_breadwrite.seek_pos = pos;
  m.m_vfs_fs_breadwrite.nbytes = num_of_bytes;

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
  m.m_vfs_fs_chmod.inode = inode_nr;
  m.m_vfs_fs_chmod.mode = rmode;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  /* Copy back actual mode. */
  *new_modep = m.m_fs_vfs_chmod.mode;

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
  m.m_vfs_fs_chown.inode = inode_nr;
  m.m_vfs_fs_chown.uid = newuid;
  m.m_vfs_fs_chown.gid = newgid;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  /* Return new mode to caller. */
  *new_modep = m.m_fs_vfs_chown.mode;

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
//  struct vmnt *vmp;

//  vmp = find_vmnt(fs_e);

  len = strlen(path) + 1;
  grant_id = cpf_grant_direct(fs_e, (vir_bytes) path, len, CPF_READ);
  if (grant_id == -1)
	panic("req_create: cpf_grant_direct failed");

  /* Fill in request message */
  m.m_type = REQ_CREATE;
  m.m_vfs_fs_create.inode = inode_nr;
  m.m_vfs_fs_create.mode = omode;
  m.m_vfs_fs_create.uid = uid;
  m.m_vfs_fs_create.gid = gid;
  m.m_vfs_fs_create.grant = grant_id;
  m.m_vfs_fs_create.path_len = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);
  if (r != OK) return(r);

  /* Fill in response structure */
  res->fs_e	= m.m_source;
  res->inode_nr	= m.m_fs_vfs_create.inode;
  res->fmode	= m.m_fs_vfs_create.mode;
  res->fsize    = m.m_fs_vfs_create.file_size;
  res->uid	= m.m_fs_vfs_create.uid;
  res->gid	= m.m_fs_vfs_create.gid;
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
  m.m_vfs_fs_flush.device = dev;

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
  m.m_vfs_fs_statvfs.grant = grant_id;

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
  m.m_vfs_fs_ftrunc.inode = inode_nr;
  m.m_vfs_fs_ftrunc.trc_start = start;
  m.m_vfs_fs_ftrunc.trc_end = end;

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
  vir_bytes buf,
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
	grant_id = cpf_grant_direct(fs_e, buf, size, CPF_WRITE);
  } else {
	grant_id = cpf_grant_magic(fs_e, who_e, buf, size,
				   CPF_WRITE | cpflag);
  }

  if (grant_id < 0)
	panic("req_getdents: cpf_grant_direct/cpf_grant_magic failed: %d",
								grant_id);

  m.m_type = REQ_GETDENTS;
  m.m_vfs_fs_getdents.inode = inode_nr;
  m.m_vfs_fs_getdents.grant = grant_id;
  m.m_vfs_fs_getdents.mem_size = size;
  m.m_vfs_fs_getdents.seek_pos = pos;
  if (!(vmp->m_fs_flags & RES_64BIT) && (pos > INT_MAX)) {
	/* FS does not support 64-bit off_t and 32 bits is not enough */
	return EINVAL;
  }

  r = fs_sendrec(fs_e, &m);

  if (cpf_revoke(grant_id) == GRANT_FAULTED) return(ERESTART);

  if (r == OK) {
	*new_pos = m.m_fs_vfs_getdents.seek_pos;
	r = m.m_fs_vfs_getdents.nbytes;
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
  vir_bytes buf,
  size_t size,
  off_t *new_pos,
  int direct)
{
	int r;

	r = req_getdents_actual(fs_e, inode_nr, pos, buf, size, new_pos,
		direct, CPF_TRY);

	if (r == ERESTART) {
		assert(!direct);

		if((r=vm_vfs_procctl_handlemem(who_e, buf, size, 1)) != OK) {
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
  m.m_vfs_fs_inhibread.inode = inode_nr;

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
  m.m_vfs_fs_link.inode = linked_file;
  m.m_vfs_fs_link.dir_ino = link_parent;
  m.m_vfs_fs_link.grant = grant_id;
  m.m_vfs_fs_link.path_len = len;

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
//  struct vmnt *vmp;
  cp_grant_id_t grant_id=0, grant_id2=0;

//  vmp = find_vmnt(fs_e);

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
  m.m_vfs_fs_mkdir.inode = inode_nr;
  m.m_vfs_fs_mkdir.mode = dmode;
  m.m_vfs_fs_mkdir.uid = uid;
  m.m_vfs_fs_mkdir.gid = gid;
  m.m_vfs_fs_mkdir.grant = grant_id;
  m.m_vfs_fs_mkdir.path_len = len;

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
  m.m_vfs_fs_mknod.inode = inode_nr;
  m.m_vfs_fs_mknod.mode = dmode;
  m.m_vfs_fs_mknod.device = dev;
  m.m_vfs_fs_mknod.uid = uid;
  m.m_vfs_fs_mknod.gid = gid;
  m.m_vfs_fs_mknod.grant = grant_id;
  m.m_vfs_fs_mknod.path_len = len;

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
  m.m_vfs_fs_mountpoint.inode = inode_nr;

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
//  struct vmnt *vmp;
  int r;
  message m;

//  vmp = find_vmnt(fs_e);

  /* Fill in request message */
  m.m_type = REQ_NEWNODE;
  m.m_vfs_fs_newnode.mode = dmode;
  m.m_vfs_fs_newnode.device = dev;
  m.m_vfs_fs_newnode.uid = uid;
  m.m_vfs_fs_newnode.gid = gid;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  res->fs_e	= m.m_source;
  res->inode_nr = m.m_fs_vfs_newnode.inode;
  res->fmode	= m.m_fs_vfs_newnode.mode;
  res->fsize    = m.m_fs_vfs_newnode.file_size;
  res->dev	= m.m_fs_vfs_newnode.device;
  res->uid	= m.m_fs_vfs_newnode.uid;
  res->gid	= m.m_fs_vfs_newnode.gid;

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
  m.m_vfs_fs_new_driver.device = dev;
  m.m_vfs_fs_new_driver.grant = grant_id;
  m.m_vfs_fs_new_driver.path_len = len;

  /* Issue request */
  r = fs_sendrec(fs_e, &m);

  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_putnode				     *
 *===========================================================================*/
int
req_putnode(int fs_e, ino_t inode_nr, int count)
{
  message m;

  /* Fill in request message */
  m.m_type = REQ_PUTNODE;
  m.m_vfs_fs_putnode.inode = inode_nr;
  m.m_vfs_fs_putnode.count = count;

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
  m.m_vfs_fs_rdlink.inode = inode_nr;
  m.m_vfs_fs_rdlink.grant = grant_id;
  m.m_vfs_fs_rdlink.mem_size = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  if (cpf_revoke(grant_id) == GRANT_FAULTED) return(ERESTART);

  if (r == OK) r = m.m_fs_vfs_rdlink.nbytes;

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

	if (r == ERESTART) {
		assert(!direct);

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
	unsigned int num_of_bytes, off_t *new_posp, size_t *cum_iop,
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
  m.m_vfs_fs_readwrite.inode = inode_nr;
  m.m_vfs_fs_readwrite.grant = grant_id;
  m.m_vfs_fs_readwrite.seek_pos = pos;
  if ((!(vmp->m_fs_flags & RES_64BIT)) && (pos > INT_MAX)) {
	return EINVAL;
  }
  m.m_vfs_fs_readwrite.nbytes = num_of_bytes;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  if (cpf_revoke(grant_id) == GRANT_FAULTED) return(ERESTART);

  if (r == OK) {
	/* Fill in response structure */
	*new_posp = m.m_fs_vfs_readwrite.seek_pos;
	*cum_iop = m.m_fs_vfs_readwrite.nbytes;
  }

  return(r);
}

/*===========================================================================*
 *				req_readwrite				     *
 *===========================================================================*/
int req_readwrite(endpoint_t fs_e, ino_t inode_nr, off_t pos,
	int rw_flag, endpoint_t user_e, vir_bytes user_addr,
	unsigned int num_of_bytes, off_t *new_posp, size_t *cum_iop)
{
	int r;

	r = req_readwrite_actual(fs_e, inode_nr, pos, rw_flag, user_e,
		user_addr, num_of_bytes, new_posp, cum_iop, CPF_TRY);

	if (r == ERESTART) {
		if ((r=vm_vfs_procctl_handlemem(user_e, (vir_bytes) user_addr,
		    num_of_bytes, rw_flag == READING)) != OK) {
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
  m.m_vfs_fs_readwrite.inode = inode_nr;
  m.m_vfs_fs_readwrite.grant = -1;
  m.m_vfs_fs_readwrite.seek_pos = pos;
  m.m_vfs_fs_readwrite.nbytes = bytes;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}

/*===========================================================================*
 *				req_rename	     			     *
 *===========================================================================*/
int
req_rename(endpoint_t fs_e, ino_t old_dir, char *old_name, ino_t new_dir, char *new_name)
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
  m.m_vfs_fs_rename.dir_old = old_dir;
  m.m_vfs_fs_rename.grant_old = gid_old;
  m.m_vfs_fs_rename.len_old = len_old;

  m.m_vfs_fs_rename.dir_new = new_dir;
  m.m_vfs_fs_rename.grant_new = gid_new;
  m.m_vfs_fs_rename.len_new = len_new;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(gid_old);
  cpf_revoke(gid_new);

  return(r);
}


/*===========================================================================*
 *				req_rmdir	      			     *
 *===========================================================================*/
int
req_rmdir(endpoint_t fs_e, ino_t inode_nr, char *lastc)
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
  m.m_vfs_fs_unlink.inode = inode_nr;
  m.m_vfs_fs_unlink.grant = grant_id;
  m.m_vfs_fs_unlink.path_len = len;

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
  m.m_vfs_fs_slink.inode = inode_nr;
  m.m_vfs_fs_slink.uid = uid;
  m.m_vfs_fs_slink.gid = gid;
  m.m_vfs_fs_slink.grant_path = gid_name;
  m.m_vfs_fs_slink.path_len = len;
  m.m_vfs_fs_slink.grant_target = gid_buf;
  m.m_vfs_fs_slink.mem_size = path_length;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  cpf_revoke(gid_name);
  if (cpf_revoke(gid_buf) == GRANT_FAULTED) return(ERESTART);

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

	if (r == ERESTART) {
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
  m.m_vfs_fs_stat.inode = inode_nr;
  m.m_vfs_fs_stat.grant = grant_id;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);

  if (cpf_revoke(grant_id) == GRANT_FAULTED) return(ERESTART);

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

	if (r == ERESTART) {
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
int
req_sync(endpoint_t fs_e)
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
int
req_unlink(endpoint_t fs_e, ino_t inode_nr, char *lastc)
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
  m.m_vfs_fs_unlink.inode = inode_nr;
  m.m_vfs_fs_unlink.grant = grant_id;
  m.m_vfs_fs_unlink.path_len = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  return(r);
}


/*===========================================================================*
 *				req_unmount	    			     *
 *===========================================================================*/
int
req_unmount(endpoint_t fs_e)
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
  m.m_vfs_fs_utime.inode = inode_nr;
  m.m_vfs_fs_utime.actime = actimespec->tv_sec;
  m.m_vfs_fs_utime.modtime = modtimespec->tv_sec;
  m.m_vfs_fs_utime.acnsec = actimespec->tv_nsec;
  m.m_vfs_fs_utime.modnsec = modtimespec->tv_nsec;

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
}
