#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>

void _loadname(const char *name, message *msgptr)
{
/* This function is used to load a string into a type m3 message. If the
 * string fits in the message, it is copied there.  If not, a pointer to
 * it is passed.
 */
  register size_t k;

  k = strlen(name) + 1;
  msgptr->m_lc_vfs_path.len = k;
  msgptr->m_lc_vfs_path.name = (vir_bytes)name;
  if (k <= M_PATH_STRING_MAX) strcpy(msgptr->m_lc_vfs_path.buf, name);
}
