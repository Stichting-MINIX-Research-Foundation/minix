#include <sys/cdefs.h>
#include <namespace.h>
#include <lib.h>
#include <machine/stackframe.h>
#include <sys/cdefs.h>
#include <ucontext.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__x86_64__)
void ctx_start(void);
#else
void ctx_start(void (*)(void), int, ...);
#endif

/*===========================================================================*
 *				setuctx					     *
 *===========================================================================*/
int setuctx(const ucontext_t *ucp)
{
  int r;

  if (ucp == NULL) {
	errno = EFAULT;
	return(-1);
  }

  if (!(ucp->uc_flags & _UC_IGNSIGM)) {
	/* Set signal mask */
	if ((r = sigprocmask(SIG_SETMASK, &ucp->uc_sigmask, NULL)) == -1)
		return(r);
  }

  if (!(ucp->uc_flags & _UC_IGNFPU)) {
	if ((r = setmcontext(&(ucp->uc_mcontext))) == -1)
		return(r);
  }

  return(0);
}


/*===========================================================================*
 *				getuctx					     *
 *===========================================================================*/
int getuctx(ucontext_t *ucp) 
{
  int r;

  if (ucp == NULL) {
	errno = EFAULT;
	return(-1);
  }

  if (!(ucp->uc_flags & _UC_IGNSIGM)) {
	/* Get signal mask */
	if ((r = sigprocmask(0, NULL, &ucp->uc_sigmask)) == -1)
		return(r);
  }

  if (!(ucp->uc_flags & _UC_IGNFPU)) {
	if ((r = getmcontext(&(ucp->uc_mcontext))) != 0)
		return(r);
  }

  return(0);
}


/*===========================================================================*
 *				makecontext				     *
 *===========================================================================*/
void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
  va_list ap;
#if defined(__i386__) || defined(__arm__)
  unsigned int *stack_top;
