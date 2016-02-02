//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// utilities

// exchange

#include <utility>
#include <cassert>
#include <string>

int main()
{
#if _LIBCPP_STD_VER > 11
    {
    int v = 12;
    assert ( std::exchange ( v, 23 ) == 12 );
    assert ( v == 23 );
    assert ( std::exchange ( v, 67.2 ) == 23 );
    assert ( v == 67 );

    assert ((std::exchange<int, float> ( v, {} )) == 67 );
    assert ( v == 0 );
    
    }

    {
    bool b = false;
    assert ( !std::exchange ( b, true ));
    assert ( b );
    }

    {
    const std::string s1 ( "Hi Mom!" );
    const std::string s2 ( "Yo Dad!" );
    std::string s3 = s1; // Mom
    assert ( std::exchange ( s3, s2 ) == s1 );
    assert ( s3 == s2 );
    assert ( std::exchange ( s3, "Hi Mom!" ) == s2 );
    assert ( s3 == s1 );

    s3 = s2; // Dad
    assert ( std::exchange ( s3, {} ) == s2 );
    assert ( s3.size () == 0 );
    
    s3 = s2; // Dad
    assert ( std::exchange ( s3, "" ) == s2 );
    assert ( s3.size () == 0 );
    }

#endif
}
