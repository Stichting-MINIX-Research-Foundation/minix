
/* This file contains the wrapper functions for issueing a request
 * and receiving response from FS processes.
 * Each function builds a request message according to the request
 * parameter, calls the most low-level fs_sendrec and copies
 * back the response.
 * The low-level fs_sendrec handles the recovery mechanism from
 * a dead driver and reissues the request.
 *
 *  Sep 2006 (Balazs Gerofi)
 */

#include "fs.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <minix/u64.h>
#include <unistd.h>

#include <minix/vfsif.h>
#include "fproc.h"
#include "vmnt.h"
#include "vnode.h"
#include "param.h"

FORWARD _PROTOTYPE(int fs_sendrec_f, (char *file, int line, endpoint_t fs_e, message *reqm));

#define fs_sendrec(e, m) fs_sendrec_f(__FILE__, __LINE__, (e), (m))

/*===========================================================================*
 *				req_getnode				     *
 *===========================================================================*/
PUBLIC int req_getnode_f(file, line, req, res)
char *file;
int line;
node_req_t *req; 
node_details_t *res;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_GETNODE;
    m.REQ_INODE_NR = req->inode_nr;

    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;
    res->dev = m.RES_DEV;
    res->uid = m.RES_UID;
    res->gid = m.RES_GID;

    return OK;
}


/*===========================================================================*
 *				req_putnode				     *
 *===========================================================================*/
PUBLIC int req_putnode(fs_e, inode_nr, count)
int fs_e;
ino_t inode_nr;
int count;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_PUTNODE;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_COUNT = count;

    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}

/*===========================================================================*
 *				req_open    				     *
 *===========================================================================*/
int req_open(req, res)
open_req_t *req; 
node_details_t *res; 
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_OPEN;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_FLAGS = req->oflags;
    m.REQ_MODE = req->omode;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_PATH = req->lastc;
    m.REQ_PATH_LEN = strlen(req->lastc) + 1;

    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;
    res->dev = m.RES_DEV;
    res->inode_index = m.RES_INODE_INDEX;
    /* For exec */
    res->uid = m.RES_UID;
    res->gid = m.RES_GID;
    res->ctime = m.RES_CTIME;

    return OK;
}


/*===========================================================================*
 *				req_create    				     *
 *===========================================================================*/
int req_create(fs_e, inode_nr, omode, uid, gid, path, res)
int fs_e;
ino_t inode_nr;
int omode;
uid_t uid;
gid_t gid;
char *path;
node_details_t *res; 
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_CREATE;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_MODE = omode;
    m.REQ_UID = uid;
    m.REQ_GID = gid;
    m.REQ_PATH = path;
    m.REQ_PATH_LEN = strlen(path) + 1;

    /* Send/rec request */
    if ((r = fs_sendrec(fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;
    res->dev = m.RES_DEV;
    res->inode_index = m.RES_INODE_INDEX;
    /* For exec */
    res->uid = m.RES_UID;
    res->gid = m.RES_GID;
    res->ctime = m.RES_CTIME;

    return OK;
}


/*===========================================================================*
 *				req_readwrite  				     *
 *===========================================================================*/
int req_readwrite(req, res)
readwrite_req_t *req; 
readwrite_res_t *res; 
{
    int r;
    message m;

    if (ex64hi(req->pos) != 0)
	panic(__FILE__, "req_readwrite: pos too large", NO_NUM);
    
    /* Fill in request message */
    m.m_type = req->rw_flag == READING ? REQ_READ : REQ_WRITE;
    m.REQ_FD_INODE_NR = req->inode_nr;
    m.REQ_FD_WHO_E = req->user_e;
    m.REQ_FD_SEG = req->seg;
    m.REQ_FD_POS = ex64lo(req->pos);
    m.REQ_FD_NBYTES = req->num_of_bytes;
    m.REQ_FD_USER_ADDR = req->user_addr;
    m.REQ_FD_INODE_INDEX = req->inode_index;
    
    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->new_pos = cvul64(m.RES_FD_POS);
    res->cum_io = m.RES_FD_CUM_IO;

    return OK;
}



/*===========================================================================*
 *				req_pipe   				     *
 *===========================================================================*/
PUBLIC int req_pipe(req, res)
pipe_req_t *req; 
node_details_t *res;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_PIPE;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;

    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;
    res->dev = m.RES_DEV;
    res->inode_index = m.RES_INODE_INDEX;
    
    return OK;
}


/*===========================================================================*
 *				req_clone_opcl  			     *
 *===========================================================================*/
PUBLIC int req_clone_opcl(req, res)
clone_opcl_req_t *req;
node_details_t *res;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_CLONE_OPCL;
    m.REQ_DEV = req->dev;

    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;
    
    /* Fill in response structure */
    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;
    res->dev = m.RES_DEV;
    res->inode_index = m.RES_INODE_INDEX;
    
    return OK;
}



