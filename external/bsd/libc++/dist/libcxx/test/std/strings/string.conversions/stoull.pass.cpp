//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// XFAIL: with_system_cxx_lib=x86_64-apple-darwin11
// XFAIL: with_system_cxx_lib=x86_64-apple-darwin12

// <string>

// unsigned long long stoull(const string& str, size_t *idx = 0, int base = 10);
// unsigned long long stoull(const wstring& str, size_t *idx = 0, int base = 10);

#include <string>
#include <cassert>

int main()
{
    assert(std::stoull("0") == 0);
    assert(std::stoull(L"0") == 0);
    assert(std::stoull("-0") == 0);
    assert(std::stoull(L"-0") == 0);
    assert(std::stoull(" 10") == 10);
    assert(std::stoull(L" 10") == 10);
    size_t idx = 0;
    assert(std::stoull("10g", &idx, 16) == 16);
    assert(idx == 2);
    idx = 0;
    assert(std::stoull(L"10g", &idx, 16) == 16);
    assert(idx == 2);
    idx = 0;
    try
    {
        std::stoull("", &idx);
        assert(false);
    }
    catch (const std::invalid_argument&)
    {
        assert(idx == 0);
    }
    idx = 0;
    try
    {
        std::stoull(L"", &idx);
        assert(false);
    }
    catch (const std::invalid_argument&)
    {
        assert(idx == 0);
    }
    try
    {
        std::stoull("  - 8", &idx);
        assert(false);
    }
    catch (const std::invalid_argument&)
    {
        assert(idx == 0);
    }
    try
    {
        std::stoull(L"  - 8", &idx);
        assert(false);
    }
    catch (const std::invalid_argument&)
    {
        assert(idx == 0);
    }
    try
    {
        std::stoull("a1", &idx);
        assert(false);
    }
    catch (const std::invalid_argument&)
    {
        assert(idx == 0);
    }
    try
    {
        std::stoull(L"a1", &idx);
        assert(false);
    }
    catch (const std::invalid_argument&)
    {
        assert(idx == 0);
    }
//  LWG issue #2009
    try
    {
        std::stoull("9999999999999999999999999999999999999999999999999", &idx);
        assert(false);
    }
    catch (const std::out_of_range&)
    {
        assert(idx == 0);
    }
    try
    {
        std::stoull(L"9999999999999999999999999999999999999999999999999", &idx);
        assert(false);
    }
    catch (const std::out_of_range&)
    {
        assert(idx == 0);
    }
}
