#include "sysutil.h"
#include <string.h>

int env_argc = 0;
char **env_argv = NULL;

static char *find_key(const char *params, const char *key);

/*===========================================================================*
 *				env_setargs				     *
 *===========================================================================*/
void env_setargs(arg_c, arg_v)
int arg_c;
char *arg_v[];
{
	env_argc= arg_c;
	env_argv= arg_v;
}

/*===========================================================================*
 *				env_get_param				     *
 *===========================================================================*/
int env_get_param(key, value, max_len)
char *key;				/* which key to look up */
char *value;				/* where to store value */
int max_len;				/* maximum length of value */
{
  message m;
  static char mon_params[MULTIBOOT_PARAM_BUF_SIZE]; /* copy parameters here */
  char *key_value;
  int i, s;
  size_t keylen;

  if (key == NULL)
  	return EINVAL;

  keylen= strlen(key);
  for (i= 1; i<env_argc; i++)
  {
  	if (strncmp(env_argv[i], key, keylen) != 0)
  		continue;
	if (strlen(env_argv[i]) <= keylen)
		continue;
	if (env_argv[i][keylen] != '=')
		continue;
	key_value= env_argv[i]+keylen+1;
	if (strlen(key_value)+1 > (size_t) max_len)
	      return(E2BIG);
	strcpy(value, key_value);
	return OK;
  }

  /* Get copy of boot monitor parameters. */
  m.m_type = SYS_GETINFO;
  m.I_REQUEST = GET_MONPARAMS;
  m.I_ENDPT = SELF;
  m.I_VAL_LEN = sizeof(mon_params);
  m.I_VAL_PTR = mon_params;
  if ((s=_kernel_call(SYS_GETINFO, &m)) != OK) {
	printf("SYS_GETINFO: %d (size %u)\n", s, sizeof(mon_params));
	return(s);
  }

  /* We got a copy, now search requested key. */
  if ((key_value = find_key(mon_params, key)) == NULL)
	return(ESRCH);

  /* Value found, see if it fits in the client's buffer. Callers assume that
   * their buffer is unchanged on error, so don't make a partial copy.
   */
  if ((strlen(key_value)+1) > (size_t) max_len) return(E2BIG);

  /* Make the actual copy. */
  strcpy(value, key_value);

  return(OK);
}


/*==========================================================================*
 *				find_key					    *
 *==========================================================================*/
static char *find_key(params,name)
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

