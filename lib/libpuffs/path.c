/*
 * This file contains the procedures that look up path names in the directory
 * system and determine the pnode number that goes with a given path name.
 *
 * Created (based on MFS):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"

#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <puffs.h>
#include <stdlib.h>
#include <string.h>

#include <minix/endpoint.h>
#include <minix/vfsif.h>
#include <minix/libminixfs.h>

#include "puffs.h"
#include "puffs_priv.h"

char dot2[3] = "..";	/* permissions for . and ..		    */

static char *get_name(char *name, char string[NAME_MAX+1]);
static int ltraverse(struct puffs_node *pn, char *suffix);
static int parse_path(ino_t dir_ino, ino_t root_ino, int flags, struct
	puffs_node **res_inop, size_t *offsetp, int *symlinkp);

/*===========================================================================*
 *                             fs_lookup				     *
 *===========================================================================*/
int fs_lookup(void)
{
  cp_grant_id_t grant;
  int r, r1, flags, symlinks;
  unsigned int len;
  size_t offset = 0, path_size;
  ino_t dir_ino, root_ino;
  struct puffs_node *pn;

  grant		= fs_m_in.m_vfs_fs_lookup.grant_path;
  path_size	= fs_m_in.m_vfs_fs_lookup.path_size;	/* Size of the buffer */
  len		= fs_m_in.m_vfs_fs_lookup.path_len;	/* including terminating nul */
  dir_ino	= fs_m_in.m_vfs_fs_lookup.dir_ino;
  root_ino	= fs_m_in.m_vfs_fs_lookup.root_ino;
  flags		= fs_m_in.m_vfs_fs_lookup.flags;

  /* Check length. */
  if (len > sizeof(user_path)) return(E2BIG);	/* too big for buffer */
  if (len == 0) return(EINVAL);			/* too small */

  /* Copy the pathname and set up caller's user and group id */
  r = sys_safecopyfrom(VFS_PROC_NR, grant, /*offset*/ 0,
            (vir_bytes) user_path, (size_t) len);
  if (r != OK) return(r);

  /* Verify this is a null-terminated path. */
  if (user_path[len - 1] != '\0') return(EINVAL);

  memset(&credentials, 0, sizeof(credentials));
  if(!(flags & PATH_GET_UCRED)) { /* Do we have to copy uid/gid credentials? */
        caller_uid      = fs_m_in.m_vfs_fs_lookup.uid;
        caller_gid      = fs_m_in.m_vfs_fs_lookup.gid;
  } else {
	if((r=fs_lookup_credentials(&credentials,
		&caller_uid, &caller_gid,
		fs_m_in.m_vfs_fs_lookup.grant_ucred,
		fs_m_in.m_vfs_fs_lookup.ucred_size)) != OK)
		return r;
  }


  /* Lookup pnode */
  pn = NULL;
  r = parse_path(dir_ino, root_ino, flags, &pn, &offset, &symlinks);

  if (symlinks != 0 && (r == ELEAVEMOUNT || r == EENTERMOUNT || r == ESYMLINK)){
	len = strlen(user_path)+1;
	if (len > path_size) return(ENAMETOOLONG);

	r1 = sys_safecopyto(VFS_PROC_NR, grant, (vir_bytes) 0,
			    (vir_bytes) user_path, (size_t) len);
	if (r1 != OK) return(r1);
  }

  if (r == ELEAVEMOUNT || r == ESYMLINK) {
	/* Report offset and the error */
	fs_m_out.m_fs_vfs_lookup.offset = offset;
	fs_m_out.m_fs_vfs_lookup.symloop = symlinks;
	
	return(r);
  }

  if (r != OK && r != EENTERMOUNT) {
	return(r);
  }

  if (r == OK) {
	/* Open pnode */
	pn->pn_count++;
  }

  fs_m_out.m_fs_vfs_lookup.inode	= pn->pn_va.va_fileid;
  fs_m_out.m_fs_vfs_lookup.mode		= pn->pn_va.va_mode;
  fs_m_out.m_fs_vfs_lookup.file_size	= pn->pn_va.va_size;
  fs_m_out.m_fs_vfs_lookup.symloop	= symlinks;
  fs_m_out.m_fs_vfs_lookup.uid		= pn->pn_va.va_uid;
  fs_m_out.m_fs_vfs_lookup.gid		= pn->pn_va.va_gid;

  /* This is only valid for block and character specials. But it doesn't
   * cause any harm to always set the device field. */
  fs_m_out.m_fs_vfs_lookup.device	= pn->pn_va.va_rdev;

  if (r == EENTERMOUNT) {
	fs_m_out.m_fs_vfs_lookup.offset	= offset;
  }
  
  return(r);
}


