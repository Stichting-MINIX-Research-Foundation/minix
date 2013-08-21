/* C code produced by gperf version 3.0.4 */
/* Command-line: gperf --output-file atoms.c atoms.gperf  */
/* Computed positions: -k'3,6,9,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "atoms.gperf"


/* Rely on vasprintf (GNU extension) instead of vsnprintf if
   possible... */
#ifdef HAVE_VASPRINTF
#define _GNU_SOURCE
#include <stdio.h>
#endif

#include <xcb/xcb.h>
#include <stdlib.h>
#include <stdarg.h>
#include "xcb_atom.h"

const xcb_atom_t PRIMARY = 1;
const xcb_atom_t SECONDARY = 2;
const xcb_atom_t ARC = 3;
const xcb_atom_t ATOM = 4;
const xcb_atom_t BITMAP = 5;
const xcb_atom_t CARDINAL = 6;
const xcb_atom_t COLORMAP = 7;
const xcb_atom_t CURSOR = 8;
const xcb_atom_t CUT_BUFFER0 = 9;
const xcb_atom_t CUT_BUFFER1 = 10;
const xcb_atom_t CUT_BUFFER2 = 11;
const xcb_atom_t CUT_BUFFER3 = 12;
const xcb_atom_t CUT_BUFFER4 = 13;
const xcb_atom_t CUT_BUFFER5 = 14;
const xcb_atom_t CUT_BUFFER6 = 15;
const xcb_atom_t CUT_BUFFER7 = 16;
const xcb_atom_t DRAWABLE = 17;
const xcb_atom_t FONT = 18;
const xcb_atom_t INTEGER = 19;
const xcb_atom_t PIXMAP = 20;
const xcb_atom_t POINT = 21;
const xcb_atom_t RECTANGLE = 22;
const xcb_atom_t RESOURCE_MANAGER = 23;
const xcb_atom_t RGB_COLOR_MAP = 24;
const xcb_atom_t RGB_BEST_MAP = 25;
const xcb_atom_t RGB_BLUE_MAP = 26;
const xcb_atom_t RGB_DEFAULT_MAP = 27;
const xcb_atom_t RGB_GRAY_MAP = 28;
const xcb_atom_t RGB_GREEN_MAP = 29;
const xcb_atom_t RGB_RED_MAP = 30;
const xcb_atom_t STRING = 31;
const xcb_atom_t VISUALID = 32;
const xcb_atom_t WINDOW = 33;
const xcb_atom_t WM_COMMAND = 34;
const xcb_atom_t WM_HINTS = 35;
const xcb_atom_t WM_CLIENT_MACHINE = 36;
const xcb_atom_t WM_ICON_NAME = 37;
const xcb_atom_t WM_ICON_SIZE = 38;
const xcb_atom_t WM_NAME = 39;
const xcb_atom_t WM_NORMAL_HINTS = 40;
const xcb_atom_t WM_SIZE_HINTS = 41;
const xcb_atom_t WM_ZOOM_HINTS = 42;
const xcb_atom_t MIN_SPACE = 43;
const xcb_atom_t NORM_SPACE = 44;
const xcb_atom_t MAX_SPACE = 45;
const xcb_atom_t END_SPACE = 46;
const xcb_atom_t SUPERSCRIPT_X = 47;
const xcb_atom_t SUPERSCRIPT_Y = 48;
const xcb_atom_t SUBSCRIPT_X = 49;
const xcb_atom_t SUBSCRIPT_Y = 50;
const xcb_atom_t UNDERLINE_POSITION = 51;
const xcb_atom_t UNDERLINE_THICKNESS = 52;
const xcb_atom_t STRIKEOUT_ASCENT = 53;
const xcb_atom_t STRIKEOUT_DESCENT = 54;
const xcb_atom_t ITALIC_ANGLE = 55;
const xcb_atom_t X_HEIGHT = 56;
const xcb_atom_t QUAD_WIDTH = 57;
const xcb_atom_t WEIGHT = 58;
const xcb_atom_t POINT_SIZE = 59;
const xcb_atom_t RESOLUTION = 60;
const xcb_atom_t COPYRIGHT = 61;
const xcb_atom_t NOTICE = 62;
const xcb_atom_t FONT_NAME = 63;
const xcb_atom_t FAMILY_NAME = 64;
const xcb_atom_t FULL_NAME = 65;
const xcb_atom_t CAP_HEIGHT = 66;
const xcb_atom_t WM_CLASS = 67;
const xcb_atom_t WM_TRANSIENT_FOR = 68;
#line 93 "atoms.gperf"
struct atom_map { int name; xcb_atom_t value; };
#include <string.h>
/* maximum key range = 146, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (str, len)
     register const char *str;
     register unsigned int len;
{
  static const unsigned char asso_values[] =
    {
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150,  75,  70,
       65,  60,  40,  35,  20,   5, 150, 150, 150, 150,
      150, 150, 150, 150, 150,  35,   0,  45,  15,   0,
      150,  50,   0,   5, 150, 150,  15,  35,   0,  40,
        5, 150,  10,  15,   0,  25, 150,  20,  70,  40,
       55, 150, 150, 150, 150,  15, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
      150, 150, 150, 150, 150, 150
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[8]];
      /*FALLTHROUGH*/
      case 8:
      case 7:
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
  {
    char stringpool_str4[sizeof("FONT")];
    char stringpool_str6[sizeof("NOTICE")];
    char stringpool_str9[sizeof("FONT_NAME")];
    char stringpool_str10[sizeof("POINT")];
    char stringpool_str11[sizeof("WEIGHT")];
    char stringpool_str14[sizeof("MIN_SPACE")];
    char stringpool_str15[sizeof("CAP_HEIGHT")];
    char stringpool_str16[sizeof("BITMAP")];
    char stringpool_str17[sizeof("INTEGER")];
    char stringpool_str19[sizeof("COPYRIGHT")];
    char stringpool_str24[sizeof("FULL_NAME")];
    char stringpool_str26[sizeof("STRIKEOUT_ASCENT")];
    char stringpool_str27[sizeof("STRIKEOUT_DESCENT")];
    char stringpool_str28[sizeof("RGB_GREEN_MAP")];
    char stringpool_str29[sizeof("END_SPACE")];
    char stringpool_str32[sizeof("RGB_BEST_MAP")];
    char stringpool_str33[sizeof("CARDINAL")];
    char stringpool_str36[sizeof("CURSOR")];
    char stringpool_str37[sizeof("WM_CLIENT_MACHINE")];
    char stringpool_str38[sizeof("WM_HINTS")];
    char stringpool_str41[sizeof("CUT_BUFFER7")];
    char stringpool_str42[sizeof("RGB_GRAY_MAP")];
    char stringpool_str43[sizeof("DRAWABLE")];
    char stringpool_str45[sizeof("RGB_DEFAULT_MAP")];
    char stringpool_str46[sizeof("WINDOW")];
    char stringpool_str47[sizeof("RGB_BLUE_MAP")];
    char stringpool_str48[sizeof("UNDERLINE_POSITION")];
    char stringpool_str51[sizeof("RGB_RED_MAP")];
    char stringpool_str53[sizeof("VISUALID")];
    char stringpool_str54[sizeof("RECTANGLE")];
    char stringpool_str56[sizeof("CUT_BUFFER6")];
    char stringpool_str57[sizeof("WM_NAME")];
    char stringpool_str58[sizeof("X_HEIGHT")];
    char stringpool_str61[sizeof("SUBSCRIPT_Y")];
    char stringpool_str62[sizeof("PRIMARY")];
    char stringpool_str63[sizeof("COLORMAP")];
    char stringpool_str64[sizeof("UNDERLINE_THICKNESS")];
    char stringpool_str65[sizeof("QUAD_WIDTH")];
    char stringpool_str66[sizeof("RESOURCE_MANAGER")];
    char stringpool_str67[sizeof("WM_ICON_NAME")];
    char stringpool_str68[sizeof("RGB_COLOR_MAP")];
    char stringpool_str70[sizeof("WM_NORMAL_HINTS")];
    char stringpool_str71[sizeof("CUT_BUFFER5")];
    char stringpool_str73[sizeof("WM_CLASS")];
    char stringpool_str75[sizeof("WM_COMMAND")];
    char stringpool_str76[sizeof("CUT_BUFFER4")];
    char stringpool_str78[sizeof("SUPERSCRIPT_Y")];
    char stringpool_str79[sizeof("ATOM")];
    char stringpool_str80[sizeof("NORM_SPACE")];
    char stringpool_str81[sizeof("WM_TRANSIENT_FOR")];
    char stringpool_str82[sizeof("WM_ICON_SIZE")];
    char stringpool_str83[sizeof("WM_ZOOM_HINTS")];
    char stringpool_str84[sizeof("MAX_SPACE")];
    char stringpool_str85[sizeof("POINT_SIZE")];
    char stringpool_str86[sizeof("PIXMAP")];
    char stringpool_str90[sizeof("RESOLUTION")];
    char stringpool_str91[sizeof("SUBSCRIPT_X")];
    char stringpool_str92[sizeof("ITALIC_ANGLE")];
    char stringpool_str93[sizeof("ARC")];
    char stringpool_str96[sizeof("CUT_BUFFER3")];
    char stringpool_str98[sizeof("WM_SIZE_HINTS")];
    char stringpool_str101[sizeof("CUT_BUFFER2")];
    char stringpool_str106[sizeof("CUT_BUFFER1")];
    char stringpool_str108[sizeof("SUPERSCRIPT_X")];
    char stringpool_str111[sizeof("CUT_BUFFER0")];
    char stringpool_str116[sizeof("STRING")];
    char stringpool_str121[sizeof("FAMILY_NAME")];
    char stringpool_str149[sizeof("SECONDARY")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "FONT",
    "NOTICE",
    "FONT_NAME",
    "POINT",
    "WEIGHT",
    "MIN_SPACE",
    "CAP_HEIGHT",
    "BITMAP",
    "INTEGER",
    "COPYRIGHT",
    "FULL_NAME",
    "STRIKEOUT_ASCENT",
    "STRIKEOUT_DESCENT",
    "RGB_GREEN_MAP",
    "END_SPACE",
    "RGB_BEST_MAP",
    "CARDINAL",
    "CURSOR",
    "WM_CLIENT_MACHINE",
    "WM_HINTS",
    "CUT_BUFFER7",
    "RGB_GRAY_MAP",
    "DRAWABLE",
    "RGB_DEFAULT_MAP",
    "WINDOW",
    "RGB_BLUE_MAP",
    "UNDERLINE_POSITION",
    "RGB_RED_MAP",
    "VISUALID",
    "RECTANGLE",
    "CUT_BUFFER6",
    "WM_NAME",
    "X_HEIGHT",
    "SUBSCRIPT_Y",
    "PRIMARY",
    "COLORMAP",
    "UNDERLINE_THICKNESS",
    "QUAD_WIDTH",
    "RESOURCE_MANAGER",
    "WM_ICON_NAME",
    "RGB_COLOR_MAP",
    "WM_NORMAL_HINTS",
    "CUT_BUFFER5",
    "WM_CLASS",
    "WM_COMMAND",
    "CUT_BUFFER4",
    "SUPERSCRIPT_Y",
    "ATOM",
    "NORM_SPACE",
    "WM_TRANSIENT_FOR",
    "WM_ICON_SIZE",
    "WM_ZOOM_HINTS",
    "MAX_SPACE",
    "POINT_SIZE",
    "PIXMAP",
    "RESOLUTION",
    "SUBSCRIPT_X",
    "ITALIC_ANGLE",
    "ARC",
    "CUT_BUFFER3",
    "WM_SIZE_HINTS",
    "CUT_BUFFER2",
    "CUT_BUFFER1",
    "SUPERSCRIPT_X",
    "CUT_BUFFER0",
    "STRING",
    "FAMILY_NAME",
    "SECONDARY"
  };
#define stringpool ((const char *) &stringpool_contents)
static
#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct atom_map *
in_word_set (str, len)
     register const char *str;
     register unsigned int len;
{
  enum
    {
      TOTAL_KEYWORDS = 68,
      MIN_WORD_LENGTH = 3,
      MAX_WORD_LENGTH = 19,
      MIN_HASH_VALUE = 4,
      MAX_HASH_VALUE = 149
    };

  static const struct atom_map wordlist[] =
    {
      {-1}, {-1}, {-1}, {-1},
#line 112 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str4,18},
      {-1},
#line 156 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str6,62},
      {-1}, {-1},
#line 157 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str9,63},
#line 115 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str10,21},
#line 152 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str11,58},
      {-1}, {-1},
#line 137 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str14,43},
#line 160 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str15,66},
#line 99 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str16,5},
#line 113 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str17,19},
      {-1},
#line 155 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str19,61},
      {-1}, {-1}, {-1}, {-1},
#line 159 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str24,65},
      {-1},
#line 147 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str26,53},
#line 148 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str27,54},
#line 123 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str28,29},
#line 140 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str29,46},
      {-1}, {-1},
#line 119 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str32,25},
#line 100 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str33,6},
      {-1}, {-1},
#line 102 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str36,8},
#line 130 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str37,36},
#line 129 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str38,35},
      {-1}, {-1},
#line 110 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str41,16},
#line 122 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str42,28},
#line 111 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str43,17},
      {-1},
#line 121 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str45,27},
#line 127 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str46,33},
#line 120 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str47,26},
#line 145 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str48,51},
      {-1}, {-1},
#line 124 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str51,30},
      {-1},
#line 126 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str53,32},
#line 116 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str54,22},
      {-1},
#line 109 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str56,15},
#line 133 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str57,39},
#line 150 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str58,56},
      {-1}, {-1},
#line 144 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str61,50},
#line 95 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str62,1},
#line 101 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str63,7},
#line 146 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str64,52},
#line 151 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str65,57},
#line 117 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str66,23},
#line 131 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str67,37},
#line 118 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str68,24},
      {-1},
#line 134 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str70,40},
#line 108 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str71,14},
      {-1},
#line 161 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str73,67},
      {-1},
#line 128 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str75,34},
#line 107 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str76,13},
      {-1},
#line 142 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str78,48},
#line 98 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str79,4},
#line 138 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str80,44},
#line 162 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str81,68},
#line 132 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str82,38},
#line 136 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str83,42},
#line 139 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str84,45},
#line 153 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str85,59},
#line 114 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str86,20},
      {-1}, {-1}, {-1},
#line 154 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str90,60},
#line 143 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str91,49},
#line 149 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str92,55},
#line 97 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str93,3},
      {-1}, {-1},
#line 106 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str96,12},
      {-1},
#line 135 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str98,41},
      {-1}, {-1},
#line 105 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str101,11},
      {-1}, {-1}, {-1}, {-1},
#line 104 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str106,10},
      {-1},
#line 141 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str108,47},
      {-1}, {-1},
#line 103 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str111,9},
      {-1}, {-1}, {-1}, {-1},
#line 125 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str116,31},
      {-1}, {-1}, {-1}, {-1},
#line 158 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str121,64},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 96 "atoms.gperf"
      {(int)(long)&((struct stringpool_t *)0)->stringpool_str149,2}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int o = wordlist[key].name;
          if (o >= 0)
            {
              register const char *s = o + stringpool;

              if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
                return &wordlist[key];
            }
        }
    }
  return 0;
}
#line 163 "atoms.gperf"


