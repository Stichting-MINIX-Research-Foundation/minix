#ifndef _PROCFS_PROTO_H
#define _PROCFS_PROTO_H

/* buf.c */
_PROTOTYPE( void buf_init, (off_t start, size_t len)			);
_PROTOTYPE( void buf_printf, (char *fmt, ...)				);
_PROTOTYPE( void buf_append, (char *data, size_t len)			);
_PROTOTYPE( size_t buf_get, (char **ptr)				);

/* tree.c */
_PROTOTYPE( int init_tree, (void)					);
_PROTOTYPE( int lookup_hook, (struct inode *parent, char *name,
	cbdata_t cbdata)						);
_PROTOTYPE( int getdents_hook, (struct inode *inode, cbdata_t cbdata)	);
_PROTOTYPE( int read_hook, (struct inode *inode, off_t offset,
	char **ptr, size_t *len, cbdata_t cbdata)			);
_PROTOTYPE( int rdlink_hook, (struct inode *inode, char *ptr,
	size_t max, cbdata_t cbdata)					);

/* util.c */
_PROTOTYPE( int	procfs_getloadavg, (struct load *loadavg, int nelem)	);

#endif /* _PROCFS_PROTO_H */
