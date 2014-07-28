/* This file provides functionality to parse strings of comma-separated
 * options, each being either a single key name or a key=value pair, where the
 * value may be enclosed in quotes. A table of optset entries is provided to
 * determine which options are recognized, how to parse their values, and where
 * to store those. Unrecognized options are silently ignored; improperly
 * formatted options are silently set to reasonably acceptable values.
 *
 * The entry points into this file are:
 *   optset_parse	parse the given options string using the given table
 *
 * Created:
 *   May 2009 (D.C. van Moolenbroek)
 */

#include <stdlib.h>
#include <string.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/optset.h>

static void optset_parse_entry(struct optset *entry, char *ptr, int len);

/*===========================================================================*
 *				optset_parse_entry			     *
 *===========================================================================*/
static void optset_parse_entry(struct optset *entry, char *ptr, int len)
{
/* Parse and store the value of a single option.
 */
  char *dst;
  int val;

  switch (entry->os_type) {
  case OPT_BOOL:
	*((int *) entry->os_ptr) = entry->os_val;

	break;

  case OPT_STRING:
	if (len >= entry->os_val)
		len = entry->os_val - 1;

	dst = (char *) entry->os_ptr;

	if (len > 0)
		memcpy(dst, ptr, len);
	dst[len] = 0;

	break;

  case OPT_INT:
	if (len > 0)
		val = (int) strtoul(ptr, NULL, entry->os_val);
	else
		val = 0;

	*((int *) entry->os_ptr) = val;

	break;
  }
}

/*===========================================================================*
 *				optset_parse				     *
 *===========================================================================*/
void optset_parse(struct optset *table, char *string)
{
/* Parse a string of options, using the provided table of optset entries.
 */
  char *p, *kptr, *vptr;
  int i, klen, vlen;

  for (p = string; *p; ) {
	/* Get the key name for the field. */
	for (kptr = p, klen = 0; *p && *p != '=' && *p != ','; p++, klen++);

	if (*p == '=') {
		/* The field has an associated value. */
		vptr = ++p;

		/* If the first character after the '=' is a quote character,
		 * find a matching quote character followed by either a comma
		 * or the terminating null character, and use the string in
		 * between. Otherwise, use the string up to the next comma or
		 * the terminating null character.
		 */
		if (*p == '\'' || *p == '"') {
			p++;

			for (vlen = 0; *p && (*p != *vptr ||
				(p[1] && p[1] != ',')); p++, vlen++);

			if (*p) p++;
			vptr++;
		}
		else
			for (vlen = 0; *p && *p != ','; p++, vlen++);
	}
	else {
		vptr = NULL;
		vlen = 0;
	}

	if (*p == ',') p++;

	/* Find a matching entry for this key in the given table. If found,
	 * call optset_parse_entry() on it. Silently ignore the option
	 * otherwise.
	 */
	for (i = 0; table[i].os_name != NULL; i++) {
		if ((int) strlen(table[i].os_name) == klen &&
			!strncasecmp(table[i].os_name, kptr, klen)) {

			optset_parse_entry(&table[i], vptr, vlen);

			break;
		}
	}
  }
}
