/* -*- indented-text -*- */
%option noinput
%option nounput
%{
#include "y.tab.h"
#include <string.h>


#if 0
#define ECHO fwrite(yytext, yyleng, 1, yyout)
#else
#define ECHO
#endif
%}
BO              [{]
BC              [}]
NL              [\n]
SC              [;]
CHAR            [0-9a-zA-Z_/\-\*\.]
EQ              [=]
SPACE           [\032]
%%

usb_driver         { ECHO; return USB_DRIVER;}
devprefix          { ECHO; return DEV_PREFIX;}
devtype            { ECHO; return DEV_TYPE;}
char               { ECHO; return BLOCK_DEV;}
block              { ECHO; return CHAR_DEV;}
binary             { ECHO; return BINARY;}
bInterfaceClass    { ECHO; return INTERFACE_CLASS;}
bInterfaceSubClass { ECHO; return INTERFACE_SUB_CLASS;}
bInterfaceProtocol { ECHO; return INTERFACE_PROTOCOL;}
id                 { ECHO; return ID;}
upscript           { ECHO; return UPSCRIPT;}
downscript         { ECHO; return DOWNSCRIPT;}
{EQ}               { ECHO; return EQUALS;}
{SC}               { ECHO; return SEMICOLON;}
{BO}               { ECHO; return BRACKET_OPEN;}
{BC}               { ECHO; return BRACKET_CLOSE;}
{CHAR}+            { ECHO; yylval.string = (char *)strdup(yytext); return STRING;}
.               ;  
%%

