#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static ssize_t __getdents321(int fd, char *buffer, size_t nbytes);

ssize_t getdents(int fd, char *buffer, size_t nbytes)
{
  message m;
  int r, orig_errno;

  orig_errno = errno;
  m.m1_i1 = fd;
  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) buffer;
  r = _syscall(VFS_PROC_NR, GETDENTS, &m);
  if (r == -1 && errno == ENOSYS) {
	errno = orig_errno;/* Restore old value so world is still as expected*/
	r = __getdents321(fd, buffer, nbytes);
  }

  return r;
}

ssize_t __getdents321(int fd, char *buffer, size_t nbytes)
{
  message m;
  int r, consumed = 0, newconsumed = 0;
  char *intermediate = NULL;
  struct dirent *dent;
  struct dirent_321 *dent_321;
#define DWORD_ALIGN(d) if((d) % sizeof(long)) (d)+=sizeof(long)-(d)%sizeof(long)

  intermediate = malloc(nbytes);
  if (intermediate == NULL) return EINVAL;

  m.m1_i1 = fd;
  /* Pretend the buffer is smaller so we know the converted/expanded version
   * will fit.
   */
  nbytes = nbytes / 2;
  if (nbytes < (sizeof(struct dirent) + NAME_MAX + 1)) {
	free(intermediate);
	return EINVAL;	/* This might not fit. Sorry */
  }

  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) intermediate;
  r = _syscall(VFS_PROC_NR, GETDENTS_321, &m);

  if (r <= 0) {
	free(intermediate);
	return r;
  }

  /* Provided format is struct dirent_321 and has to be translated to
   * struct dirent */
  dent_321 = (struct dirent_321 *) intermediate;
  dent     = (struct dirent *)     buffer;

  while (consumed < r && dent_321->d_reclen > 0) {
	dent->d_ino = (ino_t) dent_321->d_ino;
	dent->d_off = (off_t) dent_321->d_off;
	dent->d_reclen = offsetof(struct dirent, d_name) +
			 strlen(dent_321->d_name) + 1;
	DWORD_ALIGN(dent->d_reclen);
	strcpy(dent->d_name, dent_321->d_name);
	consumed += dent_321->d_reclen;
	newconsumed += dent->d_reclen;
	dent_321 = (struct dirent_321 *) &intermediate[consumed];
	dent     = (struct dirent *)     &buffer[newconsumed];
  }

  free(intermediate);

  return newconsumed;
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getdents, __getdents30)
#endif
