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
#include "fproc.h"
#include "param.h"
#include "path.h"
#include "vmnt.h"
#include "vnode.h"


static size_t translate_dents(char *src, size_t size, char *dst, int direction);

/*===========================================================================*
 *			req_breadwrite					     *
 *===========================================================================*/
int req_breadwrite(
  endpoint_t fs_e,
  endpoint_t user_e,
  dev_t dev,
  off_t pos,
  unsigned int num_of_bytes,
  vir_bytes user_addr,
  int rw_flag,
  off_t *new_pos,
  unsigned int *cum_iop
)
{
  int r;
  cp_grant_id_t grant_id;
  message m;

  grant_id = cpf_grant_magic(fs_e, user_e, user_addr, num_of_bytes,
			(rw_flag == READING ? CPF_WRITE : CPF_READ));
  if(grant_id == -1)
	  panic("req_breadwrite: cpf_grant_magic failed");

  /* Fill in request message */
  m.m_type = rw_flag == READING ? REQ_BREAD : REQ_BWRITE;
  m.REQ_DEV2 = dev;
  m.REQ_GRANT = grant_id;
  m.REQ_SEEK_POS_LO = ex64lo(pos);
  m.REQ_SEEK_POS_HI = ex64hi(pos);
  m.REQ_NBYTES = num_of_bytes;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);
  if (r != OK) return(r);

  /* Fill in response structure */
  *new_pos = make64(m.RES_SEEK_POS_LO, m.RES_SEEK_POS_HI);
  *cum_iop = m.RES_NBYTES;

  return(OK);
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
  m.REQ_DEV2 = dev;
  m.REQ_SEEK_POS_LO = ex64lo(pos);
  m.REQ_SEEK_POS_HI = ex64hi(pos);
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
  if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
	res->fsize = make64(m.RES_FILE_SIZE_LO, m.RES_FILE_SIZE_HI);
  } else {
	res->fsize = m.RES_FILE_SIZE_LO;
  }
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

  m.REQ_TRC_START_LO = ex64lo(start);
  if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
	m.REQ_TRC_START_HI = ex64hi(start);
  } else if (start > INT_MAX) {
	/* FS does not support 64-bit off_t and 32 bits is not enough */
	return EINVAL;
  } else {
	m.REQ_TRC_START_HI = 0;
  }

  m.REQ_TRC_END_LO = ex64lo(end);
  if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
	m.REQ_TRC_END_HI = ex64hi(end);
  } else if (end > INT_MAX) {
	/* FS does not support 64-bit off_t and 32 bits is not enough */
	return EINVAL;
  } else {
	m.REQ_TRC_END_HI = 0;
  }

  /* Send/rec request */
  return fs_sendrec(fs_e, &m);
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
  int direct,
  int getdents_321	/* Set to 1 if user land expects old format */
)
{
  int r;
  int fs_getdents_321 = 0, do_translation = 0;
  message m;
  cp_grant_id_t grant_id;
  struct vmnt *vmp;
  char *indir_buf_src = NULL;
  char *indir_buf_dst = NULL;

  vmp = find_vmnt(fs_e);
  assert(vmp != NULL);

  if (VFS_FS_PROTO_VERSION(vmp->m_proto) == 0) {
	fs_getdents_321 = 1;
  }

  /* When we have to translate new struct dirent to the old format or vice
   * versa, we're going to have to ignore the user provided buffer and do only
   * one entry at a time. We have to do the translation here and allocate
   * space on the stack. This is a limited resource. Besides, we don't want to
   * be dependent on crazy buffer sizes provided by user space (i.e., we'd have
   * to allocate a similarly sized buffer here).
   *
   * We need to translate iff:
   *  1. userland expects old format and FS provides new format
   *  2. userland expects new format and FS provides old format
   * We don't need to translate iff
   *  3. userland expects old format and FS provides old format
   *  4. userland expects new format and FS provides new format
   *
   * Note: VFS expects new format (when doing 'direct'), covered by case 2.
   */
  if (getdents_321 && !fs_getdents_321) {	/* case 1 */
	do_translation = 1;
  } else if (fs_getdents_321 && !getdents_321) {/* case 2 */
	do_translation = 1;
  }

  if (do_translation) {
	/* We're cutting down the buffer size in two so it's guaranteed we
	 * have enough space for the translation (data structure has become
	 * larger).
	 */
	size = size / 2;
	indir_buf_src = malloc(size);
	indir_buf_dst = malloc(size * 2); /* dst buffer keeps original size */
	if (indir_buf_src == NULL || indir_buf_dst == NULL)
		panic("Couldn't allocate temp buf space\n");

	grant_id = cpf_grant_direct(fs_e, (vir_bytes) indir_buf_src, size,
				CPF_WRITE);
  } else if (direct) {
	grant_id = cpf_grant_direct(fs_e, (vir_bytes) buf, size, CPF_WRITE);
  } else {
	grant_id = cpf_grant_magic(fs_e, who_e, (vir_bytes) buf, size,
				   CPF_WRITE);
  }

  if (grant_id < 0)
	panic("req_getdents: cpf_grant_direct/cpf_grant_magic failed: %d",
								grant_id);

  m.m_type = REQ_GETDENTS;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_GRANT = grant_id;
  m.REQ_MEM_SIZE = size;
  m.REQ_SEEK_POS_LO = ex64lo(pos);
  if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
	m.REQ_SEEK_POS_HI = ex64hi(pos);
  } else if (pos > INT_MAX) {
	/* FS does not support 64-bit off_t and 32 bits is not enough */
	if (indir_buf_src != NULL) free(indir_buf_src);
	if (indir_buf_dst != NULL) free(indir_buf_dst);
	return EINVAL;
  } else {
	m.REQ_SEEK_POS_HI = 0;
  }

  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  if (do_translation) {
	if (r == OK) {
		m.RES_NBYTES = translate_dents(indir_buf_src, m.RES_NBYTES,
						indir_buf_dst, getdents_321);
		if (direct) {
			memcpy(buf, indir_buf_dst, m.RES_NBYTES);
		} else {
			r = sys_vircopy(SELF, (vir_bytes) indir_buf_dst, who_e,
					(vir_bytes) buf, m.RES_NBYTES);
		}
	}
	free(indir_buf_src);
	free(indir_buf_dst);
  }

  if (r == OK) {
	if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
		*new_pos = make64(m.RES_SEEK_POS_LO, m.RES_SEEK_POS_HI);
	} else {
		*new_pos = m.RES_SEEK_POS_LO;
	}
	r = m.RES_NBYTES;
  }

  return(r);
}

