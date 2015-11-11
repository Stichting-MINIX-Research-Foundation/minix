
#ifndef _PASS_H
#define _PASS_H

#include <set>
#include <map>

#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/AliasAnalysis.h>

#include <llvm/Support/Debug.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ADT/Statistic.h>

#include <llvm/Support/Regex.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Analysis/LoopInfo.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/Transforms/Utils/Local.h>

#include <llvm/Transforms/Scalar.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include <common/pass_common.h>

#endif /* _PASS_H */
