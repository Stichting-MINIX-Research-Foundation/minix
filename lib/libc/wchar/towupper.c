/*
 * Very lame implementation of towupper that simply applies toupper
 * if the character is within range of 'normal' characters.
 */

#include <wchar.h>
#include <ctype.h>

wint_t towupper(wint_t c)
{
    if( c>0xff ){
        return c;
    }
    return (wint_t) toupper((char) c);
}
