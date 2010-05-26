/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#ifndef _HGFS_H
#define _HGFS_H

#include <sys/types.h>
#include <minix/u64.h>

typedef void *hgfs_file_t;		/* handle to open file */
typedef void *hgfs_dir_t;		/* handle to directory search */

struct hgfs_attr {
  u32_t a_mask;				/* which fields to retrieve/set */
  mode_t a_mode;			/* file type and permissions */
  u64_t a_size;				/* file size */
  time_t a_crtime;			/* file creation time */
  time_t a_atime;			/* file access time */
  time_t a_mtime;			/* file modification time */
  time_t a_ctime;			/* file change time */
};

#define HGFS_ATTR_SIZE		0x01	/* get/set file size */
#define HGFS_ATTR_CRTIME	0x02	/* get/set file creation time */
#define HGFS_ATTR_ATIME		0x04	/* get/set file access time */
#define HGFS_ATTR_MTIME		0x08	/* get/set file modification time */
#define HGFS_ATTR_CTIME		0x10	/* get/set file change time */
#define HGFS_ATTR_MODE		0x20	/* get/set file mode */
#define HGFS_ATTR_ALL	 	\
	(HGFS_ATTR_SIZE | HGFS_ATTR_CRTIME | HGFS_ATTR_ATIME | \
	HGFS_ATTR_MTIME | HGFS_ATTR_CTIME | HGFS_ATTR_MODE)

_PROTOTYPE( int hgfs_init, (void)					);
_PROTOTYPE( void hgfs_cleanup, (void)					);

_PROTOTYPE( int hgfs_enabled, (void)					);

_PROTOTYPE( int hgfs_open, (char *path, int flags, int mode,
			hgfs_file_t *handle)				);
_PROTOTYPE( int hgfs_read, (hgfs_file_t handle, char *buf, size_t size,
			u64_t offset)					);
_PROTOTYPE( int hgfs_write, (hgfs_file_t handle, const char *buf,
			size_t len, u64_t offset, int append)		);
_PROTOTYPE( int hgfs_close, (hgfs_file_t handle)			);

_PROTOTYPE( size_t hgfs_readbuf, (char **ptr)				);
_PROTOTYPE( size_t hgfs_writebuf, (char **ptr)				);

_PROTOTYPE( int hgfs_opendir, (char *path, hgfs_dir_t *handle)		);
_PROTOTYPE( int hgfs_readdir, (hgfs_dir_t handle, unsigned int index,
			char *buf, size_t size, struct hgfs_attr *attr)	);
_PROTOTYPE( int hgfs_closedir, (hgfs_dir_t handle)			);

_PROTOTYPE( int hgfs_getattr, (char *path, struct hgfs_attr *attr)	);
_PROTOTYPE( int hgfs_setattr, (char *path, struct hgfs_attr *attr)	);

_PROTOTYPE( int hgfs_mkdir, (char *path, int mode)			);
_PROTOTYPE( int hgfs_unlink, (char *path)				);
_PROTOTYPE( int hgfs_rmdir, (char *path)				);
_PROTOTYPE( int hgfs_rename, (char *opath, char *npath)			);

_PROTOTYPE( int hgfs_queryvol, (char *path, u64_t *free, u64_t *total)	);

#endif /* _HGFS_H */
