.include <bsd.own.mk>

SUBDIR= csu
SUBDIR+= .WAIT
SUBDIR+= libc
SUBDIR+= .WAIT
SUBDIR+= libcompat_minix libc libblockdriver libchardriver     \
	libnetdriver libedit libm libsys libtimers libutil \
	libl libz libfetch libvtreefs libaudiodriver libmthread   \
	libexec libdevman libusb libminlib libasyn          \
	libddekit libminixfs libbdev libelf libminc libcrypt libterminfo \
	libvassert libutil libbz2 libarchive libprop \
	libnetsock libpuffs libsffs libhgfs libvboxfs 

SUBDIR+=        ../external/public-domain/xz/lib

# libraries that follow depend on earlier ones

SUBDIR+= .WAIT
SUBDIR+= librefuse
SUBDIR+= libcurses

.include <bsd.subdir.mk>
