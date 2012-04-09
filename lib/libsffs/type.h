#ifndef _SFFS_TYPE_H
#define _SFFS_TYPE_H

/* Structure with global file system state. */
struct state {
  int s_mounted;		/* is the file system mounted? */
  int s_signaled;		/* have we received a SIGTERM? */
  int s_read_only;		/* is the file system mounted read-only? note,
				 * has no relation to the shared folder mode */
  dev_t s_dev;			/* device the file system is mounted on */
};

#endif /* _SFFS_TYPE_H */
