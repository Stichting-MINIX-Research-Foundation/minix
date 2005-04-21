#include "utils.h"
#include <minix/config.h>
#include <string.h>

FORWARD _PROTOTYPE( char *find_key, (const char *params, const char *key));

/*===========================================================================*
 *				get_mon_param				     *
 *===========================================================================*/
PUBLIC int get_mon_param(key, value, max_len)
char *key;				/* which key to look up */
char *value;				/* where to store value */
int max_len;				/* maximum length of value */
{
  message m;
  static char mon_params[128*sizeof(char *)];	/* copy parameters here */
  char *key_value;
  int s;

  if (key != NULL) {	
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
  return(EINVAL);
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