/*===========================================================================*
 *                             parse_path				     *
 *===========================================================================*/
static int parse_path(
	ino_t dir_ino,
	ino_t root_ino,
	int flags,
	struct puffs_node **res_inop,
	size_t *offsetp,
	int *symlinkp
)
{
  /* Parse the path in user_path, starting at dir_ino. If the path is the empty
   * string, just return dir_ino. It is upto the caller to treat an empty
   * path in a special way. Otherwise, if the path consists of just one or
   * more slash ('/') characters, the path is replaced with ".". Otherwise,
   * just look up the first (or only) component in path after skipping any
   * leading slashes.
   */
  int r, leaving_mount;
  struct puffs_node *pn, *pn_dir;
  char *cp, *next_cp; /* component and next component */
  char component[NAME_MAX+1];

  /* Start parsing path at the first component in user_path */
  cp = user_path;

  /* No symlinks encountered yet */
  *symlinkp = 0;

  /* Find starting pnode according to the request message */
  /* XXX it's deffinitely OK to use nodewalk here */
  if ((pn = puffs_pn_nodewalk(global_pu, 0, &dir_ino)) == NULL) {
	lpuffs_debug("nodewalk failed\n");
	return(ENOENT);
  }

  /* If dir has been removed return ENOENT. */
  if (pn->pn_va.va_nlink == NO_LINK) return(ENOENT);

  /* If the given start pnode is a mountpoint, we must be here because the file
   * system mounted on top returned an ELEAVEMOUNT error. In this case, we must
   * only accept ".." as the first path component.
   */
  leaving_mount = pn->pn_mountpoint; /* True iff pn is a mountpoint */

  /* Scan the path component by component. */
  while (TRUE) {
	if (cp[0] == '\0') {
		/* We're done; either the path was empty or we've parsed all
		   components of the path */

		*res_inop = pn;
		*offsetp += cp - user_path;

		/* Return EENTERMOUNT if we are at a mount point */
		if (pn->pn_mountpoint) return(EENTERMOUNT);

		return(OK);
	}

	while(cp[0] == '/') cp++;
	next_cp = get_name(cp, component);
	if (next_cp == NULL) {
		return(err_code);
	}

	/* Special code for '..'. A process is not allowed to leave a chrooted
	 * environment. A lookup of '..' at the root of a mounted filesystem
	 * has to return ELEAVEMOUNT. In both cases, the caller needs search
	 * permission for the current pnode, as it is used as directory.
	 */
	if (strcmp(component, "..") == 0) {
		/* 'pn' is now accessed as directory */
		if ((r = forbidden(pn, X_BIT)) != OK) {
			return(r);
		}

		if (pn->pn_va.va_fileid == root_ino) {
			cp = next_cp;
			continue;	/* Ignore the '..' at a process' root
					   and move on to the next component */
		}

		if (pn->pn_va.va_fileid == global_pu->pu_pn_root->pn_va.va_fileid
			&& !is_root_fs) {
			/* Climbing up to parent FS */
			*offsetp += cp - user_path;
			return(ELEAVEMOUNT);
		}
	}

	/* Only check for a mount point if we are not coming from one. */
	if (!leaving_mount && pn->pn_mountpoint) {
		/* Going to enter a child FS */

		*res_inop = pn;
		*offsetp += cp - user_path;
		return(EENTERMOUNT);
	}

	/* There is more path.  Keep parsing.
	 * If we're leaving a mountpoint, skip directory permission checks.
	 */
	pn_dir = pn;
	if ((pn_dir->pn_va.va_mode & I_TYPE) != I_DIRECTORY)  {
		return(ENOTDIR);
	}
	pn = advance(pn_dir, leaving_mount ? dot2 : component, CHK_PERM);
	if (err_code == ELEAVEMOUNT || err_code == EENTERMOUNT)
		err_code = OK;

	if (err_code != OK) {
		return(err_code);
	}
	
	assert(pn != NULL);

	leaving_mount = 0;

	/* The call to advance() succeeded.  Fetch next component. */
	if (S_ISLNK(pn->pn_va.va_mode)) {
		if (next_cp[0] == '\0' && (flags & PATH_RET_SYMLINK)) {
			*res_inop = pn;
			*offsetp += next_cp - user_path;
			return(OK);
		}

		/* Extract path name from the symlink file */
		r = ltraverse(pn, next_cp);
		next_cp = user_path;
		*offsetp = 0;

		/* Symloop limit reached? */
		if (++(*symlinkp) > _POSIX_SYMLOOP_MAX)
			r = ELOOP;

		if (r != OK)
			return(r);

		if (next_cp[0] == '/')
                        return(ESYMLINK);

		pn = pn_dir;
	}

	cp = next_cp; /* Process subsequent component in next round */
  }

}


