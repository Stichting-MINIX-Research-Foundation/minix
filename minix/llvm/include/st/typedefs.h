#ifndef ST_TYPEDEFS_H
#define ST_TYPEDEFS_H

#include <stdint.h>

/* Typedefs for predefined state transfer names. */
#define ST_DECLARE_STD_PTR_TYPEDEFS(PREFIX)         \
    typedef void*           PREFIX ## void_ptr_t;   \
    typedef char*           PREFIX ## char_ptr_t;   \
    typedef short*          PREFIX ## short_ptr_t;  \
    typedef int*            PREFIX ## int_ptr_t;    \
    typedef long*           PREFIX ## long_ptr_t;   \
    typedef unsigned char*  PREFIX ## uchar_ptr_t;  \
    typedef unsigned short* PREFIX ## ushort_ptr_t; \
    typedef unsigned int*   PREFIX ## uint_ptr_t;   \
    typedef unsigned long*  PREFIX ## ulong_ptr_t;  \
    typedef float*          PREFIX ## float_ptr_t;  \
    typedef double*         PREFIX ## double_ptr_t; \
    typedef uint8_t*        PREFIX ## u8_ptr_t;     \
    typedef uint16_t*       PREFIX ## u16_ptr_t;    \
    typedef uint32_t*       PREFIX ## u32_ptr_t;    \
    typedef uint64_t*       PREFIX ## u64_ptr_t;    \
    typedef int8_t*         PREFIX ## i8_ptr_t;     \
    typedef int16_t*        PREFIX ## i16_ptr_t;    \
    typedef int32_t*        PREFIX ## i32_ptr_t

#define ST_DECLARE_STD_PTRINT_TYPEDEFS(PREFIX)      \
    typedef uint32_t        PREFIX ## u32_t;        \
    typedef int             PREFIX ## int_t;        \
    typedef long            PREFIX ## long_t;       \
    typedef unsigned int    PREFIX ## uint_t;       \
    typedef unsigned long   PREFIX ## ulong_t

#define ST_TYPENAME_NO_TRANSFER_NAMES           "noxfer_*", "pthread_mutex_t", "siginfo_t", "epoll_data_t", "YYSTYPE"
    ST_DECLARE_STD_PTR_TYPEDEFS(noxfer_);
#define ST_TYPENAME_IDENTITY_TRANSFER_NAMES     "ixfer_*"
    ST_DECLARE_STD_PTR_TYPEDEFS(ixfer_);
#define ST_TYPENAME_CIDENTITY_TRANSFER_NAMES    "cixfer_*"
    ST_DECLARE_STD_PTR_TYPEDEFS(cixfer_);
#define ST_TYPENAME_PTR_TRANSFER_NAMES          "pxfer_*"
    ST_DECLARE_STD_PTRINT_TYPEDEFS(pxfer_);
#define ST_TYPENAME_STRUCT_TRANSFER_NAMES       "sxfer_*"
#ifdef __MINIX
#define ST_SENTRYNAME_NO_TRANSFER_NAMES         "noxfer_*", "sef_*", "st_*", "etext"
#else
#define ST_SENTRYNAME_NO_TRANSFER_NAMES         "noxfer_*", "st_*", "etext", "allocatedDescs*", "ep.*" /* nginx specific */
#define ST_DSENTRYLIB_NO_TRANSFER_NAMES         "*/libst.so", "*/libcommon.so", "*/libtaskctl.so"
#endif
#define ST_SENTRYNAME_NO_TRANSFER_MEM_NAMES     "_brksize"
#define ST_SENTRYNAME_IDENTITY_TRANSFER_NAMES   "ixfer_*"
#define ST_SENTRYNAME_CIDENTITY_TRANSFER_NAMES  "cixfer_*"
#define ST_SENTRYNAME_PTR_TRANSFER_NAMES        "pxfer_*"


#endif /* ST_TYPEDEFS_H */
