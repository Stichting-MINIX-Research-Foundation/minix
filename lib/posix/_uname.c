/*	uname() - get system info			Author: Kees J. Bot
 *								7 Nov 1994
 * Returns information about the Minix system.  Alas most
 * of it is gathered at compile time, so machine is wrong, and
 * release and version become wrong if not recompiled.
 * More chip types and Minix versions need to be added.
 */
#define uname	_uname
#define open	_open
#define read	_read
#define close	_close
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <minix/config.h>
#include <minix/minlib.h>

int uname(name) struct utsname *name;
{
  int hf, n, err;
  char *nl;

  /* Read the node name from /etc/hostname.file. */
  if ((hf = open("/etc/hostname.file", O_RDONLY)) < 0) {
	if (errno != ENOENT) return(-1);
	strcpy(name->nodename, "noname");
  } else {
	n = read(hf, name->nodename, sizeof(name->nodename) - 1);
	err = errno;
	close(hf);
	errno = err;
	if (n < 0) return(-1);
	name->nodename[n] = 0;
	if ((nl = strchr(name->nodename, '\n')) != NULL) {
		memset(nl, 0, (name->nodename + sizeof(name->nodename)) - nl);
	}
  }

  strcpy(name->sysname, "Minix");
  strcpy(name->release, OS_RELEASE);
  strcpy(name->version, OS_VERSION);
#if (CHIP == INTEL)
  name->machine[0] = 'i';
  strcpy(name->machine + 1, itoa(getprocessor()));
#if _WORD_SIZE == 4
  strcpy(name->arch, "i386");
#else
  strcpy(name->arch, "i86");
#endif
#endif
  return(0);
}
