/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

/* Various macros used here and there */
#define MAKELONG(a,b) ((a) | ((b) << 16))
#define HIWORD(d) ((d) >> 16)
#define LOWORD(d) ((d) & 0xffff)
#define BYTES(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

/* Valid channel types for channel_open() */
#define CH_IN BYTES('T', 'C', 'L', 'O')
#define CH_OUT BYTES('R', 'P', 'C', 'I')

/* RPC constants */
#define RPC_BUF_SIZE 6134			/* max size of RPC request */
#define RPC_HDR_SIZE 10				/* RPC HGFS header size */

/* RPC macros. These NEED NOT be portable. VMware only does x86(-64) anyway. */
/* ..all this because ACK can't pack structures :( */
#define RPC_NEXT8 *(((u8_t*)(++rpc_ptr))-1)	/* get/set next byte */
#define RPC_NEXT16 *(((u16_t*)(rpc_ptr+=2))-1)	/* get/set next short */
#define RPC_NEXT32 *(((u32_t*)(rpc_ptr+=4))-1)	/* get/set next long */
#define RPC_LEN (rpc_ptr - rpc_buf)		/* request length thus far */
#define RPC_ADVANCE(n) rpc_ptr += n		/* skip n bytes in buffer */
#define RPC_PTR (rpc_ptr)			/* pointer to next data */
#define RPC_RESET rpc_ptr = rpc_buf		/* start at beginning */
#define RPC_REQUEST(r) \
  RPC_RESET; \
  RPC_NEXT8 = 'f'; \
  RPC_NEXT8 = ' '; \
  RPC_NEXT32 = 0; \
  RPC_NEXT32 = r;				/* start a RPC request */

/* HGFS requests */
enum {
  HGFS_REQ_OPEN,
  HGFS_REQ_READ,
  HGFS_REQ_WRITE,
  HGFS_REQ_CLOSE,
  HGFS_REQ_OPENDIR,
  HGFS_REQ_READDIR,
  HGFS_REQ_CLOSEDIR,
  HGFS_REQ_GETATTR,
  HGFS_REQ_SETATTR,
  HGFS_REQ_MKDIR,
  HGFS_REQ_UNLINK,
  HGFS_REQ_RMDIR,
  HGFS_REQ_RENAME,
  HGFS_REQ_QUERYVOL
};

/* HGFS open types */
enum {
  HGFS_OPEN_TYPE_O,
  HGFS_OPEN_TYPE_OT,
  HGFS_OPEN_TYPE_CO,
  HGFS_OPEN_TYPE_C,
  HGFS_OPEN_TYPE_COT
};

/* HGFS mode/perms conversion macros */
#define HGFS_MODE_TO_PERM(m) (((m) & S_IRWXU) >> 6)
#define HGFS_PERM_TO_MODE(p) (((p) << 6) & S_IRWXU)

/* HGFS attribute flags */
#define HGFS_ATTR_SIZE		0x01	/* get/set file size */
#define HGFS_ATTR_CRTIME	0x02	/* get/set file creation time */
#define HGFS_ATTR_ATIME		0x04	/* get/set file access time */
#define HGFS_ATTR_MTIME		0x08	/* get/set file modification time */
#define HGFS_ATTR_CTIME		0x10	/* get/set file change time */
#define HGFS_ATTR_MODE		0x20	/* get/set file mode */
#define HGFS_ATTR_ATIME_SET	0x40	/* set specific file access time */
#define HGFS_ATTR_MTIME_SET	0x80	/* set specific file modify time */
