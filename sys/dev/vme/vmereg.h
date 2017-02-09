/* $NetBSD: vmereg.h,v 1.3 2005/12/11 12:24:07 christos Exp $ */

/* constants for address modifiers */

#define VME_AM_ADRSIZEMASK 0x30
#define VME_AM_ADRSIZESHIFT 4
#define VME_AM_A32 0
#define VME_AM_A16 0x20
#define VME_AM_A24 0x30
#define VME_AM_USERDEF 0x10 /* user/vendor definable */

#define VME_AM_MBO 8 /* must be set for standard AMs */

#define VME_AM_PRIVMASK 4
#define VME_AM_SUPER 4
#define VME_AM_USER 0

#define VME_AM_MODEMASK 3
#define VME_AM_DATA 1
#define VME_AM_PRG 2 /* only with A32, A24 */
#define VME_AM_BLT32 3 /* only with A32, A24 */
#define VME_AM_BLT64 0 /* new, only with A32, A24 */

#if 0
/* some AMs not yet supported by the framework */

/* ??? */
VME_AM_A24_xxx 0x32
VME_AM_A32_xxx 0x05

/* VME64 extension */
VME_AM_A40 0x34,0x35,0x37
VME_AM_A64 0x00,0x01,0x03,0x04
VME_AM_CR_CSR 0x2f /* GEO */

/* 2eVME extension */
VME_AM_2E_6U 0x20
VME_AM_2E_3U 0x21
#endif
