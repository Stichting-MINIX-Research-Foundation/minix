//===--------------------------- libuwind.cpp -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
// Implements C++ ABI Exception Handling Level 1 as documented at:
//      http://mentorembedded.github.io/cxx-abi/abi-eh.html
//
//===----------------------------------------------------------------------===//

#define _UNWIND_GCC_EXTENSIONS

#include <unwind.h>

#include "UnwindCursor.hpp"

using namespace _Unwind;

typedef CFI_Parser<LocalAddressSpace, NativeUnwindRegisters> MyCFIParser;

// Internal object representing the address space of this process.
static LocalAddressSpace sThisAddressSpace(MyCFIParser::findPCRange);

typedef UnwindCursor<LocalAddressSpace, NativeUnwindRegisters> ThisUnwindCursor;

static _Unwind_Reason_Code unwind_phase1(ThisUnwindCursor &cursor,
                                         struct _Unwind_Exception *exc) {
  cursor.setInfoBasedOnIPRegister();

  // Walk frames looking for a place to stop.
  for (;;) {
    // Get next frame.
    // First frame is _Unwind_RaiseException and skipped.
    switch (cursor.step()) {
    case UNW_STEP_END:
      return _URC_END_OF_STACK;
    case UNW_STEP_FAILED:
      return _URC_FATAL_PHASE1_ERROR;
    case UNW_STEP_SUCCESS:
      break;
    }

    // Check if there is a personality routine for this frame.
    unw_proc_info_t frameInfo;
    cursor.getInfo(&frameInfo);
    if (frameInfo.end_ip == 0)
      return _URC_FATAL_PHASE1_ERROR;

    if (frameInfo.handler == 0)
      continue; // No personality routine, so try next frame.

    __personality_routine p = (__personality_routine)(frameInfo.handler);
    _Unwind_Reason_Code result = (*p)(1, _UA_SEARCH_PHASE, exc->exception_class,
                                      exc, (struct _Unwind_Context *)(&cursor));

    switch (result) {
    case _URC_HANDLER_FOUND:
      // This is either a catch clause or a local variable
      // with destructor.
      // Stop search and remember the frame for phase 2.
      exc->private_2 = cursor.getSP();
      return _URC_NO_REASON;

    case _URC_CONTINUE_UNWIND:
      // Continue unwinding
      break;

    default:
      // Bad personality routine.
      return _URC_FATAL_PHASE1_ERROR;
    }
  }
}

static _Unwind_Reason_Code unwind_phase2(ThisUnwindCursor &cursor,
                                         struct _Unwind_Exception *exc) {
  cursor.setInfoBasedOnIPRegister();

  // Walk frames until the frame selected in phase 1 is reached.
  for (;;) {
    // Get next frame.
    // First frame is _Unwind_RaiseException and skipped.
    switch (cursor.step()) {
    case UNW_STEP_END:
      return _URC_END_OF_STACK;
    case UNW_STEP_FAILED:
      return _URC_FATAL_PHASE2_ERROR;
    case UNW_STEP_SUCCESS:
      break;
    }

    unw_proc_info_t frameInfo;
    cursor.getInfo(&frameInfo);
    if (frameInfo.end_ip == 0)
      return _URC_FATAL_PHASE2_ERROR;

    if (frameInfo.handler == 0)
      continue; // No personality routine, continue.

    uintptr_t sp = cursor.getSP();

    _Unwind_Action action = _UA_CLEANUP_PHASE;
    // If this frame was selected in phase 1,
    // inform the personality routine.
    if (sp == exc->private_2)
      action = (_Unwind_Action)(action | _UA_HANDLER_FRAME);
    __personality_routine p = (__personality_routine)(frameInfo.handler);
    _Unwind_Reason_Code result = (*p)(1, action, exc->exception_class, exc,
                                      (struct _Unwind_Context *)(&cursor));
    switch (result) {
    case _URC_CONTINUE_UNWIND:
      // Continue unwinding unless the selected frame passed.
      if (sp == exc->private_2)
        return _URC_FATAL_PHASE2_ERROR;
      break;
    case _URC_INSTALL_CONTEXT:
      // Transfer control to landing pad.
      cursor.jumpto();
    default:
      // Bad personality routine.
      return _URC_FATAL_PHASE2_ERROR;
    }
  }
}

