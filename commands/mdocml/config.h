
#ifndef MANDOC_CONFIG_H
#define MANDOC_CONFIG_H

#define HAVE_STRLCAT
#define HAVE_STRLCPY

#include <sys/types.h>
#include <sys/cdefs.h>

#ifndef __GNUC__
#define inline
#endif

#ifndef _DIAGASSERT
#define _DIAGASSERT assert
#endif

#endif /* MANDOC_CONFIG_H */

