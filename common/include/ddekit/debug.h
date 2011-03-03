#ifndef DDEKIT_DEBUG_H
#define DDEKIT_DEBUG_H
#include <ddekit/ddekit.h>
#include <ddekit/printf.h>

#define DDEBUG_QUIET 0
#define DDEBUG_ERR 1
#define DDEBUG_WARN 2
#define DDEBUG_INFO 3
#define DDEBUG_VERBOSE 4

#define DDEBUG_MEM DDEBUG_INFO

#define DDEBUG_MSG_ERR(fmt, ...)
#define DDEBUG_MSG_WARN(fmt, ...)
#define DDEBUG_MSG_INFO(fmt, ...)
#define DDEBUG_MSG_VERBOSE(fmt, ...)

#if DDEBUG >= DDEBUG_ERR
#undef DDEBUG_MSG_ERR
#define  DDEBUG_MSG_ERR(fmt, ...) ddekit_printf("%s : "fmt"\n", __func__, ##__VA_ARGS__ ) 
#endif

#if DDEBUG >= DDEBUG_WARN
#undef DDEBUG_MSG_WARN
#define  DDEBUG_MSG_WARN(fmt, ...) ddekit_printf("%s: "fmt"\n", __func__, ##__VA_ARGS__ )
#endif

#if DDEBUG >= DDEBUG_INFO
#undef DDEBUG_MSG_INFO
#define  DDEBUG_MSG_INFO(fmt, ...) ddekit_printf("%s: "fmt"\n", __func__, ##__VA_ARGS__ )
#endif

#if DDEBUG >= DDEBUG_VERBOSE
#undef DDEBUG_MSG_VERBOSE
#define  DDEBUG_MSG_VERBOSE(fmt, ...) ddekit_printf("%s: "fmt"\n", __func__, ##__VA_ARGS__ ) 
#endif

#endif


