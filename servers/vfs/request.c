
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
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <unistd.h>

#include <minix/vfsif.h>
#include "fproc.h"
#include "vmnt.h"
#include "vnode.h"
#include "param.h"


/*===========================================================================*
 *				req_getnode				     *
 *===========================================================================*/
PUBLIC int req_getnode(req, res)
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

    return OK;
}


/*===========================================================================*
 *				req_putnode				     *
 *===========================================================================*/
PUBLIC int req_putnode(req)
node_req_t *req; 
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_PUTNODE;
    m.REQ_INODE_NR = req->inode_nr;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
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
 *				req_readwrite  				     *
 *===========================================================================*/
int req_readwrite(req, res)
readwrite_req_t *req; 
readwrite_res_t *res; 
{
    int r;
    message m;
    
    /* Fill in request message */
    m.m_type = req->rw_flag == READING ? REQ_READ : REQ_WRITE;
    m.REQ_FD_INODE_NR = req->inode_nr;
    m.REQ_FD_WHO_E = req->user_e;
    m.REQ_FD_SEG = req->seg;
    m.REQ_FD_POS = req->pos;
    m.REQ_FD_NBYTES = req->num_of_bytes;
    m.REQ_FD_USER_ADDR = req->user_addr;
    m.REQ_FD_INODE_INDEX = req->inode_index;
    
    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->new_pos = m.RES_FD_POS;
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
PUBLIC int req_chmod(req)
chmod_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_CHMOD;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_MODE = req->rmode;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_chown          			     *
 *===========================================================================*/
PUBLIC int req_chown(req)
chown_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_CHOWN;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_NEW_UID = req->newuid;
    m.REQ_NEW_GID = req->newgid;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
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
PUBLIC int req_stat(req)
stat_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_STAT;
    m.REQ_INODE_NR = req->inode_nr;
    m.REQ_UID = req->uid;
    m.REQ_GID = req->gid;
    m.REQ_WHO_E = req->who_e;
    m.REQ_USER_ADDR = req->buf;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_fstat          			     *
 *===========================================================================*/
PUBLIC int req_fstat(req)
stat_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_FSTAT;
    m.REQ_FD_INODE_NR = req->inode_nr;
    m.REQ_FD_WHO_E = req->who_e;
    m.REQ_FD_USER_ADDR = req->buf;
    m.REQ_FD_POS = req->pos;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
}


/*===========================================================================*
 *				req_fstatfs        			     *
 *===========================================================================*/
PUBLIC int req_fstatfs(req)
stat_req_t *req;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_FSTATFS;
    m.REQ_FD_INODE_NR = req->inode_nr;
    m.REQ_FD_WHO_E = req->who_e;
    m.REQ_FD_USER_ADDR = req->buf;

    /* Send/rec request */
    return fs_sendrec(req->fs_e, &m);
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
 *				req_getdir 				     *
 *===========================================================================*/
PUBLIC int req_getdir(req, res)
getdir_req_t *req; 
node_details_t *res;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_GETDIR;
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
        printf("VFSreq_newdriver: error sending message to %d\n", fs_e);
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
            res->inode_nr = m.RES_INODE_NR;
            res->fmode = m.RES_MODE;
            res->fsize = m.RES_FILE_SIZE;
            res->dev = m.RES_DEV;
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
    m.REQ_FD_BDEV = req->dev;
    m.REQ_FD_BLOCK_SIZE = req->blocksize;
    m.REQ_FD_WHO_E = req->user_e;
    m.REQ_FD_POS = req->pos;
    m.REQ_FD_NBYTES = req->num_of_bytes;
    m.REQ_FD_USER_ADDR = req->user_addr;
    
    /* Send/rec request */
    if ((r = fs_sendrec(req->fs_e, &m)) != OK) return r;

    /* Fill in response structure */
    res->new_pos = m.RES_FD_POS;
    res->cum_io = m.RES_FD_CUM_IO;

    return OK;
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
PUBLIC int fs_sendrec(endpoint_t fs_e, message *reqm)
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
      if (OK != sendrec(fs_e, reqm)) {
          printf("VFS: error sending message. FS_e: %d req_nr: %d\n", 
                  fs_e, reqm->m_type);
      }

      /* Get response type */
      r = reqm->m_type;

      /* Dead driver */
      if (r == EDEADSRCDST || r == EDSTDIED || r == ESRCDIED) {
          old_driver_e = 0;
          /* Find old driver enpoint */
          for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
              if (vmp->m_fs_e == reqm->m_source) {   /* found FS */
                  old_driver_e = vmp->m_driver_e;
                  dmap_unmap_by_endpt(old_driver_e); /* unmap driver */
                  break;
              }
          }
         
          /* No FS ?? */
          if (!old_driver_e) {
              panic(__FILE__, "VFSdead_driver: couldn't find FS\n", 
			      old_driver_e);
          }

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
      
      /* Sendrec was okay */
      break;
  }
  /* Return message type */
  return r;
}



