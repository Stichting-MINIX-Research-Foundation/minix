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
 *			req_breadwrite					     *
 *===========================================================================*/
PUBLIC int req_breadwrite(fs_e, user_e, dev, pos, num_of_bytes, user_addr,
	rw_flag, new_posp, cum_iop)
endpoint_t fs_e;
endpoint_t user_e;
dev_t dev;
u64_t pos;
unsigned int num_of_bytes;
char *user_addr;
int rw_flag;
u64_t *new_posp;
unsigned int *cum_iop;
{
    int r;
    cp_grant_id_t gid;
    message m;

    gid= cpf_grant_magic(fs_e, user_e, (vir_bytes)user_addr,
	num_of_bytes, (rw_flag == READING ? CPF_WRITE : CPF_READ));
    if (gid == -1)
	panic(__FILE__, "req_breadwrite: cpf_grant_magic failed", NO_NUM);

    /* Fill in request message */
    m.m_type = rw_flag == READING ? REQ_BREAD_S : REQ_BWRITE_S;
    m.REQ_XFD_BDEV = dev;
    m.REQ_XFD_GID = gid;
    m.REQ_XFD_POS_LO = ex64lo(pos);
    m.REQ_XFD_POS_HI = ex64hi(pos);
    m.REQ_XFD_NBYTES = num_of_bytes;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(gid);

    if (r != OK) return r;

    /* Fill in response structure */
    *new_posp = make64(m.RES_XFD_POS_LO, m.RES_XFD_POS_HI);
    *cum_iop = m.RES_XFD_CUM_IO;

    return OK;
}


/*===========================================================================*
 *				req_chmod	      			     *
 *===========================================================================*/
PUBLIC int req_chmod(fs_e, inode_nr, rmode, new_modep)
int fs_e;
ino_t inode_nr;
mode_t rmode;
mode_t *new_modep;
{
    message m;
    int r;

    /* Fill in request message */
    m.m_type = REQ_CHMOD;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_MODE = rmode;
    m.REQ_UID = fp->fp_effuid;
    m.REQ_GID = fp->fp_effgid;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    /* Copy back actual mode. */
    *new_modep = m.RES_MODE;

    return r;
}


/*===========================================================================*
 *				req_chown          			     *
 *===========================================================================*/
PUBLIC int req_chown(fs_e, inode_nr, newuid, newgid, new_modep)
endpoint_t fs_e;
ino_t inode_nr;
uid_t newuid;
gid_t newgid;
mode_t *new_modep;
{
    message m;
    int r;

    /* Fill in request message */
    m.m_type = REQ_CHOWN;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_UID = fp->fp_effuid;
    m.REQ_GID = fp->fp_effgid;
    m.REQ_NEW_UID = newuid;
    m.REQ_NEW_GID = newgid;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    /* Return new mode to caller. */
    *new_modep = m.RES_MODE;

    return r;
}


/*===========================================================================*
 *				req_create				     *
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
    cp_grant_id_t grant_id;
    size_t len;
    message m;

    len= strlen(path) + 1;
    grant_id= cpf_grant_direct(fs_e, (vir_bytes)path, len, CPF_READ);
    if (grant_id == -1)
	panic(__FILE__, "req_create: cpf_grant_direct failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_CREATE_S;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_MODE = omode;
    m.REQ_UID = uid;
    m.REQ_GID = gid;
    m.REQ_GRANT = grant_id;
    m.REQ_PATH_LEN = len;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(grant_id);

    if (r != OK) return r;

    /* Fill in response structure */
    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;
    res->uid = m.RES_UID;
    res->gid = m.RES_GID;
    res->dev = m.RES_DEV;
    res->inode_index = m.RES_INODE_INDEX;

    return OK;
}


/*===========================================================================*
 *				req_flush	      			     *
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


/*===========================================================================*
 *				req_fstatfs	    			     *
 *===========================================================================*/
PUBLIC int req_fstatfs(fs_e, who_e, buf)
int fs_e;
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
  m.REQ_GRANT = gid;

  /* Send/rec request */
  r= fs_sendrec(fs_e, &m);

  cpf_revoke(gid);

  return r;
}


