#ifndef _MINIX_FSDRIVER_H
#define _MINIX_FSDRIVER_H

struct stat;
struct statvfs;
struct timespec;

/* Resulting node properties. */
struct fsdriver_node {
	ino_t fn_ino_nr;		/* inode number */
	mode_t fn_mode;			/* file mode */
	off_t fn_size;			/* file size */
	uid_t fn_uid;			/* owning user ID */
	gid_t fn_gid;			/* owning group ID */
	dev_t fn_dev;			/* device number, for block/char dev */
};

/* Opaque data structure for the fsdriver_copyin, _copyout, _zero functions. */
struct fsdriver_data {
	endpoint_t endpt;		/* source/destination endpoint */
	union {
		cp_grant_id_t grant;	/* grant, if endpt != SELF */
		char *ptr;		/* local pointer, if endpt == SELF */
	};
	size_t size;			/* total buffer size (check only) */
};

/* Opaque data structure for the fsdriver_dentry_ functions. */
struct fsdriver_dentry {
	const struct fsdriver_data *data;
	size_t data_size;
	size_t data_off;
	char *buf;
	size_t buf_size;
	size_t buf_off;
};

/*
 * For a few groups of calls, the functions have the same signature, so that
 * the file system can use a single implementation for multiple functions
 * without requiring extra stubs.  Thus, we pass in an extra parameter that
 * identifies the call; one of the values below.  For the same reason, the peek
 * and bpeek calls have a "data" parameter which is always set to NULL.
 */
#define FSC_READ	0		/* read or bread call */
#define FSC_WRITE	1		/* write or bwrite call */
#define FSC_PEEK	2		/* peek or bpeek call */

#define FSC_UNLINK	0		/* unlink call */
#define FSC_RMDIR	1		/* rmdir call */

/* Function call table for file system services. */
struct fsdriver {
	int (*fdr_mount)(dev_t dev, unsigned int flags,
	    struct fsdriver_node *root_node, unsigned int *res_flags);
	void (*fdr_unmount)(void);
	int (*fdr_lookup)(ino_t dir_nr, char *name, struct fsdriver_node *node,
	    int *is_mountpt);
	int (*fdr_newnode)(mode_t mode, uid_t uid, gid_t gid, dev_t dev,
	    struct fsdriver_node *node);
	int (*fdr_putnode)(ino_t ino_nr, unsigned int count);
	ssize_t (*fdr_read)(ino_t ino_nr, struct fsdriver_data *data,
	    size_t bytes, off_t pos, int call);
	ssize_t (*fdr_write)(ino_t ino_nr, struct fsdriver_data *data,
	    size_t bytes, off_t pos, int call);
	ssize_t (*fdr_peek)(ino_t ino_nr, struct fsdriver_data *data,
	    size_t bytes, off_t pos, int call);
	ssize_t (*fdr_getdents)(ino_t ino_nr, struct fsdriver_data *data,
	    size_t bytes, off_t *pos);
	int (*fdr_trunc)(ino_t ino_nr, off_t start_pos, off_t end_pos);
	void (*fdr_seek)(ino_t ino);
	int (*fdr_create)(ino_t dir_nr, char *name, mode_t mode, uid_t uid,
	    gid_t gid, struct fsdriver_node *node);
	int (*fdr_mkdir)(ino_t dir_nr, char *name, mode_t mode, uid_t uid,
	    gid_t gid);
	int (*fdr_mknod)(ino_t dir_nr, char *name, mode_t mode, uid_t uid,
	    gid_t gid, dev_t rdev);
	int (*fdr_link)(ino_t dir_nr, char *name, ino_t ino_nr);
	int (*fdr_unlink)(ino_t dir_nr, char *name, int call);
	int (*fdr_rmdir)(ino_t dir_nr, char *name, int call);
	int (*fdr_rename)(ino_t old_dir_nr, char *old_name, ino_t new_dir_nr,
	    char *new_name);
	int (*fdr_slink)(ino_t dir_nr, char *name, uid_t uid, gid_t gid,
	    struct fsdriver_data *data, size_t bytes);
	ssize_t (*fdr_rdlink)(ino_t ino_nr, struct fsdriver_data *data,
	    size_t bytes);
	int (*fdr_stat)(ino_t ino_nr, struct stat *buf);
	int (*fdr_chown)(ino_t ino_nr, uid_t uid, gid_t gid, mode_t *mode);
	int (*fdr_chmod)(ino_t ino_nr, mode_t *mode);
	int (*fdr_utime)(ino_t ino_nr, struct timespec *atime,
	    struct timespec *mtime);
	int (*fdr_mountpt)(ino_t ino_nr);
	int (*fdr_statvfs)(struct statvfs *buf);
	void (*fdr_sync)(void);
	void (*fdr_driver)(dev_t dev, char *label);
	ssize_t (*fdr_bread)(dev_t dev, struct fsdriver_data *data,
	    size_t bytes, off_t pos, int call);
	ssize_t (*fdr_bwrite)(dev_t dev, struct fsdriver_data *data,
	    size_t bytes, off_t pos, int call);
	ssize_t (*fdr_bpeek)(dev_t dev, struct fsdriver_data *data,
	    size_t bytes, off_t pos, int call);
	void (*fdr_bflush)(dev_t dev);
	void (*fdr_postcall)(void);
	void (*fdr_other)(const message *m_ptr, int ipc_status);
};

/* Functions defined by libfsdriver. */
void fsdriver_process(const struct fsdriver * __restrict fdp,
	const message * __restrict m_ptr, int ipc_status, int asyn_reply);
void fsdriver_terminate(void);
void fsdriver_task(struct fsdriver *fdp);

int fsdriver_copyin(const struct fsdriver_data *data, size_t off, void *ptr,
	size_t len);
int fsdriver_copyout(const struct fsdriver_data *data, size_t off,
	const void *ptr, size_t len);
int fsdriver_zero(const struct fsdriver_data *data, size_t off, size_t len);

void fsdriver_dentry_init(struct fsdriver_dentry * __restrict dentry,
	const struct fsdriver_data * __restrict data, size_t bytes,
	char * __restrict buf, size_t bufsize);
ssize_t fsdriver_dentry_add(struct fsdriver_dentry * __restrict dentry,
	ino_t ino_nr, const char * __restrict name, size_t namelen,
	unsigned int type);
ssize_t fsdriver_dentry_finish(struct fsdriver_dentry *dentry);

#endif /* !_MINIX_FSDRIVER_H */
