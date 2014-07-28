/* system services puts()
 *
 * This is here because gcc converts printf() calls without actual formatting
 * in the format string, to puts() calls. While that "feature" can be disabled
 * with the -fno-builtin-printf gcc flag, we still don't want the resulting
 * mayhem to occur in system servers even when that flag is forgotten.
 */

#include <stdio.h>

/* puts() uses kputc() to print characters. */
void kputc(int c);

int puts(const char *s)
{

	for (; *s; s++)
		kputc(*s);

	kputc('\n');
	kputc('\0');

	return 0;
}
