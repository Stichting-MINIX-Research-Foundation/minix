.include <bsd.own.mk>

SUBDIR= csu
SUBDIR+= .WAIT
SUBDIR+= libc
SUBDIR+= .WAIT
SUBDIR+= libcompat_minix libc libblockdriver libchardriver     \
	libnetdriver libedit libm libsys libtimers libutil \
	libz libfetch libvtreefs libaudiodriver libmthread   \
	libexec libdevman libusb libminlib libasyn          \
	libddekit libminixfs libbdev libelf libminc libcrypt libterminfo \
	libvassert libutil libbz2 libprop librmt \
	libnetsock libpuffs libsffs libhgfs libvboxfs 

SUBDIR+=        ../external/public-domain/xz/lib

# libraries that follow depend on earlier ones

SUBDIR+= .WAIT
SUBDIR+=        ../external/bsd/libarchive/lib  # depends on libxz
SUBDIR+=	../external/bsd/flex/lib
SUBDIR+= librefuse
SUBDIR+= libcurses

.include <bsd.subdir.mk>
