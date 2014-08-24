/* This file contains path component name utility functions.
 *
 * The entry points into this file are:
 *   normalize_name	normalize a path component name for hashing purposes
 *   compare_name	check whether two path component names are equivalent
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

#include <ctype.h>

/*===========================================================================*
 *				normalize_name				     *
 *===========================================================================*/
void normalize_name(char dst[NAME_MAX+1], char *src)
{
/* Normalize the given path component name, storing the result in the given
 * buffer.
 */
  size_t i, size;

  size = strlen(src) + 1;

  assert(size <= NAME_MAX+1);

  if (sffs_params->p_case_insens) {
	for (i = 0; i < size; i++)
		*dst++ = tolower((int)*src++);
  }
  else memcpy(dst, src, size);
}

/*===========================================================================*
 *				compare_name				     *
 *===========================================================================*/
int compare_name(char *name1, char *name2)
{
/* Return TRUE if the given path component names are equivalent, FALSE
 * otherwise.
 */
  int r;

  if (sffs_params->p_case_insens)
	r = strcasecmp(name1, name2);
  else
	r = strcmp(name1, name2);

  return r ? FALSE : TRUE;
}
