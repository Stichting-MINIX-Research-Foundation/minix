#include <lib.h>
#include <sys/sigcontext.h>
#include <setjmp.h>

PUBLIC void siglongjmp(env, val)
sigjmp_buf env;
int val;
{
  if (env[0].__flags & SC_SIGCONTEXT)
	longjmp(env, val);
  else
	_longjmp(env, val);
}
