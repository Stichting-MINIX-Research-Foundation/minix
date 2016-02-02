#!/bin/sh

set -e

rm -rf CVS ChangeLog.xsl style.css index.css *.sgml regress

uuencode external.png < external.png > external.png.uu
rm external.png

cleantags .
for f in [a-z]*; do
	sed -e 's/[$]Mdocdate: \([^$]*\) \([0-9][0-9][0-9][0-9]\) [$]/\1, \2/' < $f > $f.new && mv $f.new $f
done
