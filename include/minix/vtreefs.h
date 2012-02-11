#ifndef _MINIX_VTREEFS_H
#define _MINIX_VTREEFS_H

struct inode;
typedef int index_t;
typedef void *cbdata_t;

#define NO_INDEX	((index_t) -1)

/* Maximum file name length, excluding terminating null character. It is set
 * to a low value to limit memory usage, but can be changed to any value.
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
	int (*read_hook)(struct inode *inode, off_t offset, char **ptr,
		size_t *len, cbdata_t cbdata);
	int (*rdlink_hook)(struct inode *inode, char *ptr, size_t max,
		cbdata_t cbdata);
	int (*message_hook)(message *m);
};

extern struct inode *add_inode(struct inode *parent, char *name, index_t index,
	struct inode_stat *stat, index_t nr_indexed_entries, cbdata_t cbdata);
extern void delete_inode(struct inode *inode);

extern struct inode *get_inode_by_name(struct inode *parent, char *name);
extern struct inode *get_inode_by_index(struct inode *parent, index_t index);

extern char const *get_inode_name(struct inode *inode);
extern index_t get_inode_index(struct inode *inode);
extern cbdata_t get_inode_cbdata(struct inode *inode);

extern struct inode *get_root_inode(void);
extern struct inode *get_parent_inode(struct inode *inode);
extern struct inode *get_first_inode(struct inode *parent);
extern struct inode *get_next_inode(struct inode *previous);

extern void get_inode_stat(struct inode *inode, struct inode_stat *stat);
extern void set_inode_stat(struct inode *inode, struct inode_stat *stat);

extern void start_vtreefs(struct fs_hooks *hooks, unsigned int nr_inodes,
	struct inode_stat *stat, index_t nr_indexed_entries);

#endif /* _MINIX_VTREEFS_H */
