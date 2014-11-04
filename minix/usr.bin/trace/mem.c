
#include "inc.h"

/*
 * Retrieve 'len' bytes from the memory of the traced process 'pid' at address
 * 'addr' and put the result in the buffer pointed to by 'ptr'.  Return 0 on
 * success, or otherwise -1 with errno set appropriately.
 */
int
mem_get_data(pid_t pid, vir_bytes addr, void * ptr, size_t len)
{
	struct ptrace_range pr;

	if (len == 0) return 0;

	pr.pr_space = TS_DATA;
	pr.pr_addr = addr;
	pr.pr_size = len;
	pr.pr_ptr = ptr;

	return ptrace(T_GETRANGE, pid, &pr, 0);
}

/*
 * Retrieve 'len' bytes from the kernel structure memory of the traced process
 * 'pid' at offset 'addr' and put the result in the buffer pointed to by 'ptr'.
 * Return 0 on success, or otherwise -1 with errno set appropriately.
 */
int
mem_get_user(pid_t pid, vir_bytes addr, void * ptr, size_t len)
{
	long data;
	char *p;
	size_t off, chunk;

	if (len == 0) return 0;

	/* Align access to address. */
	off = addr & (sizeof(data) - 1);
	addr -= off;

	p = ptr;

	while (len > 0) {
		errno = 0;
		data = ptrace(T_GETUSER, pid, (void *)addr, 0);
		if (errno != 0) return -1;

		chunk = sizeof(data) - off;
		if (chunk > len)
			chunk = len;

		memcpy(p, (char *)&data + off, chunk);
		p += chunk;
		addr += chunk;
		len -= chunk;
		off = 0;
	}

	return 0;
}
