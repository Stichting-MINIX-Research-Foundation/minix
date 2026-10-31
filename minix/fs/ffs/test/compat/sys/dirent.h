#ifndef SHIM_SYS_DIRENT_H
#define SHIM_SYS_DIRENT_H
#include <minixshim.h>
/* Minimal struct dirent; the server only uses sizeof(struct dirent). */
struct dirent {
	ino_t    d_fileno;
	uint16_t d_reclen;
	uint8_t  d_type;
	uint8_t  d_namlen;
	char     d_name[256];
};
#endif