/*===========================================================================*
 *				req_ftrunc          			     *
 *===========================================================================*/
PUBLIC int req_ftrunc(req)
ftrunc_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_FTRUNC;
    m.REQ_FD_INODE_NR = req->inode_nr;
    m.REQ_FD_START = req->start;
    m.REQ_FD_END = req->end;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_chmod          			     *
 *===========================================================================*/
PUBLIC int req_chmod(req, ch_mode)
chmod_req_t *req;
int *ch_mode;
{
    message m;
    int r;

    /* Fill in request message */
    m.m_type = REQ_CHMOD;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_MODE = req->rmode;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;

    /* Send/rec request */
    r = fs_sendrec(req->fs_e, &m);

    /* Copy back actual mode. */
    if(ch_mode) *ch_mode = m.RES_MODE;

    return r;
}


/*===========================================================================*
 *				req_chown          			     *
 *===========================================================================*/
PUBLIC int req_chown(req, ch_mode)
chown_req_t *req;
int *ch_mode;
{
    message m;
    int r;

    /* Fill in request message */
    m.m_type = REQ_CHOWN;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_NEW_UID = req->newuid;
    m.REQ_NEW_GID = req->newgid;

    /* Send/rec request */
    r = fs_sendrec(req->fs_e, &m);

    /* Return new mode to caller. */
    if(ch_mode) *ch_mode = m.RES_MODE;

    return r;
}


/*===========================================================================*
 *				req_access         			     *
 *===========================================================================*/
PUBLIC int req_access(req)
access_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_ACCESS;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_MODE = req->amode;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_mknod          			     *
 *===========================================================================*/
PUBLIC int req_mknod(req)
mknod_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_MKNOD;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_MODE = req->rmode;
    m.REQ_DEV = req->dev;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_PATH = req->lastc;
    m.REQ_PATH_LEN = strlen(req->lastc) + 1;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_mkdir          			     *
 *===========================================================================*/
PUBLIC int req_mkdir(req)
mkdir_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_MKDIR;
    m.REQ_INODE_NR = req->d_inode_nr;
    m.REQ_MODE = req->rmode;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_PATH = req->lastc;
    m.REQ_PATH_LEN = strlen(req->lastc) + 1;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}



/*===========================================================================*
 *				req_inhibread         			     *
 *===========================================================================*/
PUBLIC int req_inhibread(req)
node_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_INHIBREAD;
    m.REQ_INODE_NR = req->inode_nr;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_stat          			     *
 *===========================================================================*/
PUBLIC int req_stat(fs_e, inode_nr, who_e, buf, pos)
int fs_e;
ino_t inode_nr;
int who_e;
char *buf;
int pos;
{
  cp_grant_id_t gid;
  int r;
  message m;
  struct stat sb;

  if (pos != 0)
  {
	gid= cpf_grant_direct(fs_e, (vir_bytes)&sb, sizeof(struct stat),
		CPF_WRITE);
  }
  else
  {
	gid= cpf_grant_magic(fs_e, who_e, (vir_bytes)buf, sizeof(struct stat),
		CPF_WRITE);
  }
  if (gid < 0)
	return gid;

  /* Fill in request message */
  m.m_type = REQ_STAT;
  m.REQ_INODE_NR = inode_nr;
  m.REQ_GRANT = gid;

  /* Send/rec request */
  r= fs_sendrec(fs_e, &m);

  cpf_revoke(gid);

  if (r == OK && pos != 0)
  {
	sb.st_size -= pos;
	r= sys_vircopy(SELF, D, (vir_bytes)&sb, who_e, D, (vir_bytes)buf, 
		sizeof(struct stat));
  }

  return r;
}


