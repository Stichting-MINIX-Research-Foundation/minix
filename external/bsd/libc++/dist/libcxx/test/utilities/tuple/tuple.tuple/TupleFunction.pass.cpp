//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// This is for bugs 18853 and 19118

#if __cplusplus >= 201103L

#include <tuple>
#include <functional>

struct X
{
    X() {}

    template <class T>
    X(T);

    void operator()() {}
};

int
main()
{
    X x;
    std::function<void()> f(x);
}
#else
int main () {}
#endif
