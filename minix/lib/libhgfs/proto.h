/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

/* attr.c */
#define attr_get PREFIX(attr_get)
void attr_get(struct sffs_attr *attr);
int hgfs_getattr(const char *path, struct sffs_attr *attr);
int hgfs_setattr(const char *path, struct sffs_attr *attr);

/* backdoor.s */
#define backdoor PREFIX(backdoor)
#define backdoor_in PREFIX(backdoor_in)
#define backdoor_out PREFIX(backdoor_out)
u32_t backdoor(u32_t ptr[6]);
u32_t backdoor_in(u32_t ptr[6]);
u32_t backdoor_out(u32_t ptr[6]);

/* channel.c */
#define channel_open PREFIX(channel_open)
#define channel_close PREFIX(channel_close)
#define channel_send PREFIX(channel_send)
#define channel_recv PREFIX(channel_recv)
int channel_open(struct channel *ch, u32_t type);
void channel_close(struct channel *ch);
int channel_send(struct channel *ch, char *buf, int len);
int channel_recv(struct channel *ch, char *buf, int max);

/* dir.c */
int hgfs_opendir(const char *path, sffs_dir_t *handle);
int hgfs_readdir(sffs_dir_t handle, unsigned int index, char *buf, size_t size,
  struct sffs_attr *attr);
int hgfs_closedir(sffs_dir_t handle);

/* error.c */
#define error_convert PREFIX(error_convert)
int error_convert(int err);

/* file.c */
int hgfs_open(const char *path, int flags, int mode, sffs_file_t *handle);
ssize_t hgfs_read(sffs_file_t handle, char *buf, size_t size, u64_t offset);
ssize_t hgfs_write(sffs_file_t handle, char *buf, size_t len, u64_t offset);
int hgfs_close(sffs_file_t handle);
size_t hgfs_readbuf(char **ptr);
size_t hgfs_writebuf(char **ptr);

/* info.c */
int hgfs_queryvol(const char *path, u64_t *free, u64_t *total);

/* link.c */
int hgfs_mkdir(const char *path, int mode);
int hgfs_unlink(const char *path);
int hgfs_rmdir(const char *path);
int hgfs_rename(const char *opath, const char *npath);

/* path.c */
#define path_put PREFIX(path_put)
#define path_get PREFIX(path_get)
void path_put(const char *path);
int path_get(char *path, int max);

/* rpc.c */
#define rpc_open PREFIX(rpc_open)
#define rpc_query PREFIX(rpc_query)
#define rpc_test PREFIX(rpc_test)
#define rpc_close PREFIX(rpc_close)
int rpc_open(void);
int rpc_query(void);
int rpc_test(void);
void rpc_close(void);

/* time.c */
#define time_init PREFIX(time_init)
#define time_put PREFIX(time_put)
#define time_get PREFIX(time_get)
void time_init(void);
void time_put(struct timespec *tsp);
void time_get(struct timespec *tsp);
