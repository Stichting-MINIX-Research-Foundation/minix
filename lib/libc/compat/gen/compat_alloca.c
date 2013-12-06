#include <sys/cdefs.h>

__warn_references(alloca,
    "Warning: reference to the libc supplied alloca(3); this most likely will "
    "not work. Please use the compiler provided version of alloca(3), by "
    "supplying the appropriate compiler flags (e.g. not -std=c89).")