static const char atom_names[] =
	"PRIMARY\0"
	"SECONDARY\0"
	"ARC\0"
	"ATOM\0"
	"BITMAP\0"
	"CARDINAL\0"
	"COLORMAP\0"
	"CURSOR\0"
	"CUT_BUFFER0\0"
	"CUT_BUFFER1\0"
	"CUT_BUFFER2\0"
	"CUT_BUFFER3\0"
	"CUT_BUFFER4\0"
	"CUT_BUFFER5\0"
	"CUT_BUFFER6\0"
	"CUT_BUFFER7\0"
	"DRAWABLE\0"
	"FONT\0"
	"INTEGER\0"
	"PIXMAP\0"
	"POINT\0"
	"RECTANGLE\0"
	"RESOURCE_MANAGER\0"
	"RGB_COLOR_MAP\0"
	"RGB_BEST_MAP\0"
	"RGB_BLUE_MAP\0"
	"RGB_DEFAULT_MAP\0"
	"RGB_GRAY_MAP\0"
	"RGB_GREEN_MAP\0"
	"RGB_RED_MAP\0"
	"STRING\0"
	"VISUALID\0"
	"WINDOW\0"
	"WM_COMMAND\0"
	"WM_HINTS\0"
	"WM_CLIENT_MACHINE\0"
	"WM_ICON_NAME\0"
	"WM_ICON_SIZE\0"
	"WM_NAME\0"
	"WM_NORMAL_HINTS\0"
	"WM_SIZE_HINTS\0"
	"WM_ZOOM_HINTS\0"
	"MIN_SPACE\0"
	"NORM_SPACE\0"
	"MAX_SPACE\0"
	"END_SPACE\0"
	"SUPERSCRIPT_X\0"
	"SUPERSCRIPT_Y\0"
	"SUBSCRIPT_X\0"
	"SUBSCRIPT_Y\0"
	"UNDERLINE_POSITION\0"
	"UNDERLINE_THICKNESS\0"
	"STRIKEOUT_ASCENT\0"
	"STRIKEOUT_DESCENT\0"
	"ITALIC_ANGLE\0"
	"X_HEIGHT\0"
	"QUAD_WIDTH\0"
	"WEIGHT\0"
	"POINT_SIZE\0"
	"RESOLUTION\0"
	"COPYRIGHT\0"
	"NOTICE\0"
	"FONT_NAME\0"
	"FAMILY_NAME\0"
	"FULL_NAME\0"
	"CAP_HEIGHT\0"
	"WM_CLASS\0"
	"WM_TRANSIENT_FOR\0"
