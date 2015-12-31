//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// dynarray.zero

// dynarray shall provide support for the special case of construction with a size of zero.
// In the case that the size is zero, begin() == end() == unique value. 
// The return value of data() is unspecified. 
// The effect of calling front() or back() for a zero-sized dynarray is undefined.


  
#include <__config>

#if _LIBCPP_STD_VER > 11

#include <experimental/dynarray>
#include <cassert>

#include <algorithm>
#include <complex>
#include <string>

using std::experimental::dynarray;

template <class T>
void test ( ) {
    typedef dynarray<T> dynA;
    
    dynA d1 ( 0 );
    assert ( d1.size() == 0 );
    assert ( d1.begin() == d1.end ());
    }

int main()
{
    test<int> ();
    test<double> ();
    test<std::complex<double>> ();
    test<std::string> ();
}
#else
int main() {}
#endif
