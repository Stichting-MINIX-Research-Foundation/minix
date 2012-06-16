/* VTreeFS - stadir.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

#include <time.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <string.h>

/*===========================================================================*
 *				fs_stat					     *
 *===========================================================================*/
int fs_stat(void)
{
	/* Retrieve file status.
	 */
	char path[PATH_MAX];
	struct stat statbuf;
	time_t cur_time;
	struct inode *node;
	int r;

	if ((node = find_inode(fs_m_in.REQ_INODE_NR)) == NULL)
		return EINVAL;

	memset(&statbuf, 0, sizeof(struct stat));

	/* Fill in the basic info. */
	statbuf.st_dev = fs_dev;
	statbuf.st_ino = get_inode_number(node);
	statbuf.st_mode = node->i_stat.mode;
	statbuf.st_nlink = !is_inode_deleted(node);
	statbuf.st_uid = node->i_stat.uid;
	statbuf.st_gid = node->i_stat.gid;
	statbuf.st_rdev = (dev_t) node->i_stat.dev;
	statbuf.st_size = node->i_stat.size;

	/* If it is a symbolic link, return the size of the link target. */
	if (S_ISLNK(node->i_stat.mode) && vtreefs_hooks->rdlink_hook != NULL) {
		r = vtreefs_hooks->rdlink_hook(node, path, sizeof(path),
			get_inode_cbdata(node));

		if (r == OK)
			statbuf.st_size = strlen(path);
	}

	/* Take the current time as file time for all files. */
	cur_time = time(NULL);
	statbuf.st_atime = cur_time;
	statbuf.st_mtime = cur_time;
	statbuf.st_ctime = cur_time;

	/* Copy the struct to user space. */
	return sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0,
		(vir_bytes) &statbuf, (phys_bytes) sizeof(statbuf));
}

/*===========================================================================*
 *				fs_fstatfs				     *
 *===========================================================================*/
int fs_fstatfs(void)
{
	/* Retrieve file system statistics.
	 */
	struct statfs statfs;

	memset(&statfs, 0, sizeof(statfs));

	/* Copy the struct to user space. */
	return sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0,
		(vir_bytes) &statfs, (phys_bytes) sizeof(statfs));
}

/*===========================================================================*
 *				fs_fstatfs				     *
 *===========================================================================*/
int fs_statvfs(void)
{
	/* Retrieve file system statistics.
	 */
	struct statvfs statvfs;

	memset(&statvfs, 0, sizeof(statvfs));

	statvfs.f_fsid = fs_dev;
	statvfs.f_flag = ST_RDONLY | ST_NOTRUNC;
	statvfs.f_namemax = PNAME_MAX;

	return sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0,
		(vir_bytes) &statvfs, sizeof(statvfs));
}
