/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

/* attr.c */
#define attr_get PREFIX(attr_get)
void attr_get(struct hgfs_attr *attr);

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

/* error.c */
#define error_convert PREFIX(error_convert)
int error_convert(int err);

/* path.c */
#define path_put PREFIX(path_put)
#define path_get PREFIX(path_get)
void path_put(char *path);
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
void time_put(time_t *timep);
void time_get(time_t *timep);
