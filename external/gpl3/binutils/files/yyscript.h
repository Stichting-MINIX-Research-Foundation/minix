/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_YYSCRIPT_H_INCLUDED
# define YY_YY_YYSCRIPT_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    PLUSEQ = 258,
    MINUSEQ = 259,
    MULTEQ = 260,
    DIVEQ = 261,
    LSHIFTEQ = 262,
    RSHIFTEQ = 263,
    ANDEQ = 264,
    OREQ = 265,
    OROR = 266,
    ANDAND = 267,
    EQ = 268,
    NE = 269,
    LE = 270,
    GE = 271,
    LSHIFT = 272,
    RSHIFT = 273,
    UNARY = 274,
    STRING = 275,
    QUOTED_STRING = 276,
    INTEGER = 277,
    ABSOLUTE = 278,
    ADDR = 279,
    ALIGN_K = 280,
    ALIGNOF = 281,
    ASSERT_K = 282,
    AS_NEEDED = 283,
    AT = 284,
    BIND = 285,
    BLOCK = 286,
    BYTE = 287,
    CONSTANT = 288,
    CONSTRUCTORS = 289,
    COPY = 290,
    CREATE_OBJECT_SYMBOLS = 291,
    DATA_SEGMENT_ALIGN = 292,
    DATA_SEGMENT_END = 293,
    DATA_SEGMENT_RELRO_END = 294,
    DEFINED = 295,
    DSECT = 296,
    ENTRY = 297,
    EXCLUDE_FILE = 298,
    EXTERN = 299,
    FILL = 300,
    FLOAT = 301,
    FORCE_COMMON_ALLOCATION = 302,
    GLOBAL = 303,
    GROUP = 304,
    HLL = 305,
    INCLUDE = 306,
    INHIBIT_COMMON_ALLOCATION = 307,
    INFO = 308,
    INPUT = 309,
    KEEP = 310,
    LEN = 311,
    LENGTH = 312,
    LOADADDR = 313,
    LOCAL = 314,
    LONG = 315,
    MAP = 316,
    MAX_K = 317,
    MEMORY = 318,
    MIN_K = 319,
    NEXT = 320,
    NOCROSSREFS = 321,
    NOFLOAT = 322,
    NOLOAD = 323,
    ONLY_IF_RO = 324,
    ONLY_IF_RW = 325,
    ORG = 326,
    ORIGIN = 327,
    OUTPUT = 328,
    OUTPUT_ARCH = 329,
    OUTPUT_FORMAT = 330,
    OVERLAY = 331,
    PHDRS = 332,
    PROVIDE = 333,
    PROVIDE_HIDDEN = 334,
    QUAD = 335,
    SEARCH_DIR = 336,
    SECTIONS = 337,
    SEGMENT_START = 338,
    SHORT = 339,
    SIZEOF = 340,
    SIZEOF_HEADERS = 341,
    SORT_BY_ALIGNMENT = 342,
    SORT_BY_NAME = 343,
    SPECIAL = 344,
    SQUAD = 345,
    STARTUP = 346,
    SUBALIGN = 347,
    SYSLIB = 348,
    TARGET_K = 349,
    TRUNCATE = 350,
    VERSIONK = 351,
    OPTION = 352,
    PARSING_LINKER_SCRIPT = 353,
    PARSING_VERSION_SCRIPT = 354,
    PARSING_DEFSYM = 355,
    PARSING_DYNAMIC_LIST = 356
  };
#endif
/* Tokens.  */
#define PLUSEQ 258
#define MINUSEQ 259
#define MULTEQ 260
#define DIVEQ 261
#define LSHIFTEQ 262
#define RSHIFTEQ 263
#define ANDEQ 264
#define OREQ 265
#define OROR 266
#define ANDAND 267
#define EQ 268
#define NE 269
#define LE 270
#define GE 271
#define LSHIFT 272
#define RSHIFT 273
#define UNARY 274
#define STRING 275
#define QUOTED_STRING 276
#define INTEGER 277
#define ABSOLUTE 278
#define ADDR 279
#define ALIGN_K 280
#define ALIGNOF 281
#define ASSERT_K 282
#define AS_NEEDED 283
#define AT 284
#define BIND 285
#define BLOCK 286
#define BYTE 287
#define CONSTANT 288
#define CONSTRUCTORS 289
#define COPY 290
#define CREATE_OBJECT_SYMBOLS 291
#define DATA_SEGMENT_ALIGN 292
#define DATA_SEGMENT_END 293
#define DATA_SEGMENT_RELRO_END 294
#define DEFINED 295
#define DSECT 296
#define ENTRY 297
#define EXCLUDE_FILE 298
#define EXTERN 299
#define FILL 300
#define FLOAT 301
#define FORCE_COMMON_ALLOCATION 302
#define GLOBAL 303
#define GROUP 304
#define HLL 305
#define INCLUDE 306
#define INHIBIT_COMMON_ALLOCATION 307
#define INFO 308
#define INPUT 309
#define KEEP 310
#define LEN 311
#define LENGTH 312
#define LOADADDR 313
#define LOCAL 314
#define LONG 315
#define MAP 316
#define MAX_K 317
#define MEMORY 318
#define MIN_K 319
#define NEXT 320
#define NOCROSSREFS 321
#define NOFLOAT 322
#define NOLOAD 323
#define ONLY_IF_RO 324
#define ONLY_IF_RW 325
#define ORG 326
#define ORIGIN 327
#define OUTPUT 328
#define OUTPUT_ARCH 329
#define OUTPUT_FORMAT 330
#define OVERLAY 331
#define PHDRS 332
#define PROVIDE 333
#define PROVIDE_HIDDEN 334
#define QUAD 335
#define SEARCH_DIR 336
#define SECTIONS 337
#define SEGMENT_START 338
#define SHORT 339
#define SIZEOF 340
#define SIZEOF_HEADERS 341
#define SORT_BY_ALIGNMENT 342
#define SORT_BY_NAME 343
#define SPECIAL 344
#define SQUAD 345
#define STARTUP 346
#define SUBALIGN 347
#define SYSLIB 348
#define TARGET_K 349
#define TRUNCATE 350
#define VERSIONK 351
#define OPTION 352
#define PARSING_LINKER_SCRIPT 353
#define PARSING_VERSION_SCRIPT 354
#define PARSING_DEFSYM 355
#define PARSING_DYNAMIC_LIST 356

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 53 "yyscript.y" /* yacc.c:1909  */

  /* A string.  */
  struct Parser_string string;
  /* A number.  */
  uint64_t integer;
  /* An expression.  */
  Expression_ptr expr;
  /* An output section header.  */
  struct Parser_output_section_header output_section_header;
  /* An output section trailer.  */
  struct Parser_output_section_trailer output_section_trailer;
  /* A section constraint.  */
  enum Section_constraint constraint;
  /* A complete input section specification.  */
  struct Input_section_spec input_section_spec;
  /* A list of wildcard specifications, with exclusions.  */
  struct Wildcard_sections wildcard_sections;
  /* A single wildcard specification.  */
  struct Wildcard_section wildcard_section;
  /* A list of strings.  */
  String_list_ptr string_list;
  /* Information for a program header.  */
  struct Phdr_info phdr_info;
  /* Used for version scripts and within VERSION {}.  */
  struct Version_dependency_list* deplist;
  struct Version_expression_list* versyms;
  struct Version_tree* versnode;
  enum Script_section_type section_type;

#line 286 "yyscript.h" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (void* closure);

#endif /* !YY_YY_YYSCRIPT_H_INCLUDED  */
