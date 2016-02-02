//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


// <string_view>

// template<class Allocator>
// basic_string_view(const basic_string<_CharT, _Traits, Allocator>& _str) noexcept


#include <experimental/string_view>
#include <string>
#include <cassert>

struct dummy_char_traits : public std::char_traits<char> {};

template<typename CharT, typename Traits>
void test ( const std::basic_string<CharT, Traits> &str ) {
    std::experimental::basic_string_view<CharT, Traits> sv1 ( str );
    assert ( sv1.size() == str.size());
    assert ( sv1.data() == str.data());
}

int main () {

    test ( std::string("QBCDE") );
    test ( std::string("") );
    test ( std::string() );
    
    test ( std::wstring(L"QBCDE") );
    test ( std::wstring(L"") );
    test ( std::wstring() );

#if __cplusplus >= 201103L
    test ( std::u16string{u"QBCDE"} );
    test ( std::u16string{u""} );
    test ( std::u16string{} );

    test ( std::u32string{U"QBCDE"} );
    test ( std::u32string{U""} );
    test ( std::u32string{} );
#endif
    
    test ( std::basic_string<char, dummy_char_traits>("QBCDE") );
    test ( std::basic_string<char, dummy_char_traits>("") );
    test ( std::basic_string<char, dummy_char_traits>() );

}
