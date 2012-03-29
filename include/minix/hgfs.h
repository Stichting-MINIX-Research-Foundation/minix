/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#ifndef _HGFS_H
#define _HGFS_H

#include <sys/types.h>
#include <minix/u64.h>
#include <time.h>

typedef void *hgfs_file_t;		/* handle to open file */
typedef void *hgfs_dir_t;		/* handle to directory search */

struct hgfs_attr {
  u32_t a_mask;				/* which fields to retrieve/set */
  mode_t a_mode;			/* file type and permissions */
  u64_t a_size;				/* file size */
  struct timespec a_crtime;		/* file creation time */
  struct timespec a_atime;		/* file access time */
  struct timespec a_mtime;		/* file modification time */
  struct timespec a_ctime;		/* file change time */
};

#define HGFS_ATTR_SIZE		0x01	/* get/set file size */
#define HGFS_ATTR_CRTIME	0x02	/* get/set file creation time */
#define HGFS_ATTR_ATIME		0x04	/* get/set file access time */
#define HGFS_ATTR_MTIME		0x08	/* get/set file modification time */
#define HGFS_ATTR_CTIME		0x10	/* get/set file change time */
#define HGFS_ATTR_MODE		0x20	/* get/set file mode */
#define HGFS_ATTR_ATIME_SET	0x40	/* set specific file access time */
#define HGFS_ATTR_MTIME_SET	0x80	/* set specific file modify time */
#define HGFS_ATTR_ALL	 	\
	(HGFS_ATTR_SIZE | HGFS_ATTR_CRTIME | HGFS_ATTR_ATIME | \
	HGFS_ATTR_MTIME | HGFS_ATTR_CTIME | HGFS_ATTR_MODE | \
	HGFS_ATTR_ATIME_SET | HGFS_ATTR_MTIME_SET)

int hgfs_init(void);
void hgfs_cleanup(void);

int hgfs_enabled(void);

int hgfs_open(char *path, int flags, int mode, hgfs_file_t *handle);
int hgfs_read(hgfs_file_t handle, char *buf, size_t size, u64_t offset);
int hgfs_write(hgfs_file_t handle, const char *buf, size_t len, u64_t
	offset, int append);
int hgfs_close(hgfs_file_t handle);

size_t hgfs_readbuf(char **ptr);
size_t hgfs_writebuf(char **ptr);

int hgfs_opendir(char *path, hgfs_dir_t *handle);
int hgfs_readdir(hgfs_dir_t handle, unsigned int index, char *buf,
	size_t size, struct hgfs_attr *attr);
int hgfs_closedir(hgfs_dir_t handle);

int hgfs_getattr(char *path, struct hgfs_attr *attr);
int hgfs_setattr(char *path, struct hgfs_attr *attr);

int hgfs_mkdir(char *path, int mode);
int hgfs_unlink(char *path);
int hgfs_rmdir(char *path);
int hgfs_rename(char *opath, char *npath);

int hgfs_queryvol(char *path, u64_t *free, u64_t *total);

#endif /* _HGFS_H */
