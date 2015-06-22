#ifndef _MINIX_PTY_PTYFS_H
#define _MINIX_PTY_PTYFS_H

int ptyfs_set(unsigned int index, mode_t mode, uid_t uid, gid_t gid,
	dev_t dev);
int ptyfs_clear(unsigned int index);
int ptyfs_name(unsigned int index, char * name, size_t size);

#endif /* !_MINIX_PTY_PTYFS_H */
