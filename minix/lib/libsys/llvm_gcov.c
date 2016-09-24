/* LLVM-to-GCOV converter by D.C. van Moolenbroek */
/*
 * Originally, we had a GCOV code coverage implementation for GCC only.  We
 * have now largely switched to LLVM, and LLVM uses a different internal
 * implementation of the coverage data generation.  For regular userland
 * programs, the implementation is part of LLVM compiler-rt's libprofile_rt.
 * That implementation is unsuitable for our system services.  Instead, this
 * file converts the calls used by LLVM into _gcov_f*() calls expected by our
 * GCOV-for-GCC implementation, thus adding support for LLVM coverage by
 * leveraging our previous GCC support.
 */

#if __clang__

#include <stdlib.h>
#include <string.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/gcov.h>
#include <assert.h>

/*
 * What is the maximum number of source modules for one single system service?
 * This number is currently way higher than needed, but if we ever add support
 * for coverage of system service libraries (e.g., libsys and libminc), this
 * number may not even be high enough.  A warning is printed on overflow.
 * Note that we need this to be a static array, because we cannot use malloc()
 * in particular in the initialization stage of the VM service.
 */
#define NR_MODULES 256

/*
 * The code in this file is a MINIX3 service specific replacement of the
 * GCDAProfiling.c code in LLVM's compiler-rt.  Their code cannot be used
 * directly because they assume a userland environment, using all sorts of
 * POSIX calls as well as malloc(3), none of which we can offer for system
 * services in this case.  So, we provide our own implementation instead.
 * However, while compiler-rt is always kept in sync with the LLVM profiling
 * data emitter, we do not have that luxury.  The current version of this
 * implementation has been written for LLVM 3.4 and 3.6, between which the LLVM
 * GCOV ABI changed.  Our current implementation supports both versions, but
 * may break with newer LLVM versions, even though we should be good up to and
 * possibly including LLVM 4.0 at least.  Hopefully, at this point the LLVM
 * GCOV ABI should have stabilized a bit.
 *
 * Note that since we do not have access to internal LLVM headers here, an ABI
 * mismatch would not be noticable until llvm-cov fails to load the resulting
 * files.  This whole mess is worth it only because we can really, really use
 * the coverage information for our test sets..
 */
#if __clang_major__ == 3 && __clang_minor__ == 4
#define LLVM_35	0	/* version 3.4 only */
#elif __clang_major__ == 3 && __clang_minor__ >= 5
#define LLVM_35	1	/* version 3.5 and later */
#else
#error "unknown LLVM/clang version, manual inspection required"
#endif

typedef void (*write_cb_t)(void);
typedef void (*flush_cb_t)(void);

/*
 * Except for llvm_gcda_emit_function(), these functions are already declared
 * in the 3.5+ ABI style.  With the 3.4 ABI, some parameters may have garbage.
 */
void llvm_gcda_start_file(const char *, const char *, uint32_t);
void llvm_gcda_emit_function(uint32_t, const char *,
#if LLVM_35
	uint32_t, uint8_t, uint32_t);
#else
	uint8_t);
#endif
void llvm_gcda_emit_arcs(uint32_t, uint64_t *);
void llvm_gcda_summary_info(void);
void llvm_gcda_end_file(void);
void __gcov_flush(void);
void llvm_gcov_init(write_cb_t, flush_cb_t);

static flush_cb_t flush_array[NR_MODULES];
static unsigned int flush_count = 0;

static FILE *gcov_file = NULL;

/*
 * LLVM hook for opening the .gcda file for a specific source module.
 */
void
llvm_gcda_start_file(const char * file_name, const char version[4],
	uint32_t stamp)
{
	uint32_t word[3];

	assert(gcov_file == NULL);

	gcov_file = _gcov_fopen(file_name, "w+b");
	assert(gcov_file != NULL);

	/*
	 * Each _gcov_fwrite() invocation translates into a kernel call, so we
	 * want to aggregate writes as much as possible.
	 */
	word[0] = 0x67636461;				/* magic: "gcda" */
	memcpy(&word[1], version, sizeof(word[1]));	/* version */
#if LLVM_35
	word[2] = stamp;				/* stamp */
#else
	word[2] = 0x4C4C564D;				/* stamp: "LLVM" */
#endif

	_gcov_fwrite(word, sizeof(word[0]), __arraycount(word), gcov_file);
}

