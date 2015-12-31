//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <regex>

// class regex_iterator<BidirectionalIterator, charT, traits>

// regex_iterator operator++(int);

#include <regex>
#include <cassert>

int main()
{
    {
        std::regex phone_numbers("\\d{3}-\\d{4}");
        const char phone_book[] = "555-1234, 555-2345, 555-3456";
        std::cregex_iterator i(std::begin(phone_book), std::end(phone_book), phone_numbers);
        std::cregex_iterator i2 = i;
        assert(i != std::cregex_iterator());
        assert(i2!= std::cregex_iterator());
        assert((*i).size() == 1);
        assert((*i).position() == 0);
        assert((*i).str() == "555-1234");
        assert((*i2).size() == 1);
        assert((*i2).position() == 0);
        assert((*i2).str() == "555-1234");
        i++;
        assert(i != std::cregex_iterator());
        assert(i2!= std::cregex_iterator());
        assert((*i).size() == 1);
        assert((*i).position() == 10);
        assert((*i).str() == "555-2345");
        assert((*i2).size() == 1);
        assert((*i2).position() == 0);
        assert((*i2).str() == "555-1234");
        i++;
        assert(i != std::cregex_iterator());
        assert(i2!= std::cregex_iterator());
        assert((*i).size() == 1);
        assert((*i).position() == 20);
        assert((*i).str() == "555-3456");
        assert((*i2).size() == 1);
        assert((*i2).position() == 0);
        assert((*i2).str() == "555-1234");
        i++;
        assert(i == std::cregex_iterator());
        assert(i2!= std::cregex_iterator());
        assert((*i2).size() == 1);
        assert((*i2).position() == 0);
        assert((*i2).str() == "555-1234");
    }
    {
        std::regex phone_numbers("\\d{3}-\\d{4}");
        const char phone_book[] = "555-1234, 555-2345, 555-3456";
        std::cregex_iterator i(std::begin(phone_book), std::end(phone_book), phone_numbers);
        std::cregex_iterator i2 = i;
        assert(i != std::cregex_iterator());
        assert(i2!= std::cregex_iterator());
        assert((*i).size() == 1);
        assert((*i).position() == 0);
        assert((*i).str() == "555-1234");
        assert((*i2).size() == 1);
        assert((*i2).position() == 0);
        assert((*i2).str() == "555-1234");
        ++i;
        assert(i != std::cregex_iterator());
        assert(i2!= std::cregex_iterator());
        assert((*i).size() == 1);
        assert((*i).position() == 10);
        assert((*i).str() == "555-2345");
        assert((*i2).size() == 1);
        assert((*i2).position() == 0);
        assert((*i2).str() == "555-1234");
        ++i;
        assert(i != std::cregex_iterator());
        assert(i2!= std::cregex_iterator());
        assert((*i).size() == 1);
        assert((*i).position() == 20);
        assert((*i).str() == "555-3456");
        assert((*i2).size() == 1);
        assert((*i2).position() == 0);
        assert((*i2).str() == "555-1234");
        ++i;
        assert(i == std::cregex_iterator());
        assert(i2!= std::cregex_iterator());
        assert((*i2).size() == 1);
        assert((*i2).position() == 0);
        assert((*i2).str() == "555-1234");
    }
}
