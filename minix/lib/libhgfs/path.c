/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

#include <limits.h>

/*===========================================================================*
 *				path_put				     *
 *===========================================================================*/
void path_put(const char *path)
{
/* Append the given path name in HGFS format to the RPC buffer. Truncate it
 * if it is longer than PATH_MAX bytes.
 */
  const char *p;
  char buf[PATH_MAX];
  unsigned int len;

  /* No leading slashes are allowed. */
  for (p = path; *p == '/'; p++);

  /* No double or tailing slashes, either. */
  for (len = 0; *p && len < sizeof(buf) - 1; len++) {
    if (*p == '/') {
      for (p++; *p == '/'; p++);

      if (!*p) break;

      buf[len] = 0;
    }
    else buf[len] = *p++;
  }

  RPC_NEXT32 = len;

  memcpy(RPC_PTR, buf, len);
  RPC_ADVANCE(len);

  RPC_NEXT8 = 0;
}

/*===========================================================================*
 *				path_get				     *
 *===========================================================================*/
int path_get(char *path, int max)
{
/* Retrieve a HGFS formatted path name from the RPC buffer. Returns EINVAL if
 * the path name is invalid. Returns ENAMETOOLONG if the path name is too
 * long. Returns OK on success.
 */
  char *p, *q;
  int n, len;

  n = len = RPC_NEXT32;

  if (len >= max) return ENAMETOOLONG;

  for (p = path, q = RPC_PTR; n--; p++, q++) {
    /* We can not deal with a slash in a path component. */
    if (*q == '/') return EINVAL;

    if (*q == 0) *p = '/';
    else *p = *q;
  }

  RPC_ADVANCE(len);

  *p = 0;

  return (RPC_NEXT8 != 0) ? EINVAL : OK;
}