static _Unwind_Reason_Code unwind_phase2_forced(ThisUnwindCursor &cursor,
                                                struct _Unwind_Exception *exc,
                                                _Unwind_Stop_Fn stop,
                                                void *stop_arg) {
  _Unwind_Action action;
  cursor.setInfoBasedOnIPRegister();

  // Walk frames until the frame selected in phase 1 is reached.
  for (;;) {
    // Get next frame.
    // First frame is _Unwind_RaiseException and skipped.
    switch (cursor.step()) {
    case UNW_STEP_END:
    case UNW_STEP_FAILED:
      // End of stack or error condition.
      // Call the stop function one last time.
      action = (_Unwind_Action)(_UA_FORCE_UNWIND | _UA_CLEANUP_PHASE |
                                _UA_END_OF_STACK);
      (*stop)(1, action, exc->exception_class, exc,
              (struct _Unwind_Context *)(&cursor), stop_arg);

      // Didn't stop at the expected frame, so return error.
      return _URC_FATAL_PHASE2_ERROR;

    case UNW_STEP_SUCCESS:
      break;
    }

    unw_proc_info_t frameInfo;
    cursor.getInfo(&frameInfo);
    if (frameInfo.end_ip == 0)
      return _URC_FATAL_PHASE2_ERROR;

    // Call stop function for each frame
    action = (_Unwind_Action)(_UA_FORCE_UNWIND | _UA_CLEANUP_PHASE);
    _Unwind_Reason_Code result =
        (*stop)(1, action, exc->exception_class, exc,
                (struct _Unwind_Context *)(&cursor), stop_arg);
    if (result != _URC_NO_REASON)
      return _URC_FATAL_PHASE2_ERROR;

    if (frameInfo.handler == 0)
      continue; // No personality routine, continue.

    __personality_routine p = (__personality_routine)(frameInfo.handler);
    result = (*p)(1, action, exc->exception_class, exc,
                  (struct _Unwind_Context *)(&cursor));

    switch (result) {
    case _URC_CONTINUE_UNWIND:
      // Destructors called, continue.
      break;
    case _URC_INSTALL_CONTEXT:
      // Transfer control to landing pad.
      cursor.jumpto();
    default:
      // Bad personality routine.
      return _URC_FATAL_PHASE2_ERROR;
    }
  }
}

_Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception *exc) {
  NativeUnwindRegisters registers;
  ThisUnwindCursor cursor1(registers, sThisAddressSpace);
  ThisUnwindCursor cursor2(registers, sThisAddressSpace);

  // Mark this as a non-forced unwind for _Unwind_Resume().
  exc->private_1 = 0;
  exc->private_2 = 0;

  // Phase 1: searching.
  _Unwind_Reason_Code phase1 = unwind_phase1(cursor1, exc);
  if (phase1 != _URC_NO_REASON)
    return phase1;

  // Phase 2: cleaning up.
  return unwind_phase2(cursor2, exc);
}

_Unwind_Reason_Code _Unwind_ForcedUnwind(struct _Unwind_Exception *exc,
                                         _Unwind_Stop_Fn stop, void *stop_arg) {
  NativeUnwindRegisters registers;
  ThisUnwindCursor cursor(registers, sThisAddressSpace);

  // Mark this as forced unwind for _Unwind_Resume().
  exc->private_1 = (uintptr_t)stop;
  exc->private_2 = (uintptr_t)stop_arg;

  return unwind_phase2_forced(cursor, exc, stop, stop_arg);
}

void _Unwind_Resume(struct _Unwind_Exception *exc) {
  NativeUnwindRegisters registers;
  ThisUnwindCursor cursor(registers, sThisAddressSpace);

  if (exc->private_1 != 0)
    unwind_phase2_forced(cursor, exc, (_Unwind_Stop_Fn)exc->private_1,
                         (void *)exc->private_2);
  else
    unwind_phase2(cursor, exc);
  abort();
}

_Unwind_Reason_Code _Unwind_Resume_or_Rethrow(struct _Unwind_Exception *exc) {
  // This is a re-throw, if this is a non-forced unwind
  // and the stopping place was found.
  // In that case, call _Unwind_RaiseException() as if
  // it was a new exception.

  if (exc->private_1 != 0)
    _Unwind_Resume(exc);

  // This can return if there is no catch clause.
  // In that case, __cxa_rethrow is expected to call std::terminate().
  return _Unwind_RaiseException(exc);
}

void _Unwind_DeleteException(struct _Unwind_Exception *exc) {
  if (exc->exception_cleanup != NULL)
    (*exc->exception_cleanup)(_URC_FOREIGN_EXCEPTION_CAUGHT, exc);
}

uintptr_t _Unwind_GetGR(struct _Unwind_Context *context, int index) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  return cursor->getReg(index);
}

