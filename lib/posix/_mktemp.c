/* mktemp - make a name for a temporary file */

#include <lib.h>
#define access	_access
#define getpid	_getpid
#define mktemp	_mktemp
#include <unistd.h>

PUBLIC char *mktemp(template)
char *template;
{
  register int k;
  register char *p;
  register pid_t pid;

  pid = getpid();		/* get process id as semi-unique number */
  p = template;
  while (*p != 0) p++;		/* find end of string */

  /* Replace XXXXXX at end of template with a letter, then as many of the
   * trailing digits of the pid as fit.
   */
  while (*--p == 'X') {
	*p = '0' + (pid % 10);
	pid /= 10;
  }
  if (*++p != 0) {
	for (k = 'a'; k <= 'z'; k++) {
		*p = k;
		if (access(template, F_OK) < 0) return(template);
	}
  }
  return("/");
}
