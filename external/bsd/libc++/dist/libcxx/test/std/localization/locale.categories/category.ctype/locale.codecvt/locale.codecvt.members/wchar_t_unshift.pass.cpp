//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <locale>

// template <> class codecvt<wchar_t, char, mbstate_t>

// result unshift(stateT& state,
//                externT* to, externT* to_end, externT*& to_next) const;

// This is pretty much just an "are you breathing" test

#include <locale>
#include <string>
#include <vector>
#include <cassert>

typedef std::codecvt<wchar_t, char, std::mbstate_t> F;

int main()
{
    std::locale l = std::locale::classic();
    std::vector<F::extern_type> to(3);
    const F& f = std::use_facet<F>(l);
    std::mbstate_t mbs = {0};
    F::extern_type* to_next = 0;
    assert(f.unshift(mbs, to.data(), to.data() + to.size(), to_next) == F::ok);
    assert(to_next == to.data());
}
