#ifndef __krb5_types_h__
#define __krb5_types_h__

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>


typedef socklen_t krb5_socklen_t;
#include <unistd.h>
typedef ssize_t krb5_ssize_t;

typedef int krb5_socket_t;

#if !defined(__has_extension)
#define __has_extension(x) 0
#endif

#ifndef KRB5TYPES_REQUIRE_GNUC
#define KRB5TYPES_REQUIRE_GNUC(m,n,p) \
    (((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100) + __GNUC_PATCHLEVEL__) >= \
     (((m) * 10000) + ((n) * 100) + (p)))
#endif

#ifndef HEIMDAL_DEPRECATED
#if __has_extension(deprecated) || KRB5TYPES_REQUIRE_GNUC(3,1,0)
#define HEIMDAL_DEPRECATED __attribute__ ((__deprecated__))
#elif defined(_MSC_VER) && (_MSC_VER>1200)
#define HEIMDAL_DEPRECATED __declspec(deprecated)
#else
#define HEIMDAL_DEPRECATED
#endif
#endif

#ifndef HEIMDAL_PRINTF_ATTRIBUTE
#if __has_extension(format) || KRB5TYPES_REQUIRE_GNUC(3,1,0)
#define HEIMDAL_PRINTF_ATTRIBUTE(x) __attribute__ ((__format__ x))
#else
#define HEIMDAL_PRINTF_ATTRIBUTE(x)
#endif
#endif

#ifndef HEIMDAL_NORETURN_ATTRIBUTE
#if __has_extension(noreturn) || KRB5TYPES_REQUIRE_GNUC(3,1,0)
#define HEIMDAL_NORETURN_ATTRIBUTE __attribute__ ((__noreturn__))
#else
#define HEIMDAL_NORETURN_ATTRIBUTE
#endif
#endif

#ifndef HEIMDAL_UNUSED_ATTRIBUTE
#if __has_extension(unused) || KRB5TYPES_REQUIRE_GNUC(3,1,0)
#define HEIMDAL_UNUSED_ATTRIBUTE __attribute__ ((__unused__))
#else
#define HEIMDAL_UNUSED_ATTRIBUTE
#endif
#endif

#ifndef HEIMDAL_WARN_UNUSED_RESULT_ATTRIBUTE
#if __has_extension(warn_unused_result) || KRB5TYPES_REQUIRE_GNUC(3,3,0)
#define HEIMDAL_WARN_UNUSED_RESULT_ATTRIBUTE __attribute__ ((__warn_unused_result__))
#else
#define HEIMDAL_WARN_UNUSED_RESULT_ATTRIBUTE
#endif
#endif

#endif /* __krb5_types_h__ */
