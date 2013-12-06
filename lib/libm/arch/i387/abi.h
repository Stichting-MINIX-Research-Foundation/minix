/*	$NetBSD: abi.h,v 1.8 2013/09/12 15:36:16 joerg Exp $	*/

/*
 * Written by Frank van der Linden (fvdl@wasabisystems.com)
 */

/*
 * The x86-64 ABI specifies that float and double arguments
 * are passed in SSE2 (xmm) registers. Unfortunately,
 * there is no way to push those on to the FP stack, which is
 * where the fancier instructions get their arguments from.
 *
 * Define some prologues and epilogues to store and retrieve
 * xmm regs to local variables.
 */

#ifdef __x86_64__

#define ARG_LONG_DOUBLE_ONE	8(%rsp)
#define ARG_LONG_DOUBLE_TWO	24(%rsp)
#define ARG_DOUBLE_ONE		-8(%rsp)
#define ARG_DOUBLE_ONE_LSW	-8(%rsp)
#define ARG_DOUBLE_ONE_MSW	-4(%rsp)
#define ARG_DOUBLE_TWO		-16(%rsp)
#define ARG_FLOAT_ONE		-4(%rsp)
#define ARG_FLOAT_TWO		-8(%rsp)

#define XMM_ONE_ARG_DOUBLE_PROLOGUE \
	movsd	%xmm0, ARG_DOUBLE_ONE
	
#define XMM_TWO_ARG_DOUBLE_PROLOGUE \
	movsd	%xmm0, ARG_DOUBLE_ONE ; \
	movsd	%xmm1, ARG_DOUBLE_TWO

#define XMM_ONE_ARG_FLOAT_PROLOGUE \
	movss	%xmm0, ARG_FLOAT_ONE

#define XMM_TWO_ARG_FLOAT_PROLOGUE \
	movss	%xmm0, ARG_FLOAT_ONE ; \
	movss	%xmm1, ARG_FLOAT_TWO

#define XMM_DOUBLE_EPILOGUE \
	fstpl ARG_DOUBLE_ONE ; \
	movsd ARG_DOUBLE_ONE, %xmm0

#define XMM_FLOAT_EPILOGUE \
	fstps ARG_FLOAT_ONE ; \
	movss ARG_FLOAT_ONE, %xmm0

#define FLDL_VAR(x)	fldl x(%rip)

#else

#define ARG_LONG_DOUBLE_ONE	4(%esp)
#define ARG_LONG_DOUBLE_TWO	16(%esp)
#define ARG_DOUBLE_ONE		4(%esp)
#define ARG_DOUBLE_ONE_LSW	4(%esp)
#define ARG_DOUBLE_ONE_MSW	8(%esp)
#define ARG_DOUBLE_TWO		12(%esp)
#define ARG_FLOAT_ONE		4(%esp)
#define ARG_FLOAT_TWO		8(%esp)

#define XMM_ONE_ARG_DOUBLE_PROLOGUE
#define XMM_TWO_ARG_DOUBLE_PROLOGUE
#define XMM_ONE_ARG_FLOAT_PROLOGUE
#define XMM_TWO_ARG_FLOAT_PROLOGUE

#define XMM_DOUBLE_EPILOGUE
#define XMM_FLOAT_EPILOGUE

#ifdef __PIC__
#define FLDL_VAR(x) \
	PIC_PROLOGUE ; \
	fldl PIC_GOTOFF(x) ; \
	PIC_EPILOGUE
#else
#define FLDL_VAR(x) \
	fldl x

#endif
#endif
