FreeGrep
========

The grep utility searches any given input files, selecting lines
that match one or more patterns. By default, a pattern matches an
input line if the regular expression in the pattern matches the
input line without its trailing newline. An empty expression matches
every line. Each input line that matches at least one of the patterns
is written to the standard output. grep is used for simple patterns
and basic regular expressions; egrep can handle extended regular
expressions. fgrep is quicker than both grep and egrep, but can
only handle fixed patterns (i.e. it does not interpret regular
expressions). Patterns may consist of one or more lines, allowing
any of the pattern lines to match a portion of the input. zgrep,
zegrep, and zfgrep act like grep, egrep, and fgrep, respectively,
but accept input files compressed with the compress or gzip compression
utilities.
