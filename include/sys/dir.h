/*
 * this files resolves conflicts between the file of the host system and
 * the minix specific one. This file is included directly only on Minix
 * and it is an error to do so on any other system
 */

#if !defined(_MINIX) && !defined(__minix) && !defined(__ACK__)
#error "Including Minix specific file in program targeted for other system"
#else
#include <minix/dir.h>
#endif
