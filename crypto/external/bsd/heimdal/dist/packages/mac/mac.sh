#!/bin/sh
# Id

dbase=`dirname $0`
base=`cd $dbase && pwd`
config=${base}/../../configure

destdir=`pwd`/destdir
builddir=`pwd`/builddir
imgdir=`pwd`/imgdir

rm -rf ${destdir} ${builddir} ${imgdir} || exit 1
mkdir ${destdir} || exit 1
mkdir ${builddir} || exit 1
mkdir ${imgdir} || exit 1

cd ${builddir} || exit 1

version=`sh ${config} --help 2>/dev/null | head -1 | sed 's/.*Heimdal \([^ ]*\).*/\1/'`

echo "Building Mac universal binary package for Heimdal ${version}"
echo "Configure"
env \
  CFLAGS="-arch i386 -arch x86_64" \
  LDFLAGS="-arch i386 -arch x86_64" \
  ${config} --disable-dependency-tracking > log || exit 1
echo "Build"
make all > /dev/null || exit 1
echo "Run regression suite"
make check > /dev/null || exit 1
echo "Install"
make install DESTDIR=${destdir} > /dev/null || exit 1 

echo "Build package"
xcrun productbuild \
    --identifier org.h5l.heimdal \
    --version ${version} \
    --root ${destdir} / \
    --resources ${base}/Resources \
    --sign 'Developer ID Installer:' \
    ${imgdir}/Heimdal-${version}.pkg

cd ..
echo "Build disk image"
rm "heimdal-${version}.dmg"
/usr/bin/hdiutil create -volname "Heimdal-${version}" -srcfolder ${imgdir} "heimdal-${version}.dmg" || exit 1

echo "Clean"
rm -rf ${destdir} ${builddir} ${imgdir} || exit 1

echo "Done!"
exit 0
