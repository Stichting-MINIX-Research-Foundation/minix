/* This file contains the procedures that look up path names in the directory
 * system and determine the inode number that goes with a given path name.
 */
 
#include "fs.h"
#include <assert.h>
#include <string.h>
#include "buf.h"
#include "inode.h"
#include "super.h"


/*===========================================================================*
 *                             fs_lookup				     *
 *===========================================================================*/
int fs_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt)
{
  struct inode *dirp, *rip;

  /* Find the starting inode. */
  if ((dirp = find_inode(fs_dev, dir_nr)) == NULL)
	return EINVAL;

  /* Look up the directory entry. */
  if ((rip = advance(dirp, name)) == NULL)
	return err_code;

  /* On success, leave the resulting inode open and return its details. */
  node->fn_ino_nr = rip->i_num;
  node->fn_mode = rip->i_mode;
  node->fn_size = rip->i_size;
  node->fn_uid = rip->i_uid;
  node->fn_gid = rip->i_gid;
  /* This is only valid for block and character specials. But it doesn't
   * cause any harm to always set the device field. */
  node->fn_dev = (dev_t) rip->i_zone[0];

  *is_mountpt = rip->i_mountpoint;

  return OK;
}


/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
struct inode *advance(dirp, string)
struct inode *dirp;		/* inode for directory to be searched */
const char *string;		/* component name to look for */
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.
 */
  ino_t numb;
  struct inode *rip;

  assert(dirp != NULL);

  /* If 'string' is empty, return an error. */
  if (string[0] == '\0') {
  	err_code = ENOENT;
	return(NULL);
  }

  /* If dir has been removed return ENOENT. */
  if (dirp->i_nlinks == NO_LINK) {
	err_code = ENOENT;
	return(NULL);
  }

  /* If 'string' is not present in the directory, signal error. */
  if ( (err_code = search_dir(dirp, string, &numb, LOOK_UP)) != OK) {
	return(NULL);
  }

  /* The component has been found in the directory.  Get inode. */
  if ( (rip = get_inode(dirp->i_dev, (int) numb)) == NULL)  {
	assert(err_code != OK);
	return(NULL);
  }

  assert(err_code == OK);
  return(rip);
}


/*===========================================================================*
 *				search_dir				     *
 *===========================================================================*/
