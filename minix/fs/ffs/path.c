/* This file contains the procedures that look up path-name components in the
 * directory system and that add and remove directory entries.
 *
 * Directories are searched and modified one UFS_DIRBLKSIZ (512-byte) chunk at a
 * time so that no directory entry ever crosses a 512-byte boundary, matching
 * the on-disk convention fsck_ffs enforces.
 */

#include "fs.h"
#include <assert.h>
#include <string.h>
#include <sys/param.h>
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
	return(EINVAL);

  /* Look up the directory entry. */
  if ((rip = advance(dirp, name)) == NULL)
	return(err_code);

  /* On success, leave the resulting inode open and return its details. */
  node->fn_ino_nr = rip->i_num;
  node->fn_mode = rip->i_din.di_mode;
  node->fn_size = rip->i_din.di_size;
  node->fn_uid = rip->i_din.di_uid;
  node->fn_gid = rip->i_din.di_gid;
  /* Only meaningful for block/char specials, but harmless otherwise. */
  node->fn_dev = (dev_t) rip->i_din.di_rdev;

  *is_mountpt = rip->i_mountpoint;

  return(OK);
}

/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
struct inode *advance(struct inode *dirp, const char *string)
{
/* Look up a path-name component in a directory, open the resulting inode, and
 * return a pointer to it.
 */
  ino_t numb;
  struct inode *rip;

  assert(dirp != NULL);

  /* If 'string' is empty, return an error. */
  if (string[0] == '\0') {
	err_code = ENOENT;
	return(NULL);
  }

  /* If the directory has been removed, return ENOENT. */
  if (dirp->i_din.di_nlink == 0) {
	err_code = ENOENT;
	return(NULL);
  }

  /* If 'string' is not present in the directory, signal an error. */
  if ((err_code = search_dir(dirp, string, &numb, LOOK_UP, 0)) != OK)
	return(NULL);

  /* The component has been found in the directory.  Get its inode. */
  if ((rip = get_inode(dirp->i_dev, numb)) == NULL) {
	assert(err_code != OK);
	return(NULL);
  }

  return(rip);
}

/*===========================================================================*
 *				search_dir				     *
 *===========================================================================*/
