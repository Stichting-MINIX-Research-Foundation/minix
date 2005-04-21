/*	asmconv.h - shared functions			Author: Kees J. Bot
 *								19 Dec 1993
 */

#define arraysize(a)	(sizeof(a)/sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))
#define between(a, c, z)	\
			((unsigned)((c) - (a)) <= (unsigned)((z) - (a)))

void *allocate(void *mem, size_t size);
void deallocate(void *mem);
void fatal(char *label);
char *copystr(const char *s);
int isanumber(const char *s);

extern int asm_mode32;	/* In 32 bit mode if true. */

#define use16()		(!asm_mode32)
#define use32()		((int) asm_mode32)
#define set_use16()	((void) (asm_mode32= 0))
#define set_use32()	((void) (asm_mode32= 1))

extern int err_code;	/* Exit code. */
#define set_error()	((void) (err_code= EXIT_FAILURE))
