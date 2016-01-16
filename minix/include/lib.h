/* The <lib.h> header is the master header used by the library.
 * All the C files in the lib subdirectories include it.
 */

#ifndef _LIB_H
#define _LIB_H

/* First come the defines. */
#include <sys/featuretest.h>	/* tell headers to include NetBSD stuff. */

/* The following are so basic, all the lib files get them automatically. */
#include <minix/config.h>	/* must be first */
#include <sys/types.h>
#include <sys/uio.h>
#include <limits.h>
#include <errno.h>

#include <minix/const.h>
#include <minix/com.h>
#include <minix/type.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/ipc.h>

struct minix_kerninfo *get_minix_kerninfo(void);

vir_bytes minix_get_user_sp(void);

struct ps_strings; /* forward declaration for minix_stack_fill. */

void minix_stack_params(const char *path, char * const *argv,
	char * const *envp, size_t *stack_size, char *overflow, int *argc,
	int *envc);
void minix_stack_fill(const char *path, int argc, char * const *argv,
	int envc, char * const *envp, size_t stack_size, char *frame,
	int *vsp, struct ps_strings **psp);

int __execve(const char *_path, char *const _argv[], char *const
	_envp[], int _nargs, int _nenvps);
int _syscall(endpoint_t _who, int _syscallnr, message *_msgptr);
void _loadname(const char *_name, message *_msgptr);
int _len(const char *_s);
void _begsig(int _dummy);

#define _VECTORIO_READ	1
#define _VECTORIO_WRITE	2
ssize_t _vectorio_setup(const struct iovec * iov, int iovcnt, char ** ptr,
	int op);
void _vectorio_cleanup(const struct iovec * iov, int iovcnt, char * buffer,
	ssize_t r, int op);

#endif /* _LIB_H */
