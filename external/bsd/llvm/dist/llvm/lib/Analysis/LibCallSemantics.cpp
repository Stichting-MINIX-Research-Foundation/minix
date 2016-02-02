//===- LibCallSemantics.cpp - Describe library semantics ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements interfaces that can be used to describe language
// specific runtime library interfaces (e.g. libc, libm, etc) to LLVM
// optimizers.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/LibCallSemantics.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Function.h"
using namespace llvm;

/// This impl pointer in ~LibCallInfo is actually a StringMap.  This
/// helper does the cast.
static StringMap<const LibCallFunctionInfo*> *getMap(void *Ptr) {
  return static_cast<StringMap<const LibCallFunctionInfo*> *>(Ptr);
}

LibCallInfo::~LibCallInfo() {
  delete getMap(Impl);
}

const LibCallLocationInfo &LibCallInfo::getLocationInfo(unsigned LocID) const {
  // Get location info on the first call.
  if (NumLocations == 0)
    NumLocations = getLocationInfo(Locations);
  
  assert(LocID < NumLocations && "Invalid location ID!");
  return Locations[LocID];
}


/// Return the LibCallFunctionInfo object corresponding to
/// the specified function if we have it.  If not, return null.
const LibCallFunctionInfo *
LibCallInfo::getFunctionInfo(const Function *F) const {
  StringMap<const LibCallFunctionInfo*> *Map = getMap(Impl);
  
  /// If this is the first time we are querying for this info, lazily construct
  /// the StringMap to index it.
  if (!Map) {
    Impl = Map = new StringMap<const LibCallFunctionInfo*>();
    
    const LibCallFunctionInfo *Array = getFunctionInfoArray();
    if (!Array) return nullptr;
    
    // We now have the array of entries.  Populate the StringMap.
    for (unsigned i = 0; Array[i].Name; ++i)
      (*Map)[Array[i].Name] = Array+i;
  }
  
  // Look up this function in the string map.
  return Map->lookup(F->getName());
}