/*===========================================================================*
 *				translate_dents	  			     *
 *===========================================================================*/
static size_t
translate_dents(char *src, size_t size, char *dst, int to_getdents_321)
{
/* Convert between 'struct dirent' and 'struct dirent_321' both ways and
 * return the size of the new buffer.
 */
	int consumed = 0, newconsumed = 0;
	struct dirent *dent;
	struct dirent_321 *dent_321;
#define DWORD_ALIGN(d) if((d) % sizeof(long)) (d)+=sizeof(long)-(d)%sizeof(long)

	if (to_getdents_321) {
		/* Provided format is struct dirent and has to be translated
		 * to struct dirent_321 */
		dent_321 = (struct dirent_321 *) dst;
		dent     = (struct dirent *)     src;

		while (consumed < size && dent->d_reclen > 0) {
			dent_321->d_ino = (u32_t) dent->d_ino;
			dent_321->d_off = (i32_t) dent->d_off;
			dent_321->d_reclen = offsetof(struct dirent_321,d_name)+
					     strlen(dent->d_name) + 1;
			DWORD_ALIGN(dent_321->d_reclen);
			strcpy(dent_321->d_name, dent->d_name);
			consumed += dent->d_reclen;
			newconsumed += dent_321->d_reclen;
			dent     = (struct dirent *)     &src[consumed];
			dent_321 = (struct dirent_321 *) &dst[newconsumed];
		}
	} else {
		/* Provided format is struct dirent_321 and has to be
		 * translated to struct dirent */
		dent_321 = (struct dirent_321 *) src;
		dent     = (struct dirent *)     dst;

		while (consumed < size && dent_321->d_reclen > 0) {
			dent->d_ino = (ino_t) dent_321->d_ino;
			dent->d_off = (off_t) dent_321->d_off;
			dent->d_reclen = offsetof(struct dirent, d_name) +
					 strlen(dent_321->d_name) + 1;
			DWORD_ALIGN(dent->d_reclen);
			strcpy(dent->d_name, dent_321->d_name);
			consumed += dent_321->d_reclen;
			newconsumed += dent->d_reclen;
			dent_321 = (struct dirent_321 *) &src[consumed];
			dent     = (struct dirent *)     &dst[newconsumed];
		}
	}

	return newconsumed;
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

  m.m_type		= REQ_LOOKUP;
  m.REQ_GRANT		= grant_id;
  m.REQ_PATH_LEN 	= len;
  m.REQ_PATH_SIZE 	= PATH_MAX + 1;
  m.REQ_DIR_INO 	= (pino_t) dir_ino;
  m.REQ_ROOT_INO 	= (pino_t) root_ino;

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

	m.REQ_GRANT2	= grant_id2;
	m.REQ_UCRED_SIZE= sizeof(credentials);
	flags		|= PATH_GET_UCRED;
  } else {
	/* When there's only one gid, we can send it directly */
	m.REQ_UID	= (pgid_t) uid;
	m.REQ_GID	= (pgid_t) gid;
	flags		&= ~PATH_GET_UCRED;
  }

  m.REQ_FLAGS		= flags;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);
  if(rfp->fp_ngroups > 0) cpf_revoke(grant_id2);

  /* Fill in response according to the return value */
  res->fs_e = m.m_source;

  switch (r) {
  case OK:
	res->inode_nr = (ino_t) m.RES_INODE_NR;
	res->fmode = (mode_t) m.RES_MODE;
	if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
		res->fsize = make64(m.RES_FILE_SIZE_LO, m.RES_FILE_SIZE_HI);
	} else {
		res->fsize = m.RES_FILE_SIZE_LO;
	}
	res->dev = m.RES_DEV;
	res->uid = (uid_t) m.RES_UID;
	res->gid = (gid_t) m.RES_GID;
	break;
  case EENTERMOUNT:
	res->inode_nr = (ino_t) m.RES_INODE_NR;
	res->char_processed = m.RES_OFFSET;
	res->symloop = m.RES_SYMLOOP;
	break;
  case ELEAVEMOUNT:
	res->char_processed = m.RES_OFFSET;
	res->symloop = m.RES_SYMLOOP;
	break;
  case ESYMLINK:
	res->char_processed = m.RES_OFFSET;
	res->symloop = m.RES_SYMLOOP;
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
  if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
	res->fsize = make64(m.RES_FILE_SIZE_LO, m.RES_FILE_SIZE_HI);
  } else {
	res->fsize = m.RES_FILE_SIZE_LO;
  }
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
 *				req_rdlink	     			     *
 *===========================================================================*/
