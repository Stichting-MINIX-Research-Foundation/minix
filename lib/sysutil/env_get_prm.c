#include "sysutil.h"
#include <minix/config.h>
#include <string.h>

PRIVATE int argc = 0;
PRIVATE char **argv = NULL;

FORWARD _PROTOTYPE( char *find_key, (const char *params, const char *key));

/*===========================================================================*
 *				env_setargs				     *
 *===========================================================================*/
PUBLIC void env_setargs(arg_c, arg_v)
int arg_c;
char *arg_v[];
{
	argc= arg_c;
	argv= arg_v;
}

/*===========================================================================*
 *				env_get_param				     *
 *===========================================================================*/
PUBLIC int env_get_param(key, value, max_len)
char *key;				/* which key to look up */
char *value;				/* where to store value */
int max_len;				/* maximum length of value */
{
  message m;
  static char mon_params[128*sizeof(char *)];	/* copy parameters here */
  char *key_value;
  int i, s, keylen;

  if (key == NULL)
  	return EINVAL;

  keylen= strlen(key);
  for (i= 1; i<argc; i++)
  {
  	if (strncmp(argv[i], key, keylen) != 0)
  		continue;
	if (strlen(argv[i]) <= keylen)
		continue;
	if (argv[i][keylen] != '=')
		continue;
	key_value= argv[i]+keylen+1;
	if (strlen(key_value)+1 > EP_BUF_SIZE)
	      return(E2BIG);
	strcpy(value, key_value);
	return OK;
  }

  /* Get copy of boot monitor parameters. */
  m.m_type = SYS_GETINFO;
  m.I_REQUEST = GET_MONPARAMS;
  m.I_PROC_NR = SELF;
  m.I_VAL_LEN = sizeof(mon_params);
  m.I_VAL_PTR = mon_params;
  if ((s=_taskcall(SYSTASK, SYS_GETINFO, &m)) != OK) {
	printf("SYS_GETINFO: %d (size %u)\n", s, sizeof(mon_params));
	return(s);
  }

  /* We got a copy, now search requested key. */
  if ((key_value = find_key(mon_params, key)) == NULL)
	return(ESRCH);

  /* Value found, make the actual copy (as far as possible). */
  strncpy(value, key_value, max_len);

  /* See if it fits in the client's buffer. */
  if ((strlen(key_value)+1) > max_len) return(E2BIG);
  return(OK);
}


/*==========================================================================*
 *				find_key					    *
 *==========================================================================*/
PRIVATE char *find_key(params,name)
const char *params;
const char *name;
{
  register const char *namep;
  register char *envp;

  for (envp = (char *) params; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') 
		return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NULL);
}

