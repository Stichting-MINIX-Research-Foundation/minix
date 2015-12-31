//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <istream>

// basic_istream<charT,traits>& get(basic_streambuf<char_type,traits>& sb);

#include <istream>
#include <cassert>

template <class CharT>
class testbuf
    : public std::basic_streambuf<CharT>
{
    typedef std::basic_streambuf<CharT> base;
    std::basic_string<CharT> str_;
public:
    testbuf()
    {
    }
    testbuf(const std::basic_string<CharT>& str)
        : str_(str)
    {
        base::setg(const_cast<CharT*>(str_.data()),
                   const_cast<CharT*>(str_.data()),
                   const_cast<CharT*>(str_.data() + str_.size()));
    }

    std::basic_string<CharT> str() const
        {return std::basic_string<CharT>(base::pbase(), base::pptr());}

protected:

    virtual typename base::int_type
        overflow(typename base::int_type __c = base::traits_type::eof())
        {
            if (__c != base::traits_type::eof())
            {
                int n = str_.size();
                str_.push_back(__c);
                str_.resize(str_.capacity());
                base::setp(const_cast<CharT*>(str_.data()),
                           const_cast<CharT*>(str_.data() + str_.size()));
                base::pbump(n+1);
            }
            return __c;
        }
};

int main()
{
    {
        testbuf<char> sb("testing\n...");
        std::istream is(&sb);
        testbuf<char> sb2;
        is.get(sb2);
        assert(sb2.str() == "testing");
        assert(is.good());
        assert(is.gcount() == 7);
        assert(is.get() == '\n');
        is.get(sb2);
        assert(sb2.str() == "testing...");
        assert(is.eof());
        assert(!is.fail());
        assert(is.gcount() == 3);
    }
    {
        testbuf<wchar_t> sb(L"testing\n...");
        std::wistream is(&sb);
        testbuf<wchar_t> sb2;
        is.get(sb2);
        assert(sb2.str() == L"testing");
        assert(is.good());
        assert(is.gcount() == 7);
        assert(is.get() == L'\n');
        is.get(sb2);
        assert(sb2.str() == L"testing...");
        assert(is.eof());
        assert(!is.fail());
        assert(is.gcount() == 3);
    }
}
