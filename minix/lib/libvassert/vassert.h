#ifndef _VASSERT_H_
#define _VASSERT_H_

#ifdef __cplusplus
extern "C"
{
#endif /*__cplusplus*/

#define ALIGNED(n) __attribute__((__aligned__(n)))
#define VASSERT_TRIGGER_OFFSET 1221
#define VASSERT_PAGE_SIZE      4096

/* Need to align at 4K. */
/* Ensure the inReplay flag is on its own page. */
#pragma pack(1)
typedef struct VAssert_StateWrapper {
   char space1[VASSERT_TRIGGER_OFFSET];
   volatile char inReplay;
   char space[VASSERT_PAGE_SIZE - VASSERT_TRIGGER_OFFSET - sizeof(char)];
} VAssert_StateWrapper;
#pragma pack()

extern VAssert_StateWrapper vassert_state;

/*
 * User-selectable standard functions.
 * XXX: Document these, in coordination with the SDK docs.
 */

#if defined(__KERNEL__)
#  define KERNEL_VASSERT
#endif

#ifdef KERNEL_VASSERT

#  ifndef VASSERT_CUSTOM_ASSERT
#     define VASSERT_CUSTOM_ASSERT(expr)
#  endif

#  ifndef VASSERT_CUSTOM_ABORT
#     include <linux/kernel.h>
#     define VASSERT_CUSTOM_ABORT() ((void)0) // printk(KERN_ALERT"VAssert abort at %s: %d", __FILE__, __LINE__)
#  endif

#  ifndef VASSERT_CUSTOM_LOG
#     include <linux/kernel.h>
#     define VASSERT_CUSTOM_LOG printk
#  endif

#else
#  ifndef VASSERT_CUSTOM_ASSERT
#     include <assert.h>
#     define VASSERT_CUSTOM_ASSERT assert
#  endif

#  ifndef VASSERT_CUSTOM_ABORT
#     include <stdlib.h>
#     define VASSERT_CUSTOM_ABORT abort
#  endif

#  ifndef VASSERT_CUSTOM_LOG
#     include <stdio.h>
#     define VASSERT_CUSTOM_LOG printf
#  endif
#endif

/* Results: 0 if successful, -1 if not. */
// XXX need to automatic de-register trigger page when the program quits
extern char VAssert_Init(void);
extern char VAssert_Uninit(void);

/*
 * These functions should not be called directly; they need to be wrapped.
 */
extern void VAssert_LogMain(const char *format, ...);
extern void VAssert_GoLiveMain(void);
extern void VAssert_ReturnToReplayMain(void);
extern char VAssert_Trace(size_t max_size);

#ifdef VASSERT_ALWAYS_EXECUTE

#define VAssert_Assert(expr)              \
do {                                      \
   VASSERT_CUSTOM_ASSERT(expr);           \
} while (0)

#define VAssert_Log(args)                 \
do {                                      \
   VASSERT_CUSTOM_LOG args;               \
} while (0)

#define VAssert_BeginBlock
#define VAssert_EndBlock

#else /* VASSERT_ALWAYS_EXECUTE */

#define VAssert_Assert(expr)              \
do {                                      \
   if (vassert_state.inReplay) {          \
      if (!(expr)) {                      \
         VAssert_GoLiveMain();            \
         VASSERT_CUSTOM_ABORT();          \
      } else {                            \
         VAssert_ReturnToReplayMain();    \
      }                                   \
   }                                      \
} while (0)

#define VAssert_Log(args)                 \
do {                                      \
   if (vassert_state.inReplay) {          \
      VAssert_LogMain args;               \
      VAssert_ReturnToReplayMain();       \
   }                                      \
} while (0)

#define VAssert_BeginBlock if (vassert_state.inReplay)
#define VAssert_EndBlock VAssert_ReturnToReplayMain()

#endif /* VASSERT_ALWAYS_EXECUTE */

/* 
 * Record/Replay functionality
 */
/*
 * Results: True if successful, false if not.
 * Input: True to start recording, false to stop.
 */
extern char VAssert_SetRecordingMain(char start);

#define VAssert_StartRecording() VAssert_SetRecordingMain(1)
#define VAssert_StopRecording() VAssert_SetRecordingMain(0)

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /*_VASSERT_H_*/