#endif

  /* There are a number of situations that are erroneous, but we can't actually
     tell the caller something is wrong, because this is a void function.
     Instead, mcontext_t contains a magic field that has to be set
     properly before it can be used. */
  if (ucp == NULL) {
	return;
  } else if ((ucp->uc_stack.ss_sp == NULL) || 
	     (ucp->uc_stack.ss_size < MINSIGSTKSZ)) {
	ucp->uc_mcontext.mc_magic = 0; 
	_UC_MACHINE_SET_STACK(ucp, 0);
	return;
  }

  if (ucp->uc_mcontext.mc_magic == MCF_MAGIC) {
#if defined(__i386__)
	/* The caller provides a pointer to a stack that we can use to run our
	   context on. When the context starts, control is given to a wrapped 
	   start routine, which calls a function and cleans up the stack
	   afterwards. The wrapper needs the address of that function on the
	   stack.
	   The stack will be prepared as follows:
		func()       - start routine
		arg1         - first argument
		...
		argn         - last argument
		ucp          - context, esp points here when `func' returns
	   _ctx_start pops the address of `func' from the stack and calls it. 
	   The stack will then be setup with all arguments for `func'. When
	   `func' returns, _ctx_start cleans up the stack such that ucp is at
	   the top of the stack, ready to be used by resumecontext.
	   Resumecontext, in turn, checks whether another context is ready to
	   be executed (i.e., uc_link != NULL) or exit(2)s the process. */

	/* Find the top of the stack from which we grow downwards. */
	stack_top = (unsigned int *) ((uintptr_t ) ucp->uc_stack.ss_sp +
						   ucp->uc_stack.ss_size);

	/* Align the arguments to 16 bytes (we might lose a few bytes of stack
	   space here).*/
	stack_top = (unsigned int *) ((uintptr_t) stack_top & ~0xf);
	
	/* Make room for 'func', the `func' routine arguments, and ucp. */
	stack_top -= (1 + argc + 1);

	/* Adjust the machine context to point to the top of this stack and the
	   program counter to the context start wrapper. */
	_UC_MACHINE_SET_EBP(ucp, 0); /* Clear frame pointer */
	_UC_MACHINE_SET_STACK(ucp, (reg_t) stack_top);
	_UC_MACHINE_SET_PC(ucp, (reg_t) ctx_start);

	*stack_top++ = (uintptr_t) func;

	/* Copy arguments to the stack. */
	va_start(ap, argc);
	while (argc-- > 0) {
		*stack_top++ = va_arg(ap, uintptr_t);
	}
	va_end(ap);

	/* Store ucp on the stack */
	*stack_top = (uintptr_t) ucp;

	/* Set ESI to point to the base of the stack where ucp is stored, so
	   that the wrapper function knows how to clean up the stack after
	   calling `func' (i.e., how to adjust ESP). */
	_UC_MACHINE_SET_ESI(ucp, (reg_t) stack_top);
	

	/* If we ran out of stack space, invalidate stack pointer. Eventually,
	   swapcontext will choke on this and return ENOMEM. */
	if (stack_top == ucp->uc_stack.ss_sp) {
		_UC_MACHINE_SET_STACK(ucp, 0);
	}
#elif defined(__arm__)
	/* The caller provides a pointer to a stack that we can use to run our
	   context on. When the context starts, control is given to the
	   requested function. When the function finishes, it returns to the
	   _ctx_start wrapper that calls resumecontext (after setting up
	   resumecontext's parameter).

	   The first four arguments for the function will be passed in
	   regs r0-r3 as specified by the ABI, and the rest will go on
	   the stack.  The ucp is saved in r4 so that we can
	   eventually pass it to resumecontext. The r4 register is
	   callee-preserved, so the ucp will remain valid in r4 when
	   _ctx_start runs. _ctx_start will move the ucp from r4 into
	   r0, so that the ucp is the first paramater for resumecontext.
	   Then, _ctx_start will call resumecontext. Resumecontext, in turn,
	   checks whether another context is ready to be executed
	   (i.e., uc_link != NULL) or exit(2)s the process. */

	/* Find the top of the stack from which we grow downwards. */
	stack_top = (unsigned int *) ((uintptr_t ) ucp->uc_stack.ss_sp +
						   ucp->uc_stack.ss_size);

	/* Align the arguments to 16 bytes (we might lose a few bytes of stack
	   space here).*/
	stack_top = (unsigned int *) ((uintptr_t) stack_top & ~0xf);

	/* Make room for `func' routine arguments that don't fit in r0-r3 */
	if (argc > 4)
		stack_top -= argc - 4;

	/* Adjust the machine context to point to the top of this stack and the
	   program counter to the 'func' entry point. Set lr to ctx_start, so
	   ctx_start runs after 'func'. Save ucp in r4 */
	_UC_MACHINE_SET_FP(ucp, 0); /* Clear frame pointer */
	_UC_MACHINE_SET_STACK(ucp, (reg_t) stack_top);
	_UC_MACHINE_SET_PC(ucp, (reg_t) func);
	_UC_MACHINE_SET_LR(ucp, (reg_t) ctx_start);
	_UC_MACHINE_SET_R4(ucp, (reg_t) ucp);

	/* Copy arguments to r0-r3 and stack. */
	va_start(ap, argc);
	/* Pass up to four arguments in registers. */
	if (argc-- > 0)
		_UC_MACHINE_SET_R0(ucp, va_arg(ap, uintptr_t));
	if (argc-- > 0)
		_UC_MACHINE_SET_R1(ucp, va_arg(ap, uintptr_t));
	if (argc-- > 0)
		_UC_MACHINE_SET_R2(ucp, va_arg(ap, uintptr_t));
	if (argc-- > 0)
		_UC_MACHINE_SET_R3(ucp, va_arg(ap, uintptr_t));
	/* Pass the rest on the stack. */
	while (argc-- > 0) {
		*stack_top++ = va_arg(ap, uintptr_t);
	}
	va_end(ap);

	/* If we ran out of stack space, invalidate stack pointer. Eventually,
	   swapcontext will choke on this and return ENOMEM. */
	if (stack_top == ucp->uc_stack.ss_sp) {
		_UC_MACHINE_SET_STACK(ucp, 0);
	}
#elif defined(__x86_64__)
	/*
	 * SysV AMD64 ABI: first 6 arguments in RDI, RSI, RDX, RCX, R8, R9
	 * (gregs indices 0-5).  Remaining arguments go on the stack.
	 *
	 * ctx_start is the entry point (RIP).  It receives the function
	 * pointer in R12 and ucp in R13 (both callee-saved), then calls
	 * func with the args already in the argument registers, and finally
	 * calls resumecontext(ucp) when func returns.
	 *
	 * RSP must be 16-byte aligned when ctx_start executes its
	 * 'call *%r12', so that func starts with RSP ≡ 8 (mod 16) as
	 * required by the ABI.  Stack args occupy sp[0..nstackargs-1];
	 * if nstackargs is odd we allocate one extra alignment-padding slot
	 * below them (at a lower address, never accessed by func).
	 */
	{
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	uintptr_t *sp;
	int nregargs, nstackargs, i;

	/* Top of stack, aligned down to 16 bytes */
	sp = (uintptr_t *)((uintptr_t)ucp->uc_stack.ss_sp +
	    ucp->uc_stack.ss_size);
	sp = (uintptr_t *)(((uintptr_t)sp) & ~0xfUL);

	nregargs   = (argc < 6) ? argc : 6;
	nstackargs = argc - nregargs;

	if (nstackargs > 0) {
		sp -= nstackargs;
		/* Ensure 16-byte alignment: if nstackargs is odd the
		 * subtraction leaves sp 8-mod-16; add one padding slot. */
		if ((uintptr_t)sp & 0xf)
			sp--;
	}
	/* sp is now 16-byte aligned; stack args go at sp[0..nstackargs-1] */

	gr[_REG_RIP] = (uintptr_t)ctx_start;
	gr[_REG_RSP] = (uintptr_t)sp;
	gr[_REG_RBP] = 0;
	gr[_REG_R12] = (uintptr_t)(void (*)(void))func;  /* for ctx_start */
	gr[_REG_R13] = (uintptr_t)ucp;                   /* for ctx_start */

	/* First 6 args go into argument registers (gregs[0..5] = RDI..R9) */
	va_start(ap, argc);
	for (i = 0; i < nregargs; i++)
		gr[i] = va_arg(ap, __greg_t);

	/* Remaining args go on the stack at sp[0..nstackargs-1] */
	for (i = 0; i < nstackargs; i++)
		sp[i] = va_arg(ap, uintptr_t);
	va_end(ap);

	/* Invalidate if stack space was exhausted */
	if ((char *)sp < (char *)ucp->uc_stack.ss_sp)
		_UC_MACHINE_SET_STACK(ucp, 0);
	}
#else
# error "Unsupported platform"
#endif
  }
}


/*===========================================================================*
 *				swapcontext				     *
 *===========================================================================*/
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
  int r;

  if ((oucp == NULL) || (ucp == NULL)) {
	errno = EFAULT;
	return(-1);
  }

  if (_UC_MACHINE_STACK(ucp) == 0) {
	/* No stack space. Bail out. */
	errno = ENOMEM;
	return(-1);
  } 

  oucp->uc_flags &= ~_UC_SWAPPED;
  r = getcontext(oucp);
  if ((r == 0) && !(oucp->uc_flags & _UC_SWAPPED)) {
	oucp->uc_flags |= _UC_SWAPPED;
	r = setcontext(ucp);
  }

  return(r);
}


/*===========================================================================*
 *				resumecontext				     *
 *===========================================================================*/
__dead
void resumecontext(ucontext_t *ucp)
{
  if (ucp->uc_link == NULL) exit(0);

  /* Error handling? Where should the error go to? */
  (void) setcontext((const ucontext_t *) ucp->uc_link);

  exit(1); /* Never reached */
}