void _Unwind_SetGR(struct _Unwind_Context *context, int index,
                   uintptr_t new_value) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  cursor->setReg(index, new_value);
}

uintptr_t _Unwind_GetIP(struct _Unwind_Context *context) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  return cursor->getIP();
}

uintptr_t _Unwind_GetIPInfo(struct _Unwind_Context *context, int *isSignalFrame) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  *isSignalFrame = cursor->isSignalFrame() ? 1 : 0;
  return cursor->getIP();
}

void _Unwind_SetIP(struct _Unwind_Context *context, uintptr_t new_value) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  cursor->setIP(new_value);
  unw_proc_info_t info;
  cursor->getInfo(&info);
  cursor->setInfoBasedOnIPRegister(false);
}

uintptr_t _Unwind_GetRegionStart(struct _Unwind_Context *context) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  unw_proc_info_t frameInfo;
  cursor->getInfo(&frameInfo);
  return frameInfo.end_ip ? frameInfo.start_ip : 0;
}

uintptr_t _Unwind_GetLanguageSpecificData(struct _Unwind_Context *context) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  unw_proc_info_t frameInfo;
  cursor->getInfo(&frameInfo);
  return frameInfo.end_ip ? frameInfo.lsda : 0;
}

_Unwind_Reason_Code _Unwind_Backtrace(_Unwind_Trace_Fn callback, void *ref) {
  NativeUnwindRegisters registers;
  ThisUnwindCursor cursor(registers, sThisAddressSpace);
  cursor.setInfoBasedOnIPRegister();

  // Walk each frame.
  while (true) {

    // Ask libuwind to get next frame (skip over first frame which is
    // _Unwind_Backtrace()).
    if (cursor.step() != UNW_STEP_SUCCESS)
      return _URC_END_OF_STACK;

    // Call trace function with this frame.
    _Unwind_Reason_Code result =
        (*callback)((struct _Unwind_Context *)(&cursor), ref);
    if (result != _URC_NO_REASON)
      return result;
  }
}

uintptr_t _Unwind_GetCFA(struct _Unwind_Context *context) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  return cursor->getSP();
}

void *_Unwind_FindEnclosingFunction(void *pc) {
  NativeUnwindRegisters registers;
  ThisUnwindCursor cursor(registers, sThisAddressSpace);

  unw_proc_info_t info;
  cursor.setIP((uintptr_t)pc);
  cursor.setInfoBasedOnIPRegister();

  cursor.getInfo(&info);
  return info.end_ip ? (void *)info.start_ip : NULL;
}

void *_Unwind_Find_FDE(void *pc, struct dwarf_eh_bases *bases) {
  NativeUnwindRegisters registers;
  ThisUnwindCursor cursor(registers, sThisAddressSpace);

  unw_proc_info_t info;
  cursor.setIP((uintptr_t)pc);
  cursor.setInfoBasedOnIPRegister();

  cursor.getInfo(&info);
  if (info.end_ip == 0)
    return NULL;
  bases->tbase = 0; /* Not supported */
  bases->dbase = (void *)info.data_base;
  bases->func = (void *)info.start_ip;
  return (void *)info.unwind_info;
}

uintptr_t _Unwind_GetDataRelBase(struct _Unwind_Context *context) {
  ThisUnwindCursor *cursor = (ThisUnwindCursor *)context;
  unw_proc_info_t frameInfo;
  cursor->getInfo(&frameInfo);
  return frameInfo.data_base;
}

uintptr_t _Unwind_GetTextRelBase(struct _Unwind_Context *context) { return 0; }

void __register_frame(const void *fde) {
  MyCFIParser::pint_t pcStart, pcEnd;

  MyCFIParser::findPCRange(sThisAddressSpace, (uintptr_t)fde, pcStart, pcEnd);
  if (pcEnd == 0)
    return; // Bad FDE.

  sThisAddressSpace.addFDE(pcStart, pcEnd, (uintptr_t)fde);
}

void __register_frame_info(const void *ehframe, void *storage) {
  sThisAddressSpace.setLazyReload();
}

void __deregister_frame(const void *fde) {
  MyCFIParser::pint_t pcStart, pcEnd;

  MyCFIParser::findPCRange(sThisAddressSpace, (uintptr_t)fde, pcStart, pcEnd);
  if (pcEnd == 0)
    return; // Bad FDE.

  sThisAddressSpace.removeFDE(pcStart, pcEnd, (uintptr_t)fde);
}

void *__deregister_frame_info(const void *ehFrameStart) {
  sThisAddressSpace.removeDSO((LocalAddressSpace::pint_t)ehFrameStart);
  return NULL;
}
