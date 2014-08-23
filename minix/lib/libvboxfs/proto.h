/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#ifndef _VBOXFS_PROTO_H
#define _VBOXFS_PROTO_H

/* attr.c */
void vboxfs_get_attr(struct sffs_attr *attr, vboxfs_objinfo_t *info);
int vboxfs_getattr(const char *path, struct sffs_attr *attr);
int vboxfs_setattr(const char *path, struct sffs_attr *attr);

/* dir.c */
int vboxfs_opendir(const char *path, sffs_dir_t *handle);
int vboxfs_readdir(sffs_dir_t handle, unsigned int index, char *buf,
	size_t size, struct sffs_attr *attr);
int vboxfs_closedir(sffs_dir_t handle);

/* file.c */
int vboxfs_open(const char *path, int flags, int mode, sffs_file_t *handle);
ssize_t vboxfs_read(sffs_file_t handle, char *buf, size_t size, u64_t pos);
ssize_t vboxfs_write(sffs_file_t handle, char *buf, size_t len, u64_t pos);
int vboxfs_close(sffs_file_t handle);
size_t vboxfs_buffer(char **ptr);

/* handle.c */
int vboxfs_open_file(const char *path, int flags, int mode,
	vboxfs_handle_t *handlep, vboxfs_objinfo_t *infop);
void vboxfs_close_file(vboxfs_handle_t handle);

/* info.c */
int vboxfs_getset_info(vboxfs_handle_t handle, u32_t flags, void *data,
	size_t size);
int vboxfs_query_vol(const char *path, vboxfs_volinfo_t *volinfo);
int vboxfs_queryvol(const char *path, u64_t *free, u64_t *total);

/* link.c */
int vboxfs_mkdir(const char *path, int mode);
int vboxfs_unlink(const char *path);
int vboxfs_rmdir(const char *path);
int vboxfs_rename(const char *opath, const char *npath);

/* path.c */
int vboxfs_set_path(vboxfs_path_t *path, const char *name);
int vboxfs_get_path(vboxfs_path_t *path, char *name, size_t size);
size_t vboxfs_get_path_size(vboxfs_path_t *path);

#endif /* !_VBOXFS_PROTO_H */
