//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// REQUIRES: libcpp-has-no-thread-unsafe-c-functions

#include <ctime>

int main() {
    // asctime is not thread-safe.
    std::time_t t = 0;
    std::asctime(&t);
}
