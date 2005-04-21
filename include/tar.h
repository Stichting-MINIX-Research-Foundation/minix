/* The <tar.h> header is used with the tape archiver, tar. */

#ifndef _TAR_H
#define _TAR_H

#define TBLOCK 		512
#define NAMSIZ		100
#define PFXSIZ		155

#define TMODLEN 	8
#define TUIDLEN		8
#define TGIDLEN		8
#define TSIZLEN		12
#define TMTMLEN		12
#define TCKSLEN		8

#define TMAGIC		"ustar"
#define TMAGLEN		6
#define TVERSION	"00"
#define TVERSLEN	2
#define TUNMLEN		32
#define TGNMLEN		32
#define TDEVLEN		8

#define REGTYPE		'0'
#define AREGTYPE	'\0'
#define LNKTYPE		'1'
#define SYMTYPE		'2'
#define CHRTYPE		'3'
#define BLKTYPE		'4'
#define DIRTYPE		'5'
#define FIFOTYPE	'6'
#define CONTTYPE	'7'

#define TSUID		04000
#define TSGID		02000
#define TSVTX		01000

#define TUREAD		00400
#define TUWRITE		00200
#define TUEXEC		00100
#define TGREAD		00040
#define TGWRITE		00020
#define TGEXEC		00010
#define TOREAD		00004
#define TOWRITE		00002
#define TOEXEC		00001

union hblock {
  char dummy[TBLOCK];
  struct header {
	char name[NAMSIZ];
	char mode[TMODLEN];
	char uid[TUIDLEN];
	char gid[TGIDLEN];
	char size[TSIZLEN];
	char mtime[TMTMLEN];
	char chksum[TCKSLEN];
	char typeflag;
	char linkname[NAMSIZ];
	char magic[TMAGLEN];
	char version[TVERSLEN];
	char uname[TUNMLEN];
	char gname[TGNMLEN];
	char devmajor[TDEVLEN];
	char devminor[TDEVLEN];
	char prefix[PFXSIZ];
  } dbuf;
};

#endif /* _TAR_H */
