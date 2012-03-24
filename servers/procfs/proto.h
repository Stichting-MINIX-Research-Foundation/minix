#ifndef _PROCFS_PROTO_H
#define _PROCFS_PROTO_H

/* buf.c */
void buf_init(off_t start, size_t len);
void buf_printf(char *fmt, ...);
void buf_append(char *data, size_t len);
size_t buf_get(char **ptr);

/* tree.c */
int init_tree(void);
int lookup_hook(struct inode *parent, char *name, cbdata_t cbdata);
int getdents_hook(struct inode *inode, cbdata_t cbdata);
int read_hook(struct inode *inode, off_t offset, char **ptr, size_t
	*len, cbdata_t cbdata);
int rdlink_hook(struct inode *inode, char *ptr, size_t max, cbdata_t
	cbdata);

/* util.c */
int procfs_getloadavg(struct load *loadavg, int nelem);

#endif /* _PROCFS_PROTO_H */
