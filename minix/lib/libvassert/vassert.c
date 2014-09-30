#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <setjmp.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <machine/stackframe.h>
#include "vassert.h"

VAssert_StateWrapper vassert_state ALIGNED(VASSERT_PAGE_SIZE);

#define TRUE 1
#define FALSE 0

#define MAGIC_CMD  0x564d5868
#define MAGIC_PORT 0x5658
#define HIGH_BAND_PORT 0x5659

#define BACKDOOR_PORT 		51
#define BACKDOOR_HB_PORT	1
#define CMD_SET_ADDRESS 	BACKDOOR_PORT|(1<<16)
#define CMD_RETURN_REPLAY 	BACKDOOR_PORT|(2<<16)
#define CMD_GO_LIVE		BACKDOOR_PORT|(3<<16)
#define CMD_LOG			BACKDOOR_HB_PORT|(4<<16)
#define CMD_SET_RECORD		47

#define LOG_MAX 512

typedef char Bool;
typedef unsigned int uint32;
typedef unsigned long long uint64;

#ifdef VM_X86_64
typedef uint64 VA;
#else
typedef uint32 VA;
#endif

static sigjmp_buf segv_jmp;

void libvassert_process_backdoor(uint32, uint32, uint32, reg_t *, reg_t *,
				 reg_t *, reg_t *);

/*
 *---------------------------------------------------------------------
 *
 * sig_segv --
 * 
 *    Customed SEGV signal handler for VAssert_IsInVM.
 *
 * Results:
 *
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------
 */

static void __dead sig_segv(int sig_no)
{
   /* jumping to error handling in VAssert_IsInVM. */
   siglongjmp(segv_jmp, 1);
}


/*
 *---------------------------------------------------------------------
 *
 * VAssert_IsInVM --
 * 
 *    Check if we are in virtual world.
 *
 * Results:
 *
 *    Return TRUE on success, or FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------
 */

static Bool VAssert_IsInVM(void)
{
   uint32 eax, ebx, ecx, edx;
   static Bool inVM = FALSE;
   static Bool tested = FALSE;
   if (!tested) {
      /* Error handling. */
      if (sigsetjmp(segv_jmp, 0) != 0) {
         signal(SIGSEGV, SIG_DFL);
         inVM = FALSE;
         return inVM;
      }
      tested = TRUE;
      /* Install custom handler. */
      signal(SIGSEGV, sig_segv);
      /* Test if we are in a VM. */
      libvassert_process_backdoor(0x0a, 0, MAGIC_PORT, &eax, &ebx, &ecx, &edx);
      signal(SIGSEGV, SIG_DFL);
      inVM = TRUE;
   }
   return inVM;
}


/*
 *---------------------------------------------------------------------
 *
 * VAssert_Init --
 * 
 *    Tell vmx that vassert is inited.
 *
 * Results:
 *
 *    Return 0 on success, or -1 on failure.
 *
 * Side effects:
 *    None
 *
 *---------------------------------------------------------------------
 */

char VAssert_Init(void)
{
   uint32 eax, ebx, ecx, edx;
   VA page_address = (VA) &vassert_state.inReplay;
   if (!VAssert_IsInVM()) {
      return -1;
   }
   bzero((char*) &vassert_state, sizeof vassert_state);
#ifndef __minix
   /* Lock the page. */
   if (mlock(&vassert_state, sizeof vassert_state)) {
      return -1;
   }
#endif

   libvassert_process_backdoor(CMD_SET_ADDRESS, page_address,
   	MAGIC_PORT|(1<<16), &eax, &ebx, &ecx, &edx);

   return (eax != (uint32)-1) ? 0 : -1;
}


/*
 *---------------------------------------------------------------------
 *
 * VAssert_Uninit --
 * 
 *    Tell vmx that vassert is finalized.
 *
 * Results:
 *
 *    Return 0 on success, or -1 on failure.
 *
 * Side effects:
 *    None
 *
 *---------------------------------------------------------------------
 */

char VAssert_Uninit(void)
{
   unsigned int eax, ebx, ecx, edx;
   if (!VAssert_IsInVM()) {
      return -1;
   }
   libvassert_process_backdoor(CMD_SET_ADDRESS, 0, MAGIC_PORT|(0<<16), &eax, &ebx, &ecx, &edx);
   return (eax != (unsigned int)-1) ? 0 : 1;
}


/*
 *---------------------------------------------------------------------
 *
 * VAssert_LogMain --
 * 
 *    Print message to a text file on host side.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Write to a text file with fixed name.
 *    If the file exists, host UI will ask for append/replace/ignore
 *
 *---------------------------------------------------------------------
 */

void VAssert_LogMain(const char *format, ...)
{
   unsigned int eax, ebx, ecx, edx;
   char buf[LOG_MAX];
   unsigned int len = 0;
   va_list ap;
   va_start(ap, format);
   len = vsnprintf(buf, LOG_MAX, format, ap);
   va_end(ap);
   __asm__ __volatile__("cld; rep outsb;"
                        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) 
                        : "0"(MAGIC_CMD), "1"(CMD_LOG), "2"(len), "d"(HIGH_BAND_PORT), "S"(buf)
                        : "memory"
                       );
}


/*
 *---------------------------------------------------------------------
 *
 * VAssert_GoLiveMain --
 * 
 *    Make the vm which is in replay exit replay.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Replay is stopped.
 *
 *---------------------------------------------------------------------
 */

void VAssert_GoLiveMain(void)
{
   unsigned int eax, ebx, ecx, edx;
   vassert_state.inReplay = 0;
   libvassert_process_backdoor(CMD_GO_LIVE, 0, MAGIC_PORT, &eax, &ebx, &ecx, &edx);
}


/*
 *---------------------------------------------------------------------
 *
 * VAssert_ReturnToReplayMain --
 * 
 *    Called after the custom work is done, and replay is to continue.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Replay is continued from pause.
 *
 *---------------------------------------------------------------------
 */

void VAssert_ReturnToReplayMain(void)
{
   unsigned int eax, ebx, ecx, edx;
   libvassert_process_backdoor(CMD_RETURN_REPLAY, 0, MAGIC_PORT, &eax, &ebx, &ecx, &edx);
}


/*
 *---------------------------------------------------------------------
 *
 * VAssert_SetRecordingMain --
 * 
 *    Ask vmx for starting or stopping recording.
 *
 * Results:
 *
 *    Return TRUE on success, or FALSE on failure.
 *
 * Side effects:
 *    Recording is started or stopped.
 *
 *---------------------------------------------------------------------
 */

char VAssert_SetRecordingMain(char start)
{
   uint32 eax, ebx, ecx, edx;
   if (!VAssert_IsInVM()) {
      return FALSE;
   }
   libvassert_process_backdoor(CMD_SET_RECORD, start ? 1 : 2, MAGIC_PORT, &eax, &ebx, &ecx, &edx);
   return (eax == 1) ? TRUE : FALSE;
}

