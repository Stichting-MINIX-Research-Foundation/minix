//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++98, c++03, c++11

// <experimental/any>

// template <class ValueType>
// ValueType any_cast(any const &);

// Try and cast away const.

#include <experimental/any>

struct TestType {};
struct TestType2 {};

int main()
{
    using std::experimental::any;
    using std::experimental::any_cast;

    any a;

    // expected-error@experimental/any:* 2 {{binding value of type '_Tp' (aka 'const TestType') to reference to type 'TestType' drops 'const' qualifier}}
    any_cast<TestType &>(static_cast<any const&>(a)); // expected-note {{requested here}}
    any_cast<TestType &&>(static_cast<any const&>(a)); // expected-note {{requested here}}

    // expected-error@experimental/any:* 2 {{binding value of type '_Tp' (aka 'const TestType2') to reference to type 'TestType2' drops 'const' qualifier}}
    any_cast<TestType2 &>(static_cast<any const&&>(a)); // expected-note {{requested here}}
    any_cast<TestType2 &&>(static_cast<any const&&>(a)); // expected-note {{requested here}}
}
