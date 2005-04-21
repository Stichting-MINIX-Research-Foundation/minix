#include "sysincludes.h"
#include "mtools.h"

#undef got_signal

int got_signal = 0;

void signal_handler(int dummy)
{
	got_signal = 1;
#if 0
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
#endif
}

#if 0
int do_gotsignal(char *f, int n)
{
	if(got_signal)
		fprintf(stderr, "file=%s line=%d\n", f, n);
	return got_signal;
}
#endif

void setup_signal(void)
{
	/* catch signals */
	signal(SIGHUP, (SIG_CAST)signal_handler);
	signal(SIGINT, (SIG_CAST)signal_handler);
	signal(SIGTERM, (SIG_CAST)signal_handler);
	signal(SIGQUIT, (SIG_CAST)signal_handler);
}
