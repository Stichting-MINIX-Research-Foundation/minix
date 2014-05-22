#include "sysutil.h"
#include <string.h>

int env_argc = 0;
char **env_argv = NULL;

static char *find_key(const char *params, const char *key);

/*===========================================================================*
 *				env_setargs				     *
 *===========================================================================*/
void env_setargs(int arg_c, char *arg_v[])
{
	env_argc= arg_c;
	env_argv= arg_v;
}

/*===========================================================================*
 *				env_get_param				     *
 *===========================================================================*/
int env_get_param(const char *key, char *value, int max_len)
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
  m.m_lsys_krn_sys_getinfo.request = GET_MONPARAMS;
  m.m_lsys_krn_sys_getinfo.endpt = SELF;
  m.m_lsys_krn_sys_getinfo.val_len = sizeof(mon_params);
  m.m_lsys_krn_sys_getinfo.val_ptr = mon_params;
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
static char *find_key(const char *params, const char *name)
{
  const char *namep;
  char *envp;

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

