#ifndef _MINIX_VTREEFS_H
#define _MINIX_VTREEFS_H

struct inode;
typedef int index_t;
typedef void *cbdata_t;

#define NO_INDEX	((index_t) -1)

/* Maximum file name length, excluding terminating null character, for which
 * the name will be allocated statically. Longer names will be allocated
 * dynamically, and should not be used for system-critical file systems.
 */
#define PNAME_MAX	24

struct inode_stat {
	mode_t mode;		/* file mode (type and permissions) */
	uid_t uid;		/* user ID */
	gid_t gid;		/* group ID */
	off_t size;		/* file size */
	dev_t dev;		/* device number (for char/block type files) */
};

struct fs_hooks {
	void (*init_hook)(void);
	void (*cleanup_hook)(void);
	int (*lookup_hook)(struct inode *inode, char *name, cbdata_t cbdata);
	int (*getdents_hook)(struct inode *inode, cbdata_t cbdata);
	ssize_t (*read_hook)(struct inode *inode, char *ptr, size_t len,
	    off_t off, cbdata_t cbdata);
	ssize_t (*write_hook)(struct inode *inode, char *ptr, size_t max,
	    off_t off, cbdata_t cbdata);
	int (*trunc_hook)(struct inode *inode, off_t offset, cbdata_t cbdata);
	int (*mknod_hook)(struct inode *inode, char *name,
	    struct inode_stat *stat, cbdata_t cbdata);
	int (*unlink_hook)(struct inode *inode, cbdata_t cbdata);
	int (*slink_hook)(struct inode *inode, char *name,
	    struct inode_stat *stat, char *path, cbdata_t cbdata);
	int (*rdlink_hook)(struct inode *inode, char *ptr, size_t max,
	    cbdata_t cbdata);
	int (*chstat_hook)(struct inode *inode, struct inode_stat *stat,
	    cbdata_t cbdata);
	void (*message_hook)(message *m, int ipc_status);
};

extern struct inode *add_inode(struct inode *parent, const char *name,
	index_t index, const struct inode_stat *stat,
	index_t nr_indexed_slots, cbdata_t cbdata);
extern void delete_inode(struct inode *inode);

extern struct inode *get_inode_by_name(const struct inode *parent,
	const char *name);
extern struct inode *get_inode_by_index(const struct inode *parent,
	index_t index);

extern const char *get_inode_name(const struct inode *inode);
extern index_t get_inode_index(const struct inode *inode);
extern index_t get_inode_slots(const struct inode *inode);
extern cbdata_t get_inode_cbdata(const struct inode *inode);
extern void *get_inode_extra(const struct inode *inode);

extern struct inode *get_root_inode(void);
extern struct inode *get_parent_inode(const struct inode *inode);
extern struct inode *get_first_inode(const struct inode *parent);
extern struct inode *get_next_inode(const struct inode *previous);

extern void get_inode_stat(const struct inode *inode, struct inode_stat *stat);
extern void set_inode_stat(struct inode *inode, struct inode_stat *stat);

extern void run_vtreefs(struct fs_hooks *hooks, unsigned int nr_inodes,
	size_t inode_extra, struct inode_stat *stat, index_t nr_indexed_slots,
	size_t buf_size);

#endif /* _MINIX_VTREEFS_H */
