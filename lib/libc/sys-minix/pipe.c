#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(pipe, _pipe)
__weak_alias(pipe2, _pipe2)
#endif

int
pipe(int fild[2])
{
	message m;

	if (_syscall(VFS_PROC_NR, PIPE, &m) < 0) return(-1);
	fild[0] = m.m1_i1;
	fild[1] = m.m1_i2;
	return(0);
}

int
pipe2(int fild[2], int flags)
{
	message m;

	m.m1_i3 = flags;

	if (_syscall(VFS_PROC_NR, PIPE2, &m) < 0) return(-1);
	fild[0] = m.m1_i1;
	fild[1] = m.m1_i2;
	return(0);
}