/*===========================================================================*
 *				req_ftrunc	     			     *
 *===========================================================================*/
PUBLIC int req_ftrunc(fs_e, inode_nr, start, end)
endpoint_t fs_e;
ino_t inode_nr;
off_t start;
off_t end;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_FTRUNC;
    m.REQ_FD_INODE_NR = inode_nr;
    m.REQ_FD_START = start;
    m.REQ_FD_END = end;

    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_getdents	     			     *
 *===========================================================================*/
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
 *				req_inhibread	  			     *
 *===========================================================================*/
PUBLIC int req_inhibread(fs_e, inode_nr)
endpoint_t fs_e;
ino_t inode_nr;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_INHIBREAD;
    m.REQ_INODE_NR = inode_nr;

    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_link	       			     *
 *===========================================================================*/
PUBLIC int req_link(fs_e, link_parent, lastc, linked_file)
endpoint_t fs_e;
ino_t link_parent;
char *lastc;
ino_t linked_file;
{
    int r;
    cp_grant_id_t gid;
    size_t len;
    message m;

    len= strlen(lastc) + 1;
    gid= cpf_grant_direct(fs_e, (vir_bytes)lastc, len, CPF_READ);
    if (gid == -1)
	panic(__FILE__, "req_link: cpf_grant_direct failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_LINK_S;
    m.REQ_LINKED_FILE = linked_file;
    m.REQ_LINK_PARENT = link_parent;
    m.REQ_GRANT = gid;
    m.REQ_PATH_LEN = len;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(gid);

    return r;
}
    

/*===========================================================================*
 *				req_lookup	                   	     *
 *===========================================================================*/
PUBLIC int req_lookup(fs_e, path_off, dir_ino, root_ino, uid, gid, flags, res)
endpoint_t fs_e;
size_t path_off;
ino_t dir_ino;
ino_t root_ino;
uid_t uid;
gid_t gid;
int flags;
lookup_res_t *res;
{
    int r;
    size_t len;
    cp_grant_id_t grant_id;
    message m;

#if 0
    printf("req_lookup_s: fs %d, ino %d, root %d, string (off %d) '%s'\n",
	fs_e, dir_ino, root_ino, path_off, user_fullpath+path_off);
#endif

    grant_id= cpf_grant_direct(fs_e, (vir_bytes)user_fullpath,
	sizeof(user_fullpath), CPF_READ|CPF_WRITE);
    if (grant_id == -1)
	panic(__FILE__, "req_lookup_s: cpf_grant_direct failed", NO_NUM);
    len= strlen(user_fullpath+path_off) + 1;

    /* Fill in request message */
    m.m_type = REQ_LOOKUP_S;
    m.REQ_L_GRANT = grant_id;
    m.REQ_L_PATH_LEN = len;
    m.REQ_L_PATH_SIZE = sizeof(user_fullpath);
    m.REQ_L_PATH_OFF = path_off;
    m.REQ_L_DIR_INO = dir_ino;
    m.REQ_L_ROOT_INO = root_ino;
    m.REQ_L_FLAGS = flags;
    m.REQ_L_UID = uid;
    m.REQ_L_GID = gid;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(grant_id);

    /* Fill in response according to the return value */
    res->fs_e = m.m_source;
    switch (r) {
        case OK:
            res->inode_nr = m.RES_INODE_NR;
            res->fmode = m.RES_MODE;
            res->fsize = m.RES_FILE_SIZE;
            res->dev = m.RES_DEV;
	    res->uid= m.RES_UID;
	    res->gid= m.RES_GID;
            break;
        case EENTERMOUNT:
            res->inode_nr = m.RES_INODE_NR;
            res->char_processed = m.RES_OFFSET;
	    res->symloop = m.RES_SYMLOOP2;
	    break;
        case ELEAVEMOUNT:
            res->char_processed = m.RES_OFFSET;
	    res->symloop = m.RES_SYMLOOP2;
	    break;
        case ESYMLINK:
            res->char_processed = m.RES_OFFSET;
            res->symloop = m.RES_SYMLOOP2;
            break;
	default:
	    break;
    }

    return r;
}


/*===========================================================================*
 *				req_mkdir	      			     *
 *===========================================================================*/
PUBLIC int req_mkdir(fs_e, inode_nr, lastc, uid, gid, dmode)
endpoint_t fs_e;
ino_t inode_nr;
char *lastc;
uid_t uid;
gid_t gid;
mode_t dmode;
{
    int r;
    cp_grant_id_t grant_id;
    size_t len;
    message m;

    len= strlen(lastc) + 1;
    grant_id= cpf_grant_direct(fs_e, (vir_bytes)lastc, len, CPF_READ);
    if (grant_id == -1)
	panic(__FILE__, "req_mkdir: cpf_grant_direct failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_MKDIR_S;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_MODE = dmode;
    m.REQ_UID = uid;
    m.REQ_GID = gid;
    m.REQ_GRANT = grant_id;
    m.REQ_PATH_LEN = len;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(grant_id);

    return r;
}


/*===========================================================================*
 *				req_mknod	      			     *
 *===========================================================================*/
PUBLIC int req_mknod(fs_e, inode_nr, lastc, uid, gid, dmode, dev)
endpoint_t fs_e;
ino_t inode_nr;
char *lastc;
uid_t uid;
gid_t gid;
mode_t dmode;
dev_t dev;
{
    int r;
    size_t len;
    cp_grant_id_t grant_id;
    message m;

    len= strlen(lastc) + 1;
    grant_id= cpf_grant_direct(fs_e, (vir_bytes)lastc, len, CPF_READ);
    if (grant_id == -1)
	panic(__FILE__, "req_mknod: cpf_grant_direct failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_MKNOD_S;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_MODE = dmode;
    m.REQ_DEVx = dev;
    m.REQ_UID = uid;
    m.REQ_GID = gid;
    m.REQ_GRANT = grant_id;
    m.REQ_PATH_LEN = len;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(grant_id);

    return r;
}


/*===========================================================================*
 *				req_mountpoint	                 	     *
 *===========================================================================*/
PUBLIC int req_mountpoint(fs_e, inode_nr)
endpoint_t fs_e;
ino_t inode_nr;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_MOUNTPOINT_S;
    m.REQ_INODE_NR = inode_nr;

    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}


/*===========================================================================*
 *				req_newnode	      			     *
 *===========================================================================*/
PUBLIC int req_newnode(fs_e, uid, gid, dmode, dev, res)
endpoint_t fs_e;
uid_t uid;
gid_t gid;
mode_t dmode;
dev_t dev;
struct node_details *res;
{
    int r;
    message m;

    /* Fill in request message */
    m.m_type = REQ_NEWNODE;
    m.REQ_MODE = dmode;
    m.REQ_DEVx = dev;
    m.REQ_UID = uid;
    m.REQ_GID = gid;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    res->fs_e = m.m_source;
    res->inode_nr = m.RES_INODE_NR;
    res->fmode = m.RES_MODE;
    res->fsize = m.RES_FILE_SIZE;
    res->dev = m.RES_DEV;
    res->uid= m.RES_UID;
    res->gid= m.RES_GID;

    return r;
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
 *				req_rdlink	     			     *
 *===========================================================================*/
PUBLIC int req_rdlink(fs_e, inode_nr, who_e, buf, len)
endpoint_t fs_e;
ino_t inode_nr;
endpoint_t who_e;
vir_bytes buf;
size_t len;
{
    message m;
    int r;
    cp_grant_id_t gid;

    gid= cpf_grant_magic(fs_e, who_e, buf, len, CPF_WRITE);
    if (gid == -1)
	panic(__FILE__, "req_rdlink: cpf_grant_magic failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_RDLINK_S;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_GRANT = gid;
    m.REQ_SLENGTH = len;

    /* Send/rec request */
    r= fs_sendrec(fs_e, &m);

    cpf_revoke(gid);

    return r;
}


/*===========================================================================*
 *				req_readsuper	                  	     *
 *===========================================================================*/
PUBLIC int req_readsuper(fs_e, label, dev, readonly, isroot, res_nodep)
endpoint_t fs_e;
char *label;
dev_t dev;
int readonly;
int isroot;
struct node_details *res_nodep;
{
    int r;
    cp_grant_id_t gid;
    size_t len;
    message m;

    len= strlen(label)+1;
    gid= cpf_grant_direct(fs_e, (vir_bytes)label, len, CPF_READ);
    if (gid == -1)
	panic(__FILE__, "req_req_readsuper: cpf_grant_direct failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_READSUPER_S;
    m.REQ_READONLY = readonly;
    m.REQ_GRANT2 = gid;
    m.REQ_DEV = dev;
    m.REQ_ISROOT = isroot;
    m.REQ_PATH_LEN = len;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(gid);

    if(r != OK) return r;

    /* Fill in response structure */
    res_nodep->fs_e = m.m_source;
    res_nodep->inode_nr = m.RES_INODE_NR;
    res_nodep->fmode = m.RES_MODE;
    res_nodep->fsize = m.RES_FILE_SIZE;
    res_nodep->uid = m.RES_UID;
    res_nodep->gid = m.RES_GID;

    return OK;
}


/*===========================================================================*
 *				req_readwrite				     *
 *===========================================================================*/
PUBLIC int req_readwrite(fs_e, inode_nr, inode_index, pos, rw_flag, user_e,
	user_addr, num_of_bytes, new_posp, cum_iop)
endpoint_t fs_e;
ino_t inode_nr;
unsigned short inode_index;
u64_t pos;
int rw_flag;
endpoint_t user_e;
char *user_addr;
unsigned int num_of_bytes;
u64_t *new_posp;
unsigned int *cum_iop;
{
    int r;
    cp_grant_id_t gid;
    message m;

    if (ex64hi(pos) != 0)
	panic(__FILE__, "req_readwrite: pos too large", NO_NUM);

    gid= cpf_grant_magic(fs_e, user_e, (vir_bytes)user_addr,
	num_of_bytes, (rw_flag == READING ? CPF_WRITE : CPF_READ));
    if (gid == -1)
	panic(__FILE__, "req_readwrite: cpf_grant_magic failed", NO_NUM);

    /* Fill in request message */
    m.m_type = rw_flag == READING ? REQ_READ_S : REQ_WRITE_S;
    m.REQ_FD_INODE_NR = inode_nr;
    m.REQ_FD_GID = gid;
    m.REQ_FD_POS = ex64lo(pos);
    m.REQ_FD_NBYTES = num_of_bytes;
    m.REQ_FD_INODE_INDEX = inode_index;
    
    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(gid);

    if (r != OK) return r;

    /* Fill in response structure */
    *new_posp = cvul64(m.RES_FD_POS);
    *cum_iop = m.RES_FD_CUM_IO;

    return OK;
}


/*===========================================================================*
 *				req_rename	     			     *
 *===========================================================================*/
PUBLIC int req_rename(fs_e, old_dir, old_name, new_dir, new_name)
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

    len_old= strlen(old_name) + 1;
    gid_old= cpf_grant_direct(fs_e, (vir_bytes)old_name, len_old,
	CPF_READ);
    if (gid_old == -1)
	panic(__FILE__, "req_rename: cpf_grant_direct failed", NO_NUM);
    len_new= strlen(new_name) + 1;
    gid_new= cpf_grant_direct(fs_e, (vir_bytes)new_name, len_new,
	CPF_READ);
    if (gid_new == -1)
	panic(__FILE__, "req_rename: cpf_grant_direct failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_RENAME_S;
    m.REQ_REN_OLD_DIR = old_dir;
    m.REQ_REN_NEW_DIR = new_dir;
    m.REQ_REN_GRANT_OLD = gid_old;
    m.REQ_REN_LEN_OLD = len_old;
    m.REQ_REN_GRANT_NEW = gid_new;
    m.REQ_REN_LEN_NEW = len_new;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(gid_old);
    cpf_revoke(gid_new);

    return r;
}


/*===========================================================================*
 *				req_rmdir	      			     *
 *===========================================================================*/
PUBLIC int req_rmdir(fs_e, inode_nr, lastc)
endpoint_t fs_e;
ino_t inode_nr;
char *lastc;
{
    int r;
    cp_grant_id_t gid;
    size_t len;
    message m;

    len= strlen(lastc) + 1;
    gid= cpf_grant_direct(fs_e, (vir_bytes)lastc, len, CPF_READ);
    if (gid == -1)
	panic(__FILE__, "req_rmdir: cpf_grant_direct failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_RMDIR_S;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_GRANT = gid;
    m.REQ_PATH_LEN = len;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(gid);

    return r;
}


/*===========================================================================*
 *				req_slink	      			     *
 *===========================================================================*/
PUBLIC int req_slink(fs_e, inode_nr, lastc, who_e, path_addr, path_length,
	uid, gid)
endpoint_t fs_e;
ino_t inode_nr;
char *lastc;
endpoint_t who_e;
char *path_addr;
unsigned short path_length;
uid_t uid;
gid_t gid;
{
    int r;
    size_t len;
    cp_grant_id_t gid_name, gid_buf;
    message m;

    len= strlen(lastc) + 1;
    gid_name= cpf_grant_direct(fs_e, (vir_bytes)lastc, len,
	CPF_READ);
    if (gid_name == -1)
	panic(__FILE__, "req_slink: cpf_grant_direct failed", NO_NUM);

    gid_buf= cpf_grant_magic(fs_e, who_e, (vir_bytes)path_addr,
	path_length, CPF_READ);
    if (gid_buf == -1)
    {
	cpf_revoke(gid_name);
	panic(__FILE__, "req_slink: cpf_grant_magic failed", NO_NUM);
    }

    /* Fill in request message */
    m.m_type = REQ_SLINK_S;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_UID = uid;
    m.REQ_GID = gid;
    m.REQ_GRANT = gid_name;
    m.REQ_PATH_LEN = len;
    m.REQ_GRANT2 = gid_buf;
    m.REQ_SLENGTH = path_length;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(gid_name);
    cpf_revoke(gid_buf);

    return r;
}


/*===========================================================================*
 *				req_stat	       			     *
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
	panic(__FILE__, "req_stat: cpf_grant_* failed", NO_NUM);

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
 *				req_sync	       			     *
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
 *				req_unlink	     			     *
 *===========================================================================*/
PUBLIC int req_unlink(fs_e, inode_nr, lastc)
endpoint_t fs_e;
ino_t inode_nr;
char *lastc;
{
    cp_grant_id_t gid;
    size_t len;
    int r;
    message m;

    len= strlen(lastc) + 1;
    gid= cpf_grant_direct(fs_e, (vir_bytes)lastc, len, CPF_READ);
    if (gid == -1)
	panic(__FILE__, "req_unlink: cpf_grant_direct failed", NO_NUM);

    /* Fill in request message */
    m.m_type = REQ_UNLINK_S;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_GRANT = gid;
    m.REQ_PATH_LEN = len;

    /* Send/rec request */
    r = fs_sendrec(fs_e, &m);

    cpf_revoke(gid);

    return r;
}


/*===========================================================================*
 *				req_unmount	    			     *
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
 *				req_utime	      			     *
 *===========================================================================*/
PUBLIC int req_utime(fs_e, inode_nr, actime, modtime)
endpoint_t fs_e;
ino_t inode_nr;
time_t actime;
time_t modtime;
{
    message m;

    /* Fill in request message */
    m.m_type = REQ_UTIME;
    m.REQ_INODE_NR = inode_nr;
    m.REQ_ACTIME = actime;
    m.REQ_MODTIME = modtime;

    /* Send/rec request */
    return fs_sendrec(fs_e, &m);
}




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

  if (fs_e == PM_PROC_NR)
  {
	printf("from %s, %d\n", file, line);
	panic(__FILE__, "talking to PM", NO_NUM);
  }

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


