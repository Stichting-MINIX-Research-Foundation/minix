/* sys/signalfd.h - Linux-compatible signalfd, implemented in MINIX libc.
 *
 * Semantics: signals in the mask are caught by a libc-internal handler
 * (SA_RESTART) that writes a signalfd_siginfo record into a pipe - the
 * process never sees the default action.  The struct layout below matches
 * Linux field-for-field (128 bytes).
 *
 * $MINIX$
 */
#ifndef _SYS_SIGNALFD_H_
#define _SYS_SIGNALFD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <signal.h>
#include <stdint.h>

struct signalfd_siginfo {
	uint32_t ssi_signo;	/* signal number */
	int32_t  ssi_errno;	/* unused */
	int32_t  ssi_code;	/* unused (SI_USER) */
	uint32_t ssi_pid;	/* 0 (signal source tracked by caller) */
	uint32_t ssi_uid;	/* 0 */
	int32_t  ssi_fd;	/* 0 */
	uint32_t ssi_tid;	/* 0 */
	uint32_t ssi_band;	/* 0 */
	uint32_t ssi_overrun;	/* 0 */
	uint32_t ssi_trapno;	/* 0 */
	int32_t  ssi_status;	/* exit status for SIGCHLD (0 here) */
	int32_t  ssi_int;	/* 0 */
	uint64_t ssi_ptr;	/* 0 */
	uint64_t ssi_utime;	/* 0 */
	uint64_t ssi_stime;	/* 0 */
	uint64_t ssi_addr;	/* 0 */
	uint16_t ssi_addr_lsb;	/* 0 */
	uint16_t __pad2;
	int32_t  ssi_syscall;	/* 0 */
	uint64_t ssi_call_addr;	/* 0 */
	uint32_t ssi_arch;	/* 0 */
	uint8_t  pad[28];	/* pad to 128 bytes (Linux layout) */
};

#define SFD_CLOEXEC	02000000
#define SFD_NONBLOCK	04000

__BEGIN_DECLS
int	signalfd(int __fd, const sigset_t *__mask, int __flags);
__END_DECLS

#endif /* _SYS_SIGNALFD_H_ */
