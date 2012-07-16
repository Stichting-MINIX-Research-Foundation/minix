#include "sysutil.h"
#include <stdlib.h>
#include <env.h>
#include <string.h>
#include <minix/param.h>


/*=========================================================================*
 *				env_parse				   *
 *=========================================================================*/
int env_parse(env, fmt, field, param, min, max)
char *env;		/* environment variable to inspect */
char *fmt;		/* template to parse it with */
int field;		/* field number of value to return */
long *param;		/* address of parameter to get */
long min, max;		/* minimum and maximum values for the parameter */
{
/* Parse an environment variable setting, something like "DPETH0=300:3".
 * Panic if the parsing fails.  Return EP_UNSET if the environment variable
 * is not set, EP_OFF if it is set to "off", EP_ON if set to "on" or a
 * field is left blank, or EP_SET if a field is given (return value through
 * *param).  Punctuation may be used in the environment and format string,
 * fields in the environment string may be empty, and punctuation may be
 * missing to skip fields.  The format string contains characters 'd', 'o',
 * 'x' and 'c' to indicate that 10, 8, 16, or 0 is used as the last argument
 * to strtol().  A '*' means that a field should be skipped.  If the format
 * string contains something like "\4" then the string is repeated 4 characters
 * to the left.
 */
  char *val, *end;
  char value[EP_BUF_SIZE];
  char PUNCT[] = ":,;.";
  long newpar;
  int s, i, radix, r;

  if ((s=env_get_param(env, value, sizeof(value))) != 0) { 
      if (s == ESRCH) return(EP_UNSET);		/* only error allowed */ 
      printf("WARNING: env_get_param() failed in env_parse(): %d\n",s);
      return(EP_EGETKENV);
  }
  val = value;
  if (strcmp(val, "off") == 0) return(EP_OFF);
  if (strcmp(val, "on") == 0) return(EP_ON);

  i = 0;
  r = EP_ON;
  for (;;) {
	while (*val == ' ') val++;	/* skip spaces */
	if (*val == 0) return(r);	/* the proper exit point */
	if (*fmt == 0) break;		/* too many values */

	if (strchr(PUNCT, *val) != NULL) {
		/* Time to go to the next field. */
		if (strchr(PUNCT, *fmt) != NULL) i++;
		if (*fmt++ == *val) val++;
		if (*fmt < 32) fmt -= *fmt;	/* step back? */
	} else {
		/* Environment contains a value, get it. */
		switch (*fmt) {
		case '*':	radix =   -1;	break;
		case 'd':	radix =   10;	break;
		case 'o':	radix =  010;	break;
		case 'x':	radix = 0x10;	break;
		case 'c':	radix =    0;	break;
		default:	goto badenv;
		}
		
		if (radix < 0) {
			/* Skip. */
			while (strchr(PUNCT, *val) == NULL) val++;
			continue;
		} else {
			/* A number. */
			newpar = strtol(val, &end, radix);

			if (end == val) break;	/* not a number */
			val = end;
		}

		if (i == field) {
			/* The field requested. */
			if (newpar < min || newpar > max) break;
			*param = newpar;
			r = EP_SET;
		}
	}
  }
badenv:
  env_panic(env);
  return -1;
}

/*=========================================================================*
 *				env_memory_parse			   *
 *=========================================================================*/

int env_memory_parse(mem_chunks, maxchunks)
struct memory *mem_chunks;	/* where to store the memory bits */
int maxchunks;			/* how many were found */
{
  static kinfo_t kinfo;
  int mm, r;

  if((r=sys_getkinfo(&kinfo)) != OK) return r;

  /* Initialize everything to zero. */
  memset(mem_chunks, 0, maxchunks*sizeof(*mem_chunks));

  for(mm = 0; mm < MAXMEMMAP; mm++) {
  	mem_chunks[mm].base = kinfo.memmap[mm].addr;
  	mem_chunks[mm].size = kinfo.memmap[mm].len;
  }

  return OK;
}
