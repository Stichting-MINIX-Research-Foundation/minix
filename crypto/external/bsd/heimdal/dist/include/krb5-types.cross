/*
 * generic krb5-types.h for cross compiling, assume system is posix/sus
 */

#ifndef __krb5_types_h__
#define __krb5_types_h__

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef socklen_t krb5_socklen_t;
#include <unistd.h>
typedef ssize_t krb5_ssize_t;

#ifndef HEIMDAL_DEPRECATED
#if defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1 )))
#define HEIMDAL_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER) && (_MSC_VER>1200) 
#define HEIMDAL_DEPRECATED __declspec(deprecated)
#else
#define HEIMDAL_DEPRECATED
#endif
#endif

#ifndef HEIMDAL_PRINTF_ATTRIBUTE
#if defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1 )))
#define HEIMDAL_PRINTF_ATTRIBUTE(x) __attribute__((format x))
#else
#define HEIMDAL_PRINTF_ATTRIBUTE(x)
#endif
#endif

#ifndef HEIMDAL_NORETURN_ATTRIBUTE
#if defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1 )))
#define HEIMDAL_NORETURN_ATTRIBUTE __attribute__((noreturn))
#else
#define HEIMDAL_NORETURN_ATTRIBUTE
#endif
#endif

typedef int krb5_socket_t;

#endif /* __krb5_types_h__ */