/*===========================================================================*
 *				req_fstatfs        			     *
 *===========================================================================*/
PUBLIC int req_fstatfs(fs_e, inode_nr, who_e, buf)
int fs_e;
ino_t inode_nr;
int who_e;
char *buf;
{
  int r;
  cp_grant_id_t gid;
  message m;

  gid= cpf_grant_magic(fs_e, who_e, (vir_bytes)buf, sizeof(struct statfs),
		CPF_WRITE);
  if (gid < 0)
	return gid;

  /* Fill in request message */
  m.m_type = REQ_FSTATFS;
  m.REQ_INODE_NR = inode_nr;
  m.REQ_GRANT = gid;

  /* Send/rec request */
  r= fs_sendrec(fs_e, &m);

  cpf_revoke(gid);

  return r;
}


/*===========================================================================*
 *				req_unlink        			     *
 *===========================================================================*/
PUBLIC int req_unlink(req)
unlink_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_UNLINK;
    m.REQ_INODE_NR = req->d_inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_PATH = req->lastc;
    m.REQ_PATH_LEN = strlen(req->lastc) + 1;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_rmdir        			     *
 *===========================================================================*/
PUBLIC int req_rmdir(req)
unlink_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_RMDIR;
    m.REQ_INODE_NR = req->d_inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_PATH = req->lastc;
    m.REQ_PATH_LEN = strlen(req->lastc) + 1;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_utime        			     *
 *===========================================================================*/
PUBLIC int req_utime(req)
utime_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_UTIME;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_ACTIME = req->actime;
    m.REQ_MODTIME = req->modtime;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_stime        			     *
 *===========================================================================*/
PUBLIC int req_stime(fs_e, boottime)
endpoint_t fs_e; 
time_t boottime;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_STIME;
    m.REQ_BOOTTIME = boottime;
    
    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_sync         			     *
 *===========================================================================*/
PUBLIC int req_sync(fs_e)
endpoint_t fs_e; 
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_SYNC;
    
    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_link        			     *
 *===========================================================================*/
PUBLIC int req_link(req)
link_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_LINK;
    m.REQ_LINKED_FILE = req->linked_file;
    m.REQ_LINK_PARENT = req->link_parent;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_PATH = req->lastc;
    m.REQ_PATH_LEN = strlen(req->lastc) + 1;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}
    

/*===========================================================================*
 *				req_slink        			     *
 *===========================================================================*/
PUBLIC int req_slink(req)
slink_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_SLINK;
    m.REQ_INODE_NR = req->parent_dir;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_PATH = req->lastc;
    m.REQ_PATH_LEN = strlen(req->lastc) + 1;
    m.REQ_WHO_E = req->who_e;
    m.REQ_USER_ADDR = req->path_addr;
    m.REQ_SLENGTH = req->path_length;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_rdlink       			     *
 *===========================================================================*/
PUBLIC int req_rdlink(req)
rdlink_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_RDLINK;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_WHO_E = req->who_e;
    m.REQ_USER_ADDR = req->path_buffer;
    m.REQ_SLENGTH = req->max_length;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_rename       			     *
 *===========================================================================*/
PUBLIC int req_rename(req)
rename_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_RENAME;
    m.REQ_OLD_DIR = req->old_dir;
    m.REQ_NEW_DIR = req->new_dir;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_PATH = req->old_name;
    m.REQ_PATH_LEN = strlen(req->old_name) + 1;
    m.REQ_USER_ADDR = req->new_name;
    m.REQ_SLENGTH = strlen(req->new_name) + 1;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_mountpoint                  	     *
 *===========================================================================*/
PUBLIC int req_mountpoint(req, res)
mountpoint_req_t *req; 
node_details_t *res;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_MOUNTPOINT;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;

    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;

    return OK;
}


/*===========================================================================*
 *				req_readsuper                   	     *
 *===========================================================================*/
