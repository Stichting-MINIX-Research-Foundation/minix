/*	execve() - basic program execution call		Author: Kees J. Bot
 *								21 Jan 1994
 */
#define _MINIX_SOURCE

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <sys/exec_elf.h>

#ifdef __weak_alias
__weak_alias(execve, _execve)
#endif

int execve(const char *path, char * const *argv, char * const *envp)
{
	char * const *ap;
	char * const *ep;
	char *frame;
	char **vp;
	char *sp;
	size_t argc;
	int extra;
	int vectors;
	size_t frame_size;
	size_t string_off;
	size_t n;
	int ov;
	message m;

	/* Assumptions: size_t and char *, it's all the same thing. */

	/* Create a stack image that only needs to be patched up slightly
	 * by the kernel to be used for the process to be executed.
	 */

	ov= 0;			/* No overflow yet. */
	frame_size= 0;		/* Size of the new initial stack. */
	string_off= 0;		/* Offset to start of the strings. */
	argc= 0;		/* Argument count. */

	for (ap= argv; *ap != NULL; ap++) {
		n = sizeof(*ap) + strlen(*ap) + 1;
		frame_size+= n;
		if (frame_size < n) ov= 1;
		string_off+= sizeof(*ap);
		argc++;
	}

	for (ep= envp; *ep != NULL; ep++) {
		n = sizeof(*ep) + strlen(*ep) + 1;
		frame_size+= n;
		if (frame_size < n) ov= 1;
		string_off+= sizeof(*ap);
	}

	/* Add an argument count, two terminating nulls and
	 * space for the ELF aux vectors, that must come before
	 * (i.e. at a higher address) then the strings.
	 */
	vectors = sizeof(argc) + sizeof(*ap) + sizeof(*ep) +
		sizeof(AuxInfo) * PMEF_AUXVECTORS;
	extra = vectors + PMEF_EXECNAMELEN1;
	frame_size+= extra;
	string_off+= extra;

	/* Align. */
	frame_size= (frame_size + sizeof(char *) - 1) & ~(sizeof(char *) - 1);

	/* The party is off if there is an overflow. */
	if (ov || frame_size < 3 * sizeof(char *)) {
		errno= E2BIG;
		return -1;
	}

	/* Allocate space for the stack frame. */
	if ((frame = (char *) sbrk(frame_size)) == (char *) -1) {
		errno = E2BIG;
		return -1;
	}

	/* Set arg count, init pointers to vector and string tables. */
	* (size_t *) frame = argc;
	vp = (char **) (frame + sizeof(argc));
	sp = frame + string_off;

	/* Load the argument vector and strings. */
	for (ap= argv; *ap != NULL; ap++) {
		*vp++= (char *) (sp - frame);
		n= strlen(*ap) + 1;
		memcpy(sp, *ap, n);
		sp+= n;
	}
	*vp++= NULL;

	/* Load the environment vector and strings. */
	for (ep= envp; *ep != NULL; ep++) {
		*vp++= (char *) (sp - frame);
		n= strlen(*ep) + 1;
		memcpy(sp, *ep, n);
		sp+= n;
	}
	*vp++= NULL;

	/* Padding. */
	while (sp < frame + frame_size) *sp++= 0;

	/* Clear unused message fields */
	memset(&m, 0, sizeof(m));

	/* We can finally make the system call. */
	m.m1_i1 = strlen(path) + 1;
	m.m1_i2 = frame_size;
	m.m1_p1 = (char *) __UNCONST(path);
	m.m1_p2 = frame;

	/* Tell PM/VFS we have left space for the aux vectors
	 * and executable name
	 */
	m.PMEXEC_FLAGS = PMEF_AUXVECTORSPACE | PMEF_EXECNAMESPACE1;

	(void) _syscall(PM_PROC_NR, EXEC, &m);

	/* Failure, return the memory used for the frame and exit. */
	(void) sbrk(-frame_size);
	return -1;
}