/*===========================================================================*
 *                             ltraverse				     *
 *===========================================================================*/
static int ltraverse(
	struct puffs_node *pn,	/* symbolic link */
	char *suffix		/* current remaining path. Has to point in the
				 * user_path buffer
				 */
)
{
/* Traverse a symbolic link. Copy the link text from the pnode and insert
 * the text into the path. Return error code or report success. Base
 * directory has to be determined according to the first character of the
 * new pathname.
 */
  int r;
  char sp[PATH_MAX];
  size_t llen = PATH_MAX; /* length of link */
  size_t slen;	          /* length of suffix */
  PUFFS_MAKECRED(pcr, &global_kcred);


  if (!S_ISLNK(pn->pn_va.va_mode))
	r = EACCES;

  if (global_pu->pu_ops.puffs_node_readlink == NULL)
	return(EINVAL);

  if (global_pu->pu_ops.puffs_node_readlink(global_pu, pn, pcr, sp, &llen) != 0)
	return(EINVAL);

  slen = strlen(suffix);

  /* The path we're parsing looks like this:
   * /already/processed/path/<link> or
   * /already/processed/path/<link>/not/yet/processed/path
   * After expanding the <link>, the path will look like
   * <expandedlink> or
   * <expandedlink>/not/yet/processed
   * In both cases user_path must have enough room to hold <expandedlink>.
   * However, in the latter case we have to move /not/yet/processed to the
   * right place first, before we expand <link>. When strlen(<expandedlink>) is
   * smaller than strlen(/already/processes/path), we move the suffix to the
   * left. Is strlen(<expandedlink>) greater then we move it to the right. Else
   * we do nothing.
   */

  if (slen > 0) { /* Do we have path after the link? */
	/* For simplicity we require that suffix starts with a slash */
	if (suffix[0] != '/') {
		panic("ltraverse: suffix does not start with a slash");
	}

	/* To be able to expand the <link>, we have to move the 'suffix'
	 * to the right place.
	 */
	if (slen + llen + 1 > sizeof(user_path))
		return(ENAMETOOLONG);/* <expandedlink>+suffix+\0 does not fit*/
	if ((unsigned)(suffix - user_path) != llen) {
		/* Move suffix left or right if needed */
		memmove(&user_path[llen], suffix, slen+1);
	}
  } else {
	if (llen + 1 > sizeof(user_path))
		return(ENAMETOOLONG); /* <expandedlink> + \0 does not fit */

	/* Set terminating nul */
	user_path[llen]= '\0';
  }

  /* Everything is set, now copy the expanded link to user_path */
  memmove(user_path, sp, llen);

  return(OK);
}


