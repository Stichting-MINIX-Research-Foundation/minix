
/* Structure with global file system state. */
struct state {
  int mounted;			/* is the file system mounted? */
  int read_only;		/* is the file system mounted read-only? note,
				 * has no relation to the shared folder mode */
  dev_t dev;			/* device the file system is mounted on */
};

/* Structure with options affecting global behavior. */
struct opt {
  char prefix[PATH_MAX];	/* prefix for all paths used */
  uid_t uid;			/* UID that owns all files */
  gid_t gid;			/* GID that owns all files */
  unsigned int file_mask;	/* AND-mask to apply to file permissions */
  unsigned int dir_mask;	/* AND-mask to apply to directory perm's */
  int case_insens;		/* case insensitivity flag; has no relation
				 * to the hosts's shared folder naming */
};
