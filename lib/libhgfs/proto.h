/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

/* attr.c */
#define attr_get PREFIX(attr_get)
_PROTOTYPE( void attr_get, (struct hgfs_attr *attr)			);

/* backdoor.s */
#define backdoor PREFIX(backdoor)
#define backdoor_in PREFIX(backdoor_in)
#define backdoor_out PREFIX(backdoor_out)
_PROTOTYPE( u32_t backdoor, (u32_t ptr[6])				);
_PROTOTYPE( u32_t backdoor_in, (u32_t ptr[6])				);
_PROTOTYPE( u32_t backdoor_out, (u32_t ptr[6])				);

/* channel.c */
#define channel_open PREFIX(channel_open)
#define channel_close PREFIX(channel_close)
#define channel_send PREFIX(channel_send)
#define channel_recv PREFIX(channel_recv)
_PROTOTYPE( int channel_open, (struct channel *ch, u32_t type)		);
_PROTOTYPE( void channel_close, (struct channel *ch)			);
_PROTOTYPE( int channel_send, (struct channel *ch, char *buf, int len)	);
_PROTOTYPE( int channel_recv, (struct channel *ch, char *buf, int max)	);

/* error.c */
#define error_convert PREFIX(error_convert)
_PROTOTYPE( int error_convert, (int err)				);

/* path.c */
#define path_put PREFIX(path_put)
#define path_get PREFIX(path_get)
_PROTOTYPE( void path_put, (char *path)					);
_PROTOTYPE( int path_get, (char *path, int max)				);

/* rpc.c */
#define rpc_open PREFIX(rpc_open)
#define rpc_query PREFIX(rpc_query)
#define rpc_test PREFIX(rpc_test)
#define rpc_close PREFIX(rpc_close)
_PROTOTYPE( int rpc_open, (void)					);
_PROTOTYPE( int rpc_query, (void)					);
_PROTOTYPE( int rpc_test, (void)					);
_PROTOTYPE( void rpc_close, (void)					);

/* time.c */
#define time_init PREFIX(time_init)
#define time_put PREFIX(time_put)
#define time_get PREFIX(time_get)
_PROTOTYPE( void time_init, (void)					);
_PROTOTYPE( void time_put, (time_t *timep)				);
_PROTOTYPE( void time_get, (time_t *timep)				);
