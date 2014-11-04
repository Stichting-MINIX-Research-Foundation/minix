
#include "inc.h"

#include <sys/svrctl.h>

const char *
svrctl_name(unsigned long req)
{

	switch (req) {
	NAME(PMSETPARAM);
	NAME(PMGETPARAM);
	NAME(VFSGETPARAM);
	NAME(VFSSETPARAM);
	}

	return NULL;
}

int
svrctl_arg(struct trace_proc * proc, unsigned long req, void * ptr, int dir)
{
	struct sysgetenv *env;

	switch (req) {
	case PMSETPARAM:
	case VFSSETPARAM:
		if ((env = (struct sysgetenv *)ptr) == NULL)
			return IF_OUT;

		put_buf(proc, "key", PF_STRING, (vir_bytes)env->key,
		    env->keylen);
		put_buf(proc, "value", PF_STRING, (vir_bytes)env->val,
		    env->vallen);
		return IF_ALL;

	case PMGETPARAM:
	case VFSGETPARAM:
		if ((env = (struct sysgetenv *)ptr) == NULL)
			return IF_OUT | IF_IN;

		/*
		 * So far this is the only IOCTL case where the output depends
		 * on one of the values in the input: if the given key is NULL,
		 * PM provides the entire system environment in return, which
		 * means we cannot just print a single string.  We rely on PM
		 * not changing the key field, which (while true) is an
		 * assumption.  With the current (simple) model we would have
		 * to save the provided key pointer somewhere otherwise.
		 */
		if (dir == IF_OUT)
			put_buf(proc, "key", PF_STRING, (vir_bytes)env->key,
			    env->keylen);
		else
			put_buf(proc, "value",
			    (env->key != NULL) ? PF_STRING : 0,
			    (vir_bytes)env->val, env->vallen);
		return IF_ALL;

	default:
		return 0;
	}
}
