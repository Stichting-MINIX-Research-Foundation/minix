#ifndef _PROCFS_PROTO_H
#define _PROCFS_PROTO_H

/* buf.c */
void buf_init(char *ptr, size_t len, off_t start);
void buf_printf(char *fmt, ...) __attribute__((__format__(__printf__, 1, 2)));
void buf_append(char *data, size_t len);
ssize_t buf_result(void);

/* cpuinfo.c */
void root_cpuinfo(void);

/* service.c */
void service_init(void);
void service_lookup(struct inode *parent, clock_t now);
void service_getdents(struct inode *node);
void service_read(struct inode *node);

/* tree.c */
int init_tree(void);
int lookup_hook(struct inode *parent, char *name, cbdata_t cbdata);
int getdents_hook(struct inode *inode, cbdata_t cbdata);
ssize_t read_hook(struct inode *inode, char *ptr, size_t len, off_t off,
	cbdata_t cbdata);
int rdlink_hook(struct inode *inode, char *ptr, size_t max, cbdata_t cbdata);
pid_t pid_from_slot(int slot);
void out_of_inodes(void);

/* util.c */
int procfs_getloadavg(struct load *loadavg, int nelem);

#endif /* _PROCFS_PROTO_H */
