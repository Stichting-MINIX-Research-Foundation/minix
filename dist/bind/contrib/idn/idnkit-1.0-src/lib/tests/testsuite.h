/* $Id: testsuite.h,v 1.1.1.1 2003-06-04 00:27:03 marka Exp $ */
/*
 * Copyright (c) 2002 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef IDN_TESTSUITE_H
#define IDN_TESTSUITE_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Result codes for test case.
 */
typedef enum {
	idn_teststatus_pass,
	idn_teststatus_fail,
	idn_teststatus_skip
} idn_teststatus_t;

/*
 * Testsuite manager type (opaque).
 */
typedef struct idn_testsuite *idn_testsuite_t;

/*
 * Testcase function type.
 */
typedef void (*idn_testsuite_testproc_t)(idn_testsuite_t ctx);

/*
 * Message handler type.
 */
typedef void (*idn_testsuite_msgproc_t)(const char *msg);

/*
 * Create a testsuite manager context.
 *
 * Create an empty context and store it in '*ctxp'.
 * Return 1 on success.  Return 0 if memory is exhausted.
 */
extern int
idn_testsuite_create(idn_testsuite_t *ctxp);

/*
 * Destory the testsuite manager context.
 *
 * Destroy the context created by idn_testsuite_create(), and release
 * memory allocated to the context.
 */
extern void
idn_testsuite_destroy(idn_testsuite_t ctx);

/*
 * Add a test case to the `group' test group.
 * Return 1 on success.  Return 0 if memory is exhausted.
 */
extern int
idn_testsuite_addtestcase(idn_testsuite_t ctx, const char *title,
	                  idn_testsuite_testproc_t proc);

/*
 * Return the number of test cases registered in the context.
 */
extern int
idn_testsuite_ntestcases(idn_testsuite_t ctx);

/*
 * Run test cases registered in the context.
 */
extern void
idn_testsuite_runall(idn_testsuite_t ctx);
extern void
idn_testsuite_run(idn_testsuite_t ctx, char *titles[]);

/*
 * Return the string description of `status'.
 */
extern const char *
idn_teststatus_tostring(idn_teststatus_t status);

/*
 * Return the number of passed/failed/skipped test cases.
 */
extern int
idn_testsuite_npassed(idn_testsuite_t ctx);
extern int
idn_testsuite_nfailed(idn_testsuite_t ctx);
extern int
idn_testsuite_nskipped(idn_testsuite_t ctx);

/*
 * Set/Get status of the test case running currently.
 *
 * These functions must be called by test case function.
 */
extern idn_teststatus_t
idn_testsuite_getstatus(idn_testsuite_t ctx);
extern void
idn_testsuite_setstatus(idn_testsuite_t ctx, idn_teststatus_t status);

/*
 * Enable/Disable verbose mode.
 */
extern void
idn_testsuite_setverbose(idn_testsuite_t ctx);
extern void
idn_testsuite_unsetverbose(idn_testsuite_t ctx);

/*
 * Generic assertion with message
 */
extern void
idn_testsuite_assert(idn_testsuite_t ctx, const char *msg,
		     const char *file, int lineno);

#define ASSERT_THRU(msg) \
    idn_testsuite_assert(ctx__, msg, __FILE__, __LINE__)
#define ASSERT(msg) \
  do { \
    ASSERT_THRU(msg); \
    if (idn_testsuite_getstatus(ctx__) != idn_teststatus_pass) \
      goto EXIT__; \
  } while (0)

/*
 * Assertion function and macro to compare two `int' values.
 * The assertion passes if `gotten' is equal to `expected'.
 */
extern void
idn_testsuite_assertint(idn_testsuite_t ctx, int gotten, int expected,
			const char *file, int lineno);

#define ASSERT_INT(gotten, expected) \
  do { \
    idn_testsuite_assertint(ctx__, gotten, expected, __FILE__, __LINE__); \
    if (idn_testsuite_getstatus(ctx__) != idn_teststatus_pass) \
      goto EXIT__; \
  } while (0)

/*
 * Assertion function and macro to compare two strings.
 * The assertion passes if `gotten' is lexically equal to `expected'.
 */
extern void
idn_testsuite_assertstring(idn_testsuite_t ctx, const char *gotten, 
			   const char *expected, const char *file, int lineno);

#define ASSERT_STRING(gotten, expected) \
  do { \
    idn_testsuite_assertstring(ctx__, gotten, expected, __FILE__, __LINE__); \
    if (idn_testsuite_getstatus(ctx__) != idn_teststatus_pass) \
      goto EXIT__; \
  } while (0)

/*
 * Assertion function and macro to compare two pointers.
 * The assertion passes if `gotten' is equal to `expected'.
 */
extern void
idn_testsuite_assertptr(idn_testsuite_t ctx, const void *gotten, 
			const void *expected, const char *file, int lineno);

#define ASSERT_PTR(gotten, expected) \
  do { \
    idn_testsuite_assertptr(ctx__, gotten, expected, __FILE__, __LINE__); \
    if (idn_testsuite_getstatus(ctx__) != idn_teststatus_pass) \
      goto EXIT__; \
  } while (0)

/*
 * Assertion function and macro to compare two pointers.
 * The assertion passes if `gotten' is NOT equal to `expected'.
 */
extern void
idn_testsuite_assertptrne(idn_testsuite_t ctx, 
			  const void *gotten, const void *unexpected,
			  const char *file, int lineno);

#define ASSERT_PTR_NE(gotten, unexpected) \
  do { \
    idn_testsuite_assertptrne(ctx__, gotten, unexpected, __FILE__, __LINE__); \
    if (idn_testsuite_getstatus(ctx__) != idn_teststatus_pass) \
      goto EXIT__; \
  } while (0)

/*
 * Assertion function and macro to compare two `idn_result_t' values.
 * The assertion passes if `gotten' is equal to `expected'.
 */
extern void
idn_testsuite_assertresult(idn_testsuite_t ctx,
			   idn_result_t gotten, idn_result_t expected,
			   const char *file, int lineno);

#define ASSERT_RESULT(gotten, expected) \
  do { \
    idn_testsuite_assertresult(ctx__, gotten, expected, __FILE__, __LINE__); \
    if (idn_testsuite_getstatus(ctx__) != idn_teststatus_pass) \
      goto EXIT__; \
  } while (0)

/*
 * Assertion function and macro to compare two UCS4 strings.
 * The assertion passes if `gotten' is lexically equal to `expected'.
 */
extern void
idn_testsuite_assertucs4string(idn_testsuite_t ctx,
			       const unsigned long *gotten, 
			       const unsigned long *expected,
			       const char *file,
			       int lineno);

#define ASSERT_UCS4STRING_THRU(gotten, expected) \
  idn_testsuite_assertucs4string(ctx__, gotten, expected, __FILE__, __LINE__)
#define ASSERT_UCS4STRING(gotten, expected) \
  do { \
    ASSERT_UCS4STRING_THRU(gotten, expected); \
    if (idn_testsuite_getstatus(ctx__) != idn_teststatus_pass) \
      goto EXIT__; \
  } while (0)

/* 
 * Shorthands.
 */
#define SKIP_TESTCASE \
  do { \
       idn_testsuite_setstatus(ctx__, idn_teststatus_skip); \
       goto EXIT__; \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif /* IDN_TESTSUITE_H */
