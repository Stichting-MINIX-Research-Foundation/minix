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
#include <limits.h>
#include <errno.h>

#include <minix/const.h>
#include <minix/com.h>
#include <minix/type.h>
#include <minix/callnr.h>

#include <minix/ipc.h>

struct ps_strings; /* forward declaration for minix_stack_fill. */

void minix_stack_params(const char *path, char * const *argv, char * const *envp,
	size_t *stack_size,  char *overflow, int *argc, int *envc);
void minix_stack_fill(const char *path, int argc, char * const *argv,
	int envc, char * const *envp, size_t stack_size, char *frame,
	int *vsp, struct ps_strings **psp);

int __execve(const char *_path, char *const _argv[], char *const
	_envp[], int _nargs, int _nenvps);
int _syscall(endpoint_t _who, int _syscallnr, message *_msgptr);
void _loadname(const char *_name, message *_msgptr);
int _len(const char *_s);
void _begsig(int _dummy);

int getprocnr(void);
int getnprocnr(pid_t pid);
int getpprocnr(void);
int _pm_findproc(char *proc_name, int *proc_nr);
int mapdriver(char *label, int major, int style, int flags);
pid_t getnpid(endpoint_t proc_ep);
uid_t getnuid(endpoint_t proc_ep);
gid_t getngid(endpoint_t proc_ep);
ssize_t pread64(int fd, void *buf, size_t count, u64_t where);
ssize_t pwrite64(int fd, const void *buf, size_t count, u64_t where);

#endif /* _LIB_H */