;

static const uint16_t atom_name_offsets[] = {
	0,
	8,
	18,
	22,
	27,
	34,
	43,
	52,
	59,
	71,
	83,
	95,
	107,
	119,
	131,
	143,
	155,
	164,
	169,
	177,
	184,
	190,
	200,
	217,
	231,
	244,
	257,
	273,
	286,
	300,
	312,
	319,
	328,
	335,
	346,
	355,
	373,
	386,
	399,
	407,
	423,
	437,
	451,
	461,
	472,
	482,
	492,
	506,
	520,
	532,
	544,
	563,
	583,
	600,
	618,
	631,
	640,
	651,
	658,
	669,
	680,
	690,
	697,
	707,
	719,
	729,
	740,
	749,
};

xcb_atom_t xcb_atom_get(xcb_connection_t *connection, const char *atom_name)
{
	if(atom_name == NULL)
		return XCB_NONE;
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection,
		xcb_intern_atom(connection, 0, strlen(atom_name), atom_name), NULL);
	if(!reply)
		return XCB_NONE;
	xcb_atom_t atom = reply->atom;
	free(reply);
	return atom;
}

xcb_atom_t xcb_atom_get_predefined(uint16_t name_len, const char *name)
{
	const struct atom_map *value = in_word_set(name, name_len);
	xcb_atom_t ret = XCB_NONE;
	if(value)
		ret = value->value;
	return ret;
}

