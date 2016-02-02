
d="lib/roken lib/krb5 lib/gssapi lib/ntlm tests/kdc tests/gss kuser"

basedir=$(basedir $0)

${basedir}/../configure CFLAGS='-fprofile-arcs -ftest-coverage' > log

lcov --directory . --zerocounters

make all check > log

lcov --directory . --capture --output-file heimdal-lcov.info

objdir="/Volumes/data/Users/lha/obj/hg"
srcdir="/Volumes/data/Users/lha/src/heimdal/git"

perl -pi -e "s@SF:$objdir/(.*.[ly])\$@SF:$srcdir/\$1@" heimdal-lcov.info

genhtml heimdal-lcov.info