int req_rdlink(fs_e, inode_nr, proc_e, buf, len, direct)
endpoint_t fs_e;
ino_t inode_nr;
endpoint_t proc_e;
vir_bytes buf;
size_t len;
int direct; /* set to 1 to use direct grants instead of magic grants */
{
  message m;
  int r;
  cp_grant_id_t grant_id;

  if (direct) {
	grant_id = cpf_grant_direct(fs_e, buf, len, CPF_WRITE);
  } else {
	grant_id = cpf_grant_magic(fs_e, proc_e, buf, len, CPF_WRITE);
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
 *				req_readsuper	                  	     *
 *===========================================================================*/
int req_readsuper(
  struct vmnt *vmp,
  char *label,
  dev_t dev,
  int readonly,
  int isroot,
  struct node_details *res
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
  m.REQ_FLAGS = 0;
  m.REQ_PROTO = 0;
  VFS_FS_PROTO_PUT_VERSION(m.REQ_PROTO, VFS_FS_CURRENT_VERSION);
  m.REQ_FLAGS |= REQ_HASPROTO;
  if(readonly) m.REQ_FLAGS |= REQ_RDONLY;
  if(isroot)   m.REQ_FLAGS |= REQ_ISROOT;
  m.REQ_GRANT = grant_id;
  m.REQ_DEV = dev;
  m.REQ_PATH_LEN = len;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  if(r == OK) {
	/* Fill in response structure */
	res->fs_e = m.m_source;
	res->inode_nr = (ino_t) m.RES_INODE_NR;
	vmp->m_proto = m.RES_PROTO;
	res->fmode = (mode_t) m.RES_MODE;
	if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
		res->fsize = make64(m.RES_FILE_SIZE_LO, m.RES_FILE_SIZE_HI);
	} else {
		res->fsize = m.RES_FILE_SIZE_LO;
	}
	res->uid = (uid_t) m.RES_UID;
	res->gid = (gid_t) m.RES_GID;
  }

  return(r);
}


/*===========================================================================*
 *				req_readwrite				     *
 *===========================================================================*/
int req_readwrite(
endpoint_t fs_e,
ino_t inode_nr,
off_t pos,
int rw_flag,
endpoint_t user_e,
vir_bytes user_addr,
unsigned int num_of_bytes,
off_t *new_posp,
unsigned int *cum_iop)
{
  struct vmnt *vmp;
  int r;
  cp_grant_id_t grant_id;
  message m;

  vmp = find_vmnt(fs_e);

  grant_id = cpf_grant_magic(fs_e, user_e, user_addr, num_of_bytes,
			     (rw_flag==READING ? CPF_WRITE:CPF_READ));
  if (grant_id == -1)
	  panic("req_readwrite: cpf_grant_magic failed");

  /* Fill in request message */
  m.m_type = rw_flag == READING ? REQ_READ : REQ_WRITE;
  m.REQ_INODE_NR = (pino_t) inode_nr;
  m.REQ_GRANT = grant_id;
  m.REQ_SEEK_POS_LO = ex64lo(pos);
  if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
	m.REQ_SEEK_POS_HI = ex64hi(pos);
  } else if (pos > INT_MAX) {
	return EINVAL;
  } else {
	m.REQ_SEEK_POS_HI = 0;
  }
  m.REQ_NBYTES = num_of_bytes;

  /* Send/rec request */
  r = fs_sendrec(fs_e, &m);
  cpf_revoke(grant_id);

  if (r == OK) {
	/* Fill in response structure */
	if (VFS_FS_PROTO_BIGOFFT(vmp->m_proto)) {
		*new_posp = make64(m.RES_SEEK_POS_LO, m.RES_SEEK_POS_HI);
	} else {
		*new_posp = m.RES_SEEK_POS_LO;
	}
	*cum_iop = m.RES_NBYTES;
  }

  return(r);
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
  m.REQ_SEEK_POS_LO = ex64lo(pos);
  m.REQ_SEEK_POS_HI = 0;	/* Not used for now, so clear it. */
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
  size_t len;
  cp_grant_id_t gid_name, gid_buf;
  message m;

  len = strlen(lastc) + 1;
  gid_name = cpf_grant_direct(fs_e, (vir_bytes) lastc, len, CPF_READ);
  if (gid_name == GRANT_INVALID)
	  panic("req_slink: cpf_grant_direct failed");

  gid_buf = cpf_grant_magic(fs_e, proc_e, path_addr, path_length, CPF_READ);
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
 *				req_stat	       			     *
 *===========================================================================*/
int req_stat(endpoint_t fs_e, ino_t inode_nr, endpoint_t proc_e, vir_bytes buf)
{
  cp_grant_id_t grant_id;
  int r;
  message m;

  /* Grant FS access to copy straight into user provided buffer */
  grant_id = cpf_grant_magic(fs_e, proc_e, buf, sizeof(struct stat), CPF_WRITE);

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
