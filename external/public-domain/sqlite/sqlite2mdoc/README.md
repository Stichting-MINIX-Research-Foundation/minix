## Synopsis

This utility accepts an [SQLite](https://www.sqlite.org) header file
`sqlite.h` and produces a set of decently well-formed
[mdoc(7)](http://man.openbsd.org/OpenBSD-current/man7/mdoc.7) files
documenting the C API.
These will be roughly equivalent to the [C-language Interface
Specification for SQLite](https://www.sqlite.org/c3ref/intro.html).

You can also use it for any file(s) using the documentation standards of
SQLite.
See the [sqlite2mdoc.1](sqlite2mdoc.1) manpage for syntax details.

This [GitHub](https://www.github.com) repository is a read-only mirror
of the project's CVS repository.

## Installation

Simply run `make`: this utility isn't meant for installation, but for
integration into your SQLite deployment phase.
You can run `make install`, however, if you plan on using it for other
documentation.
There are no compile-time or run-time dependencies unless you're on
Linux, in which case you'll need
[libbsd](https://libbsd.freedesktop.org).
You'll also need to uncomment the `LDADD` line in the
[Makefile](Makefile), in this case.


This software has been used on OpenBSD, Mac OS X, and Linux machines.

## License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