/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
struct puffs_node *advance(
	struct puffs_node *pn_dir,	/* pnode for directory to be searched */
	char string[NAME_MAX + 1],	/* component name to look for */
	int chk_perm			/* check permissions when string is
					 * looked up*/
)
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the pnode, open it, and return a pointer to its pnode
 * slot.
 * TODO: instead of string, should get pcn.
 */
  struct puffs_node *pn;

  struct puffs_newinfo pni;

  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};

  enum vtype node_vtype;
  voff_t size;
  dev_t rdev;
  int error;

  err_code = OK;

  /* If 'string' is empty, return an error. */
  if (string[0] == '\0') {
	err_code = ENOENT;
	return(NULL);
  }

  /* Check for NULL. */
  if (pn_dir == NULL)
	return(NULL);

  if (chk_perm) {
	/* Just search permission is checked */
	if (forbidden(pn_dir, X_BIT)) {
		err_code = EACCES;
		return(NULL);
	}
  }

  if (strcmp(string, ".") == 0) {
	/* Otherwise we will fall into trouble: path for pnode to be looked up
	 * will be parent path (same pnode as the one to be looked up) +
	 * requested path. E.g. after several lookups we might get advance
	 * for "." with parent path "/././././././././.".
	 *
	 * Another problem is that after lookup pnode will be added
	 * to the pu_pnodelst, which already contains pnode instance for this
	 * pnode. It will cause lot of troubles.
	 */
	return pn_dir;
  }

  pni.pni_cookie = (void** )&pn;
  pni.pni_vtype = &node_vtype;
  pni.pni_size = &size;
  pni.pni_rdev = &rdev;

  pcn.pcn_namelen = strlen(string);
  assert(pcn.pcn_namelen <= MAXPATHLEN);
  strcpy(pcn.pcn_name, string);

  if (buildpath) {
	if (puffs_path_pcnbuild(global_pu, &pcn, pn_dir) != 0) {
		lpuffs_debug("pathbuild error\n");
		err_code = ENOENT;
		return(NULL);
	}
  }

  /* lookup *must* be present */
  error = global_pu->pu_ops.puffs_node_lookup(global_pu, pn_dir,
		  &pni, &pcn);

  if (buildpath) {
	if (error) {
		global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
		err_code = ENOENT;
		return(NULL);
	} else {
		struct puffs_node *_pn;

		/*
		 * did we get a new node or a
		 * recycled node?
		 */
		_pn = PU_CMAP(global_pu, pn);
		if (_pn->pn_po.po_path == NULL)
			_pn->pn_po = pcn.pcn_po_full;
		else
			global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
	}
  }

  if (error) {
	err_code = error < 0 ? error : -error;
	return(NULL);
  } else {
	/* In MFS/ext2 it's set by search_dir, puffs_node_lookup error codes are unclear,
	 * so we use error variable there
	 */
	err_code = OK;
  }

  assert(pn != NULL);

  /* The following test is for "mountpoint/.." where mountpoint is a
   * mountpoint. ".." will refer to the root of the mounted filesystem,
   * but has to become a reference to the parent of the 'mountpoint'
   * directory.
   *
   * This case is recognized by the looked up name pointing to a
   * root pnode, and the directory in which it is held being a
   * root pnode, _and_ the name[1] being '.'. (This is a test for '..'
   * and excludes '.'.)
   */
  if (pn->pn_va.va_fileid == global_pu->pu_pn_root->pn_va.va_fileid) {
	  if (pn_dir->pn_va.va_fileid == global_pu->pu_pn_root->pn_va.va_fileid) {
		  if (string[1] == '.') {
			  if (!is_root_fs) {
				  /* Climbing up mountpoint */
				  err_code = ELEAVEMOUNT;
			  }
		  }
	  }
  }

  /* See if the pnode is mounted on.  If so, switch to root directory of the
   * mounted file system.  The super_block provides the linkage between the
   * pnode mounted on and the root directory of the mounted file system.
   */
  if (pn->pn_mountpoint) {
	  /* Mountpoint encountered, report it */
	  err_code = EENTERMOUNT;
  }

  return(pn);
}


/*===========================================================================*
 *				get_name				     *
 *===========================================================================*/
static char *get_name(
	char *path_name,         /* path name to parse */
	char string[NAME_MAX+1]  /* component extracted from 'old_name' */
)
{
/* Given a pointer to a path name in fs space, 'path_name', copy the first
 * component to 'string' (truncated if necessary, always nul terminated).
 * A pointer to the string after the first component of the name as yet
 * unparsed is returned.  Roughly speaking,
 * 'get_name' = 'path_name' - 'string'.
 *
 * This routine follows the standard convention that /usr/ast, /usr//ast,
 * //usr///ast and /usr/ast/ are all equivalent.
 *
 * If len of component is greater, than allowed, then return 0.
 */
  size_t len;
  char *cp, *ep;

  cp = path_name;

  /* Skip leading slashes */
  while (cp[0] == '/') cp++;

  /* Find the end of the first component */
  ep = cp;
  while (ep[0] != '\0' && ep[0] != '/')
	ep++;

  len = (size_t) (ep - cp);
 
  /* XXX we don't check name_max of fileserver (probably we can't) */
  if (len > NAME_MAX) {
  	err_code = ENAMETOOLONG;
	return(NULL);
  }

  /* Special case of the string at cp is empty */
  if (len == 0)
	strcpy(string, ".");  /* Return "." */
  else {
	memcpy(string, cp, len);
	string[len]= '\0';
  }

  return(ep);
}
