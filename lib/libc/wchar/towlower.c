/*
 * Very lame implementation of towlower that simply applies tolower
 * if the character is within range of 'normal' characters.
 */

#include <wchar.h>
#include <ctype.h>

wint_t towlower(wint_t c)
{
    if( c>0xff ){
        return c;
    }
    return (wint_t) tolower((char) c);
}
