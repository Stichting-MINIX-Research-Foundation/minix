#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <minix/sysutil.h>

#include "syslib.h"

void panic_hook(void);

__weak_alias(panic_hook, __panic_hook);

void __panic_hook(void)
{
	;
}

/*===========================================================================*
 *				panic				     *
 *===========================================================================*/
void panic(const char *fmt, ...)
{
/* Something awful has happened. Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal 
 * value of a defined constant.
 */
  endpoint_t me = NONE;
  char name[20];
  char diag[512];
  int priv_flags;
  int init_flags;
  int diag_len, n;
  void (*suicide)(void);
  va_list args;

  if(sys_whoami(&me, name, sizeof(name), &priv_flags, &init_flags) == OK && me != NONE)
	diag_len = snprintf(diag, sizeof(diag), "%s(%d): panic: ", name, me);
  else
	diag_len = snprintf(diag, sizeof(diag), "(sys_whoami failed): panic: ");

  if(diag_len < 0)
	diag_len = 0;
  if(diag_len >= (int)sizeof(diag))
	diag_len = sizeof(diag) - 1;

  if(fmt) {
	va_start(args, fmt);
	n = vsnprintf(diag + diag_len, sizeof(diag) - diag_len, fmt, args);
	va_end(args);
  } else {
	n = snprintf(diag + diag_len, sizeof(diag) - diag_len, "no message");
  }
  if(n < 0)
	n = 0;
  if(n >= (int)(sizeof(diag) - diag_len))
	n = sizeof(diag) - diag_len - 1;
  diag_len += n;

  if(diag_len >= (int)sizeof(diag) - 1)
	diag_len = sizeof(diag) - 2;
  diag[diag_len++] = '\n';
  diag[diag_len] = '\0';

  /*
   * Keep service panics visible even before the kernel panics as a result
   * of SYS_EXIT.  SYS_DIAGCTL_DIAG records the text in the kernel message
   * buffer; on x86_64 the kernel mirrors that diagnostic to COM1 as well.
   * Ignore errors here: a failed diagnostic must not recurse into panic().
   */
  (void)sys_diagctl_diag(diag, diag_len);
  printf("%s", diag);

  printf("syslib:panic.c: stacktrace: ");
  util_stacktrace();

  panic_hook();

  /* Try exit */
  _exit(1);

  /* Try to signal ourself */
  abort();

  /* If exiting nicely through PM fails for some reason, try to
   * commit suicide. E.g., message to PM might fail due to deadlock.
   */
  suicide = (void (*)(void)) -1;
  suicide();

  /* If committing suicide fails for some reason, hang. */
  for(;;) { }
}

