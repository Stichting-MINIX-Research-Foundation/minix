#/bin/sh

set -e

cd dist
rm -rf ChangeLog.xsl style.css index.css *.sgml

uuencode external.png < external.png > external.png.uu
rm external.png

for f in [a-z]*; do
	sed -e 's/[$]Id:/\$Vendor-Id:/' \
	    -e 's/[$]Mdocdate: \([^$]*\) \([0-9][0-9][0-9][0-9]\) [$]/\1, \2/' < $f > $f.new && mv $f.new $f
done
