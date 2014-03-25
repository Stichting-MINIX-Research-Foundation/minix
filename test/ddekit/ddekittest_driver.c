#include <ddekit/ddekit.h>
#include <ddekit/printf.h>
#include <ddekit/thread.h>
#include <ddekit/initcall.h>

#include <stdio.h>

void
long_running_thread()
{
	int x=10;
	do {
		ddekit_printf("Long Running\n");
		ddekit_thread_msleep(2000);
		x--;
	} while(x >0);
}

void
short_running_thread()
{
	int x=15;
	do {
		ddekit_printf("Short Running\n");
		ddekit_thread_msleep(500);
		x--;
	} while(x >0);
}

void ddekit_minix_wait_exit(void);	/* import from dde-minix */

#if 0
#include <ucontext.h>
ucontext_t ctx;
#endif

int
main(void)
{

#if 0
	getcontext(&ctx);
	if (ctx.uc_mcontext.mc_magic != 0xc0ffee) {
		printf("FLAG_NONE\n");
	}

	memset(&ctx,0,sizeof(ucontext_t));
	ctx.uc_flags = _UC_IGNSIGM | _UC_IGNFPU;
	getcontext(&ctx);
	if (ctx.uc_mcontext.mc_magic != 0xc0ffee) {
		printf("_UC_IGNSIGM | _UC_IGNFPU FAIL\n");
	}

	memset(&ctx,0,sizeof(ucontext_t));
	ctx.uc_flags = _UC_IGNSIGM ;
	getcontext(&ctx);
	if (ctx.uc_mcontext.mc_magic != 0xc0ffee) {
		printf("_UC_IGNSIGM FAIL\n");
	}

	memset(&ctx,0,sizeof(ucontext_t));
	ctx.uc_flags = _UC_IGNFPU ;
	getcontext(&ctx);
	if (ctx.uc_mcontext.mc_magic != 0xc0ffee) {
		printf("_UC_IGNFPU FAIL\n");
	}
#endif

	ddekit_init();
	ddekit_thread_create(long_running_thread, NULL, "Long_thread");
	ddekit_thread_create(short_running_thread, NULL, "Short_thread");
	ddekit_minix_wait_exit();

	return 0;
}
