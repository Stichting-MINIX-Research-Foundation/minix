// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fmodules-cache-path=%t %s -I%S/Inputs/cxx-lookup -verify
// expected-no-diagnostics
namespace llvm {}
#include "c2.h"
llvm::GlobalValue *p;