int search_dir(struct inode *ldir_ptr, const char *string, ino_t *numb,
	int flag, mode_t mode)
{
/* Search the directory whose inode is 'ldir_ptr':
 *   LOOK_UP : find 'string' and return its inode number in '*numb';
 *   ENTER   : add 'string' with inode number '*numb' and type from 'mode';
 *   DELETE  : remove 'string';
 *   IS_EMPTY: return OK if the dir holds only . and .., else ENOTEMPTY.
 */
  struct buf *bp;
  struct direct *dp, *prev_dp;
  off_t pos, dir_size;
  unsigned int block_size, coff, base, reclen, dsize;
  size_t string_len;
  int required, match;

  if ((ldir_ptr->i_din.di_mode & I_TYPE) != I_DIRECTORY)
	return(ENOTDIR);

  if ((flag == ENTER || flag == DELETE) && ldir_ptr->i_sp->s_rd_only)
	return(EROFS);

  string_len = strlen(string);
  if (string_len > FFS_MAXNAMLEN)
	return(ENAMETOOLONG);

  block_size = ldir_ptr->i_sp->s_block_size;
  dir_size = (off_t) ldir_ptr->i_din.di_size;
  required = (int) UFS_DIRECTSIZ(string_len);

  /* Scan the directory one 512-byte chunk at a time. */
  for (pos = 0; pos < dir_size; pos += UFS_DIRBLKSIZ) {
	bp = get_block_map(ldir_ptr, (uint64_t) pos);
	assert(bp != NULL);
	base = (unsigned int) (pos % block_size);
	prev_dp = NULL;

	for (coff = 0; coff < UFS_DIRBLKSIZ; coff += reclen) {
		dp = (struct direct *) (b_data(bp) + base + coff);
		reclen = dp->d_reclen;
		if (reclen < UFS_DIRECTSIZ(0))
			break;		/* corrupt chunk */

		if (flag != ENTER && dp->d_ino != 0) {
			/* LOOK_UP / DELETE / IS_EMPTY: test for a match. */
			if (flag == IS_EMPTY) {
				if (!(dp->d_namlen == 1 &&
				      dp->d_name[0] == '.') &&
				    !(dp->d_namlen == 2 &&
				      dp->d_name[0] == '.' &&
				      dp->d_name[1] == '.'))
					match = 1;
				else
					match = 0;
			} else {
				match = (dp->d_namlen == string_len &&
				    memcmp(dp->d_name, string, string_len) == 0);
			}

			if (match) {
				if (flag == IS_EMPTY) {
					put_block(bp);
					return(ENOTEMPTY);
				}
				if (flag == LOOK_UP) {
					*numb = (ino_t) dp->d_ino;
					put_block(bp);
					return(OK);
				}
				/* flag == DELETE: coalesce with the previous
				 * entry in the same chunk if there is one. */
				if (prev_dp != NULL)
					prev_dp->d_reclen += dp->d_reclen;
				else
					dp->d_ino = 0;
				lmfs_markdirty(bp);
				put_block(bp);
				ldir_ptr->i_update |= CTIME | MTIME;
				ldir_ptr->i_dirt = IN_DIRTY;
				return(OK);
			}
		}

		if (flag == ENTER) {
			/* A free slot big enough, or a used slot with enough
			 * trailing slack to split off, will do. */
			if (dp->d_ino == 0 && (int) reclen >= required) {
				goto enter_here;
			}
			dsize = (dp->d_ino != 0) ?
				(unsigned int) UFS_DIRSIZ(dp) : 0;
			if (dp->d_ino != 0 &&
			    (int) (reclen - dsize) >= required) {
				/* Split: shrink this entry, create a new one. */
				dp->d_reclen = dsize;
				dp = (struct direct *)
					((char *) dp + dsize);
				dp->d_ino = 0;
				dp->d_reclen = reclen - dsize;
				goto enter_here;
			}
		}

		prev_dp = dp;
	}

	put_block(bp);
	continue;

enter_here:
	/* 'dp' points to a free slot of length dp->d_reclen >= required. */
	dp->d_ino = (u_int32_t) *numb;
	dp->d_namlen = (u_int8_t) string_len;
	dp->d_type = (u_int8_t) FFS_IFTODT(mode);
	memcpy(dp->d_name, string, string_len);
	dp->d_name[string_len] = '\0';
	lmfs_markdirty(bp);
	put_block(bp);
	ldir_ptr->i_update |= CTIME | MTIME;
	ldir_ptr->i_dirt = IN_DIRTY;
	return(OK);
  }

  if (flag != ENTER)
	return(flag == IS_EMPTY ? OK : ENOENT);

  /* No room in any existing chunk: grow the directory by one chunk.  A
   * directory's size is a multiple of UFS_DIRBLKSIZ, not of fs_bsize, so this
   * chunk may share a block -- and a cache block -- with earlier ones. */
  if ((bp = new_block(ldir_ptr, dir_size)) == NULL)
	return(err_code);

  dp = (struct direct *) (b_data(bp) + (unsigned int) (dir_size % block_size));
  dp->d_ino = (u_int32_t) *numb;
  dp->d_reclen = UFS_DIRBLKSIZ;
  dp->d_namlen = (u_int8_t) string_len;
  dp->d_type = (u_int8_t) FFS_IFTODT(mode);
  memcpy(dp->d_name, string, string_len);
  dp->d_name[string_len] = '\0';
  lmfs_markdirty(bp);
  put_block(bp);

  ldir_ptr->i_din.di_size = dir_size + UFS_DIRBLKSIZ;
  ldir_ptr->i_update |= CTIME | MTIME;
  ldir_ptr->i_dirt = IN_DIRTY;
  return(OK);
}