PUBLIC int req_readsuper(req, res)
readsuper_req_t *req; 
readsuper_res_t *res;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_READSUPER;
    m.REQ_READONLY = req->readonly;
    m.REQ_BOOTTIME = req->boottime;
    m.REQ_DRIVER_E = req->driver_e;
    m.REQ_DEV = req->dev;
    m.REQ_SLINK_STORAGE = req->slink_storage;
    m.REQ_ISROOT = req->isroot;

    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;
    res->blocksize = m.RES_BLOCKSIZE;
    res->maxsize = m.RES_MAXSIZE;

    return OK;
}


/*===========================================================================*
 *				req_unmount      			     *
 *===========================================================================*/
PUBLIC int req_unmount(fs_e)
endpoint_t fs_e; 
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_UNMOUNT;
    
    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_trunc          			     *
 *===========================================================================*/
PUBLIC int req_trunc(req)
trunc_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_TRUNC;
    m.REQ_FD_INODE_NR = req->inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_LENGTH = req->length;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}



/*===========================================================================*
 *				req_newdriver          			     *
 *===========================================================================*/
PUBLIC int req_newdriver(fs_e, dev, driver_e)
endpoint_t fs_e;
Dev_t dev;
endpoint_t driver_e;
{
/* Note: this is the only request function that doesn't use the 
 * fs_sendrec internal routine, since we want to avoid the dead
 * driver recovery mechanism here. This function is actually called 
 * during the recovery.
 */
    message m;
    int r;

    /* Fill in request message */
    m.m_type = REQ_NEW_DRIVER;
    m.REQ_DEV = dev;
    m.REQ_DRIVER_E = driver_e;

    /* Issue request */
    if ((r = sendrec(fs_e, &m)) != OK) {
        printf("VFSreq_newdriver: error sending message to %d: %d\n", fs_e, r);
        return r;
    }

    return OK;
}


/*===========================================================================*
 *				req_lookup                      	     *
 *===========================================================================*/
PUBLIC int req_lookup(req, res)
lookup_req_t *req; 
lookup_res_t *res;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_LOOKUP;
    m.REQ_PATH = req->path;
    m.REQ_PATH_LEN = strlen(req->path) + 1;
    m.REQ_USER_ADDR = req->lastc;
    m.REQ_FLAGS = req->flags;
    
    m.REQ_INODE_NR = req->start_dir;
    m.REQ_CHROOT_NR = req->root_dir;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_SYMLOOP = req->symloop;

    /* Send/rec request */
    r = fs_sendrec(req->fs_e, &m);

    /* Fill in response according to the return value */
    res->fs_e = m.m_source;
    switch (r) {
        case OK:
	default:
            res->inode_nr = m.RES_INODE_NR;
            res->fmode = m.RES_MODE;
            res->fsize = m.RES_FILE_SIZE;
            res->dev = m.RES_DEV;
	    res->uid= m.RES_UID;
	    res->gid= m.RES_GID;
            res->char_processed = m.RES_OFFSET;		/* For ENOENT */
            break;
        case EENTERMOUNT:
            res->inode_nr = m.RES_INODE_NR;
        case ELEAVEMOUNT:
        case ESYMLINK:
            res->char_processed = m.RES_OFFSET;
            res->symloop = m.RES_SYMLOOP;
            break;
    }

    return r;
}


/*===========================================================================*
 *			req_breadwrite  				     *
 *===========================================================================*/
int req_breadwrite(req, res)
breadwrite_req_t *req; 
readwrite_res_t *res; 
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = req->rw_flag == READING ? REQ_BREAD : REQ_BWRITE;
    m.REQ_XFD_BDEV = req->dev;
    m.REQ_XFD_BLOCK_SIZE = req->blocksize;
    m.REQ_XFD_WHO_E = req->user_e;
    m.REQ_XFD_POS_LO = ex64lo(req->pos);
    m.REQ_XFD_POS_HI = ex64hi(req->pos);
    m.REQ_XFD_NBYTES = req->num_of_bytes;
    m.REQ_XFD_USER_ADDR = req->user_addr;
    
    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->new_pos = make64(m.RES_XFD_POS_LO, m.RES_XFD_POS_HI);
    res->cum_io = m.RES_XFD_CUM_IO;

    return OK;
}


