/* Part of libsffs - (c) 2012, D.C. van Moolenbroek */

#ifndef _MINIX_SFFS_H
#define _MINIX_SFFS_H

#include <sys/types.h>
#include <sys/time.h>
#include <minix/u64.h>

typedef void *sffs_file_t;		/* handle to open file */
typedef void *sffs_dir_t;		/* handle to directory search */

struct sffs_attr {
  u32_t a_mask;				/* which fields to retrieve/set */
  mode_t a_mode;			/* file type and permissions */
  u64_t a_size;				/* file size */
  struct timespec a_crtime;		/* file creation time */
  struct timespec a_atime;		/* file access time */
  struct timespec a_mtime;		/* file modification time */
  struct timespec a_ctime;		/* file change time */
};

#define SFFS_ATTR_SIZE		0x01	/* get/set file size */
#define SFFS_ATTR_CRTIME	0x02	/* get/set file creation time */
#define SFFS_ATTR_ATIME		0x04	/* get/set file access time */
#define SFFS_ATTR_MTIME		0x08	/* get/set file modification time */
#define SFFS_ATTR_CTIME		0x10	/* get/set file change time */
#define SFFS_ATTR_MODE		0x20	/* get/set file mode */

struct sffs_table {
  int (*t_open)(char *path, int flags, int mode, sffs_file_t *handle);
  ssize_t (*t_read)(sffs_file_t handle, char *buf, size_t size, u64_t pos);
  ssize_t (*t_write)(sffs_file_t handle, char *buf, size_t size, u64_t pos);
  int (*t_close)(sffs_file_t handle);

  size_t (*t_readbuf)(char **ptr);
  size_t (*t_writebuf)(char **ptr);

  int (*t_opendir)(char *path, sffs_dir_t *handle);
  int (*t_readdir)(sffs_dir_t handle, unsigned int index, char *buf,
	size_t size, struct sffs_attr *attr);
  int (*t_closedir)(sffs_dir_t handle);

  int (*t_getattr)(char *path, struct sffs_attr *attr);
  int (*t_setattr)(char *path, struct sffs_attr *attr);

  int (*t_mkdir)(char *path, int mode);
  int (*t_unlink)(char *path);
  int (*t_rmdir)(char *path);
  int (*t_rename)(char *opath, char *npath);

  int (*t_queryvol)(char *path, u64_t *free, u64_t *total);
};

struct sffs_params {
  char p_prefix[PATH_MAX];	/* prefix for all paths used */
  uid_t p_uid;			/* UID that owns all files */
  gid_t p_gid;			/* GID that owns all files */
  unsigned int p_file_mask;	/* AND-mask to apply to file permissions */
  unsigned int p_dir_mask;	/* AND-mask to apply to directory perms */
  int p_case_insens;		/* case insensitivity flag */
};

int sffs_init(char *name, const struct sffs_table *table,
  struct sffs_params *params);
void sffs_signal(int signo);
void sffs_loop(void);

#endif /* _MINIX_SFFS_H */
