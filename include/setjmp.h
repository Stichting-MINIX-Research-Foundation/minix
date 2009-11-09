/* The <setjmp.h> header relates to the C phenomenon known as setjmp/longjmp.
 * It is used to escape out of the current situation into a previous one.
 *
 * The actual implementations of all these functions and, by extension, parts
 * of this header, are both compiler- and architecture-dependent. Currently
 * two compilers are supported: ACK and GCC. Each of these has their own
 * implementation of the functions in their runtime system library code.
 * As it is impossible to predict the requirements for any other compilers,
 * this header may not be compatible with such other compilers either.
 *
 * The ACK compiler will not enregister any variables inside a function
 * containing a setjmp call, even if those variables are explicitly declared
 * as register variables. Thus for ACK, of all the registers, only the
 * program counter, stack pointer and frame pointer have to be saved into the
 * jmp_buf structure. This makes the jmp_buf structure very small, and
 * moreover, the implementation of the setjmp/longjmp calls (written in EM)
 * architecture-independent and thus very portable. The ACK compiler
 * recognizes only the symbol __setjmp as being such a setjmp call.
 *
 * The GCC compiler recognizes all of the setjmp/_setjmp/__setjmp name
 * variants as calls to setjmp functions, and treats them as special
 * accordingly, but does require that the setjmp implementation save and
 * restore most of the registers. It has no portable setjmp and longjmp
 * functions like ACK, and therefore has to have enough space in the jmp_buf
 * structure to store the registers on any architecture it's ported to.
 *
 * Taking the common denominator of both compilers, the function definitions
 * in this header rely on the presence of merely two functions: __setjmp and
 * longjmp. On the other hand, the size of jmp_buf depends on the compiler
 * used: for ACK, jmp_buf is exactly big enough to store the three mentioned
 * registers; for GCC and any other compiler, the size is chosen in such a
 * way that it's likely to offer enough room to store registers for any
 * architecture. The POSIX sigjmp_buf is identical to jmp_buf in all cases.
 *
 * As far as porting is concerned --
 *
 * All code writers/porters that have to deal with the actual contents of the
 * jmp_buf structure in one way or another, should look at  <sys/jmp_buf.h>.
 *
 * Porters of a new compiler to Minix have to make sure the compiler
 * recognizes at least __setjmp as a setjmp call (if applicable) and provide
 * library implementations of __setjmp and longjmp conforming to their
 * declarations below; if this is not possible, compiler-specific code will
 * have to be added to this header.
 *
 * Porters of Minix+GCC to other architectures have to make sure that the
 * __regs array of the jmp_buf structure is large enough to hold all the
 * registers the __setjmp implementation for that architecture has to save.
 */

#ifndef _SETJMP_H
#define _SETJMP_H

#ifndef _ANSI_H
#include <ansi.h>
#endif

typedef struct {
#if defined(__ACK__)
  _PROTOTYPE(void (*__pc),(void));	/* program counter */
  void *__sp;			/* stack pointer */
  void *__lb;			/* local base (ACKspeak for frame pointer) */
  long __mask;			/* must have size >= sizeof(sigset_t) */
  int __flags;
#else /* GCC */
  int __flags;			/* XXX - long might give better alignment */
  long __mask;			/* must have size >= sizeof(sigset_t) */
  void *__regs[16];		/* actual use is architecture dependent */
#endif
} jmp_buf[1];

_PROTOTYPE( int __setjmp, (jmp_buf _env, int _savemask)			);
_PROTOTYPE( void longjmp, (jmp_buf _env, int _val)			);

#define setjmp(env)	__setjmp((env), 1)

#ifdef _MINIX
#define _setjmp(env)	__setjmp((env), 0)
#define _longjmp(env, val)	longjmp((env), (val))
#endif

#ifdef _POSIX_SOURCE
typedef jmp_buf sigjmp_buf;
#define sigsetjmp(env, savemask) __setjmp((env), (savemask))
#define siglongjmp(env, val)	longjmp((env), (val))
#endif /* _POSIX_SOURCE */

#endif /* _SETJMP_H */
