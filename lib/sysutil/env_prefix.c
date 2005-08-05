#include "sysutil.h"
#include <stdlib.h>
#include <string.h>

/*=========================================================================*
 *				env_prefix				   *
 *=========================================================================*/
PUBLIC int env_prefix(env, prefix)
char *env;		/* environment variable to inspect */
char *prefix;		/* prefix to test for */
{
	return env_prefix_x(0, NULL, env, prefix);
}


/*=========================================================================*
 *				env_prefix_x				   *
 *=========================================================================*/
PUBLIC int env_prefix_x(argc, argv, env, prefix)
int argc;
char *argv[];
char *env;		/* environment variable to inspect */
char *prefix;		/* prefix to test for */
{
/* An environment setting may be prefixed by a word, usually "pci".  
 * Return TRUE if a given prefix is used.
 */
  char value[EP_BUF_SIZE];
  char punct[] = ":,;.";
  int i, s, keylen;
  char *val;
  size_t n;

  keylen= strlen(env);
  for (i= 0; i<argc; i++)
  {
  	printf("env_prefix_x: argv[%d] = '%s'\n", i, argv[i]);
  	if (strncmp(argv[i], env, keylen) != 0)
  		continue;
	if (strlen(argv[i]) <= keylen)
		continue;
	if (argv[i][keylen] != '=')
		continue;
	val= argv[i]+keylen+1;
	if (strlen(val)+1 > EP_BUF_SIZE)
	{
	      printf("WARNING: env_parse() failed: argument too long\n");
	      return(EP_EGETKENV);
	}
	strcpy(value, val);
  }

  if (i >= argc && (s = get_mon_param(env, value, sizeof(value))) != 0) {
  	if (s != ESRCH)		/* only error allowed */
  	printf("WARNING: get_mon_param() failed in env_prefix(): %d\n", s);	
  }
  n = strlen(prefix);
  return(value != NULL
	&& strncmp(value, prefix, n) == 0
	&& strchr(punct, value[n]) != NULL);
}