/*
 * LLVM hook for writing a function announcement to the currently opened .gcda
 * file.
 */
void
llvm_gcda_emit_function(uint32_t ident, const char * func_name,
#if LLVM_35
	uint32_t func_cksum, uint8_t extra_cksum, uint32_t cfg_cksum)
#else
	uint8_t extra_cksum)
#endif
{
	uint32_t word[6];
	size_t words, len, wlen;

	word[0] = 0x01000000;			/* tag: function */
	words = 2;
	word[2] = ident;			/* ident */
#if LLVM_35
	word[3] = func_cksum;			/* function checksum */
#else
	word[3] = 0;				/* function checksum */
#endif
	if (extra_cksum) {
#if LLVM_35
		word[4] = cfg_cksum;		/* configuration checksum */
#else
		word[4] = 0;			/* configuration checksum */
#endif
		words++;
	}
	word[1] = words;			/* length */

	if (func_name != NULL) {
		len = strlen(func_name) + 1;
		wlen = len / sizeof(word[0]) + 1;
		word[1] += 1 + wlen;
		word[2 + words] = wlen;
		words++;
	}

	_gcov_fwrite(word, sizeof(word[0]), 2 + words, gcov_file);

	if (func_name != NULL) {
		_gcov_fwrite(func_name, 1, len, gcov_file);
		_gcov_fwrite("\0\0\0\0", 1, wlen * sizeof(uint32_t) - len,
		    gcov_file);
	}
}

/*
 * LLVM hook for writing function arc counters to the currently opened .gcda
 * file.
 */
void
llvm_gcda_emit_arcs(uint32_t ncounters, uint64_t * counters)
{
	uint32_t word[2];

	assert(gcov_file != NULL);

	word[0] = 0x01a10000;			/* tag: arc counters */
	word[1] = ncounters * 2;		/* length */

	_gcov_fwrite(word, sizeof(word[0]), __arraycount(word), gcov_file);
	_gcov_fwrite(counters, sizeof(*counters), ncounters, gcov_file);
}

/*
 * LLVM hook for writing summary information to the currently opened .gcda
 * file.
 */
void
llvm_gcda_summary_info(void)
{
	uint32_t word[13];

	memset(word, 0, sizeof(word));
	word[0] = 0xa1000000;			/* tag: object summary */
	word[1] = 9;				/* length */
	word[2] = 0;				/* checksum */
	word[3] = 0;				/* counter number */
	word[4] = 1;				/* runs */
	word[11] = 0xa3000000;			/* tag: program summary */
	word[12] = 0;				/* length */

	_gcov_fwrite(word, sizeof(word[0]), __arraycount(word), gcov_file);
}

/*
 * LLVM hook for closing the currently opened .gcda file.
 */
void
llvm_gcda_end_file(void)
{
	uint32_t word[2];

	assert(gcov_file != NULL);

	word[0] = 0;				/* tag: end of file */
	word[1] = 0;				/* length zero */
	_gcov_fwrite(word, sizeof(word[0]), __arraycount(word), gcov_file);

	_gcov_fclose(gcov_file);
	gcov_file = NULL;
}

/*
 * Our implementation for LLVM of the GCC function to flush the coverage data.
 * The function is called by our libsys's GCOV code.
 */
void
__gcov_flush(void)
{
	unsigned int i;

	/* Call the flush function for each registered module. */
	for (i = 0; i < flush_count; i++)
		flush_array[i]();
}

/*
 * LLVM hook for registration of write and flush callbacks.  The former is to
 * be used on exit, the latter on a pre-exit flush.  We use the latter only.
 * This function is basically called once for each compiled source module.
 */
void
llvm_gcov_init(write_cb_t write_cb __unused, flush_cb_t flush_cb)
{

	if (flush_cb == NULL)
		return;

	/* If the array is full, drop this module. */
	if (flush_count == __arraycount(flush_array))
		return; /* array full, so we are going to miss information */

	/* Add the flush function to the array. */
	flush_array[flush_count++] = flush_cb;

	/*
	 * We print this warning here so that we print it only once.  What are
	 * the odds that there are *exactly* NR_MODULES modules anyway?
	 */
	if (flush_count == __arraycount(flush_array))
		printf("llvm_gcov: process %d has too many modules, "
		    "profiling data lost\n", sef_self());
}

#endif /*__clang__*/