PUBLIC int req_getdents(fs_e, inode_nr, pos, gid, size, pos_change)
endpoint_t fs_e;
ino_t inode_nr;
off_t pos;
cp_grant_id_t gid;
size_t size;
off_t *pos_change;
{
	int r;
	message m;

	m.m_type= REQ_GETDENTS;
	m.REQ_GDE_INODE= inode_nr;
	m.REQ_GDE_GRANT= gid;
	m.REQ_GDE_SIZE= size;
	m.REQ_GDE_POS= pos;

	r = fs_sendrec(fs_e, &m);
	*pos_change= m.RES_GDE_POS_CHANGE;
	return r;
}


/*===========================================================================*
 *				req_flush         			     *
 *===========================================================================*/
PUBLIC int req_flush(fs_e, dev)
endpoint_t fs_e; 
dev_t dev;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_FLUSH;
    m.REQ_DEV = dev;
    
    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}


#if 0           
/*                 Wrapper pattern:                                          */
/*===========================================================================*
 *				req_          			     *
 *===========================================================================*/
PUBLIC int req_(req, res)
_req_t *req;
_t *res;
{
    int r;
    message m;

    /* Fill in request message */


    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;
    
    /* Fill in response structure */


    return OK;
}
#endif 




/*===========================================================================*
 *				fs_sendrec				     *
 *===========================================================================*/
PRIVATE int fs_sendrec_f(char *file, int line, endpoint_t fs_e, message *reqm)
{
/* This is the low level function that sends requests to FS processes.
 * It also handles driver recovery mechanism and reissuing the
 * request which failed due to a dead driver.
 */
  int r, old_driver_e, new_driver_e;
  message origm, m;
  struct vmnt *vmp;

  /* Make a copy of the request so that we can load it back in
   * case of a dead driver */
  origm = *reqm;
  
  for (;;) {
      /* Do the actual send, receive */
      if (OK != (r=sendrec(fs_e, reqm))) {
          printf("VFS:fs_sendrec:%s:%d: error sending message. FS_e: %d req_nr: %d err: %d\n", 
                  file, line, fs_e, reqm->m_type, r);
      }

      if(r == OK) {
      	/* Sendrec was okay */
      	break;
      }

      /* Dead driver */
      if (r == EDEADSRCDST || r == EDSTDIED || r == ESRCDIED) {
          old_driver_e = NONE;
          /* Find old driver by endpoint */
          for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
              if (vmp->m_fs_e == fs_e) {   /* found FS */
                  old_driver_e = vmp->m_driver_e;
                  dmap_unmap_by_endpt(old_driver_e); /* unmap driver */
                  break;
              }
          }
         
          /* No FS ?? */
          if (old_driver_e == NONE)
              panic(__FILE__, "VFSdead_driver: couldn't find FS\n", fs_e);

          /* Wait for a new driver. */
          for (;;) {
              new_driver_e = 0;
              printf("VFSdead_driver: waiting for new driver\n");
              r = receive(RS_PROC_NR, &m);
              if (r != OK) {
                  panic(__FILE__, "VFSdead_driver: unable to receive from RS", 
				  r);
              }
              if (m.m_type == DEVCTL) {
                  /* Map new driver */
                  r = fs_devctl(m.ctl_req, m.dev_nr, m.driver_nr,
                          m.dev_style, m.m_force);
                  if (m.ctl_req == DEV_MAP && r == OK) {
                      new_driver_e = m.driver_nr;
                      printf("VFSdead_driver: new driver endpoint: %d\n",
                              new_driver_e);
                  }
              }
              else {
                  panic(__FILE__, "VFSdead_driver: got message from RS, type", 
                          m.m_type);
              }
              m.m_type = r;
              if ((r = send(RS_PROC_NR, &m)) != OK) {
                  panic(__FILE__, "VFSdead_driver: unable to send to RS",
                          r);
              }
              /* New driver is ready */
              if (new_driver_e) break;
          }
          
          /* Copy back original request */
          *reqm = origm;  
          continue;
      }

       printf("fs_sendrec: unhandled error %d sending to %d\n", r, fs_e);
       panic(__FILE__, "fs_sendrec: unhandled error", NO_NUM);
  }

  /* Return message type */
  return reqm->m_type;
}