xcb_atom_fast_cookie_t xcb_atom_get_fast(xcb_connection_t *c, uint8_t only_if_exists, uint16_t name_len, const char *name)
{
	xcb_atom_fast_cookie_t cookie;

	if((cookie.u.atom = xcb_atom_get_predefined(name_len, name)) != XCB_NONE)
	{
		cookie.tag = TAG_VALUE;
		return cookie;
	}

	cookie.tag = TAG_COOKIE;
	cookie.u.cookie = xcb_intern_atom(c, only_if_exists, name_len, name);
	return cookie;
}

xcb_atom_t xcb_atom_get_fast_reply(xcb_connection_t *c, xcb_atom_fast_cookie_t cookie, xcb_generic_error_t **e)
{
	switch(cookie.tag)
	{
		xcb_intern_atom_reply_t *reply;
	case TAG_VALUE:
		if(e)
			*e = 0;
		break;
	case TAG_COOKIE:
		reply = xcb_intern_atom_reply(c, cookie.u.cookie, e);
		if(reply)
		{
			cookie.u.atom = reply->atom;
			free(reply);
		}
		else
			cookie.u.atom = XCB_NONE;
		break;
	}
	return cookie.u.atom;
}

const char *xcb_atom_get_name_predefined(xcb_atom_t atom)
{
	if(atom <= 0 || atom > (sizeof(atom_name_offsets) / sizeof(*atom_name_offsets)))
		return 0;
	return atom_names + atom_name_offsets[atom - 1];
}