int search_dir(ldir_ptr, string, numb, flag)
register struct inode *ldir_ptr; /* ptr to inode for dir to search */
const char *string;		 /* component to search for */
ino_t *numb;			 /* pointer to inode number */
int flag;			 /* LOOK_UP, ENTER, DELETE or IS_EMPTY */
{
/* This function searches the directory whose inode is pointed to by 'ldip':
 * if (flag == ENTER)  enter 'string' in the directory with inode # '*numb';
 * if (flag == DELETE) delete 'string' from the directory;
 * if (flag == LOOK_UP) search for 'string' and return inode # in 'numb';
 * if (flag == IS_EMPTY) return OK if only . and .. in dir else ENOTEMPTY;
 *
 * This function, and this function alone, implements name truncation,
 * by simply considering only the first MFS_NAME_MAX bytes from 'string'.
 */
  register struct direct *dp = NULL;
  register struct buf *bp = NULL;
  int i, r, e_hit, t, match;
  off_t pos;
  unsigned new_slots, old_slots;
  struct super_block *sp;
  int extended = 0;

  /* If 'ldir_ptr' is not a pointer to a dir inode, error. */
  if ( (ldir_ptr->i_mode & I_TYPE) != I_DIRECTORY)  {
	return(ENOTDIR);
   }

  if((flag == DELETE || flag == ENTER) && ldir_ptr->i_sp->s_rd_only)
	return EROFS;
  
  /* Step through the directory one block at a time. */
  old_slots = (unsigned) (ldir_ptr->i_size/DIR_ENTRY_SIZE);
  new_slots = 0;
  e_hit = FALSE;
  match = 0;			/* set when a string match occurs */

  pos = 0;
  if (flag == ENTER && ldir_ptr->i_last_dpos < ldir_ptr->i_size) {
	pos = ldir_ptr->i_last_dpos;
	new_slots = (unsigned) (pos/DIR_ENTRY_SIZE);
  }

  for (; pos < ldir_ptr->i_size; pos += ldir_ptr->i_sp->s_block_size) {
	assert(ldir_ptr->i_dev != NO_DEV);

	/* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	bp = get_block_map(ldir_ptr, pos);

	assert(ldir_ptr->i_dev != NO_DEV);
	assert(bp != NULL);

	/* Search a directory block. */
	for (dp = &b_dir(bp)[0];
		dp < &b_dir(bp)[NR_DIR_ENTRIES(ldir_ptr->i_sp->s_block_size)];
		dp++) {
		if (++new_slots > old_slots) { /* not found, but room left */
			if (flag == ENTER) e_hit = TRUE;
			break;
		}

		/* Match occurs if string found. */
		if (flag != ENTER && dp->mfs_d_ino != NO_ENTRY) {
			if (flag == IS_EMPTY) {
				/* If this test succeeds, dir is not empty. */
				if (strcmp(dp->mfs_d_name, "." ) != 0 &&
				    strcmp(dp->mfs_d_name, "..") != 0)
					match = 1;
			} else {
				if (strncmp(dp->mfs_d_name, string,
					sizeof(dp->mfs_d_name)) == 0){
					match = 1;
				}
			}
		}

		if (match) {
			/* LOOK_UP or DELETE found what it wanted. */
			r = OK;
			if (flag == IS_EMPTY) r = ENOTEMPTY;
			else if (flag == DELETE) {
				/* Save d_ino for recovery. */
				t = MFS_NAME_MAX - sizeof(ino_t);
				*((ino_t *) &dp->mfs_d_name[t]) = dp->mfs_d_ino;
				dp->mfs_d_ino = NO_ENTRY; /* erase entry */
				MARKDIRTY(bp);
				ldir_ptr->i_update |= CTIME | MTIME;
				IN_MARKDIRTY(ldir_ptr);
				if (pos < ldir_ptr->i_last_dpos)
					ldir_ptr->i_last_dpos = pos;
			} else {
				sp = ldir_ptr->i_sp;	/* 'flag' is LOOK_UP */
				*numb = (ino_t) conv4(sp->s_native,
						      (int) dp->mfs_d_ino);
			}
			put_block(bp);
			return(r);
		}

		/* Check for free slot for the benefit of ENTER. */
		if (flag == ENTER && dp->mfs_d_ino == 0) {
			e_hit = TRUE;	/* we found a free slot */
			break;
		}
	}

	/* The whole block has been searched or ENTER has a free slot. */
	if (e_hit) break;	/* e_hit set if ENTER can be performed now */
	put_block(bp);		/* otherwise, continue searching dir */
  }

  /* The whole directory has now been searched. */
  if (flag != ENTER) {
  	return(flag == IS_EMPTY ? OK : ENOENT);
  }

  /* When ENTER next time, start searching for free slot from
   * i_last_dpos. It gives some performance improvement (3-5%).
   */
  ldir_ptr->i_last_dpos = pos;

  /* This call is for ENTER.  If no free slot has been found so far, try to
   * extend directory.
   */
  if (e_hit == FALSE) { /* directory is full and no room left in last block */
	new_slots++;		/* increase directory size by 1 entry */
	if (new_slots == 0) return(EFBIG); /* dir size limited by slot count */
	if ( (bp = new_block(ldir_ptr, ldir_ptr->i_size)) == NULL)
		return(err_code);
	dp = &b_dir(bp)[0];
	extended = 1;
  }

  /* 'bp' now points to a directory block with space. 'dp' points to slot. */
  (void) memset(dp->mfs_d_name, 0, (size_t) MFS_NAME_MAX); /* clear entry */
  for (i = 0; i < MFS_NAME_MAX && string[i]; i++) dp->mfs_d_name[i] = string[i];
  sp = ldir_ptr->i_sp; 
  dp->mfs_d_ino = conv4(sp->s_native, (int) *numb);
  MARKDIRTY(bp);
  put_block(bp);
  ldir_ptr->i_update |= CTIME | MTIME;	/* mark mtime for update later */
  IN_MARKDIRTY(ldir_ptr);
  if (new_slots > old_slots) {
	ldir_ptr->i_size = (off_t) new_slots * DIR_ENTRY_SIZE;
	/* Send the change to disk if the directory is extended. */
	if (extended) rw_inode(ldir_ptr, WRITING);
  }
  return(OK);
}