int xcb_atom_get_name(xcb_connection_t *c, xcb_atom_t atom, const char **namep, int *lengthp)
{
	static char buf[100];
	const char *name = xcb_atom_get_name_predefined(atom);
	int namelen;
	xcb_get_atom_name_cookie_t atomc;
	xcb_get_atom_name_reply_t *atomr;
	if(name)
	{
		*namep = name;
		*lengthp = strlen(name);
		return 1;
	}
	atomc = xcb_get_atom_name(c, atom);
	atomr = xcb_get_atom_name_reply(c, atomc, 0);
	if(!atomr)
		return 0;
	namelen = xcb_get_atom_name_name_length(atomr);
	if(namelen > sizeof(buf))
		namelen = sizeof(buf);
	*lengthp = namelen;
	memcpy(buf, xcb_get_atom_name_name(atomr), namelen);
	*namep = buf;
	free(atomr);
	return 1;
}

static char *makename(const char *fmt, ...)
{
	char *ret;
	int n;
	va_list ap;

#ifndef HAVE_VASPRINTF
	char *np;
	int size = 64;

	/* First allocate 'size' bytes, should be enough usually */
	if((ret = malloc(size)) == NULL)
		return NULL;

	while(1)
	{
		va_start(ap, fmt);
		n = vsnprintf(ret, size, fmt, ap);
		va_end(ap);

		if(n < 0)
			return NULL;

		if(n < size)
			return ret;

		size = n + 1;
		if((np = realloc(ret, size)) == NULL)
		{
			free(ret);
			return NULL;
		}

		ret = np;
	}
#else
	va_start(ap, fmt);
	n = vasprintf(&ret, fmt, ap);
	va_end(ap);

	if(n < 0)
		return NULL;

	return ret;
#endif
}

char *xcb_atom_name_by_screen(const char *base, uint8_t screen)
{
	return makename("%s_S%u", base, screen);
}

char *xcb_atom_name_by_resource(const char *base, uint32_t resource)
{
	return makename("%s_R%08X", base, resource);
}

char *xcb_atom_name_unique(const char *base, uint32_t id)
{
	if(base)
		return makename("%s_U%lu", base, id);
	else
		return makename("U%lu", id);
}
