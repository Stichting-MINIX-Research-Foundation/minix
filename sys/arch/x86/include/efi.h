/*     $NetBSD: efi.h,v 1.8 2017/10/22 00:59:28 maya Exp $   */

/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* This file is mainly ia64/include/efi.h with little modifications. */

#ifndef _X86_EFI_H_
#define _X86_EFI_H_

#include <sys/uuid.h>

#define        EFI_PAGE_SHIFT          12
#define        EFI_PAGE_SIZE           (1 << EFI_PAGE_SHIFT)
#define        EFI_PAGE_MASK           (EFI_PAGE_SIZE - 1)

#define	EFI_TABLE_ACPI20						\
	{0x8868e871,0xe4f1,0x11d3,0xbc,0x22,{0x00,0x80,0xc7,0x3c,0x88,0x81}}
#define	EFI_TABLE_ACPI10						\
	{0xeb9d2d30,0x2d88,0x11d3,0x9a,0x16,{0x00,0x90,0x27,0x3f,0xc1,0x4d}}
#define	EFI_TABLE_SMBIOS						\
	{0xeb9d2d31,0x2d88,0x11d3,0x9a,0x16,{0x00,0x90,0x27,0x3f,0xc1,0x4d}}
#define	EFI_TABLE_SMBIOS3						\
	{0xf2fd1544,0x9794,0x4a2c,0x99,0x2e,{0xe5,0xbb,0xcf,0x20,0xe3,0x94}}

extern const struct uuid EFI_UUID_ACPI20;
extern const struct uuid EFI_UUID_ACPI10;
extern const struct uuid EFI_UUID_SMBIOS;
extern const struct uuid EFI_UUID_SMBIOS3;

extern bool bootmethod_efi;

enum efi_reset {
       EFI_RESET_COLD,
       EFI_RESET_WARM
};

typedef uint16_t       efi_char;
typedef unsigned long efi_status;

struct efi_cfgtbl {
       struct uuid     ct_uuid;
       void           *ct_data;
};

struct efi_md {
       uint32_t        md_type;
#define        EFI_MD_TYPE_NULL        0
#define        EFI_MD_TYPE_CODE        1       /* Loader text. */
#define        EFI_MD_TYPE_DATA        2       /* Loader data. */
#define        EFI_MD_TYPE_BS_CODE     3       /* Boot services text. */
#define        EFI_MD_TYPE_BS_DATA     4       /* Boot services data. */
#define        EFI_MD_TYPE_RT_CODE     5       /* Runtime services text. */
#define        EFI_MD_TYPE_RT_DATA     6       /* Runtime services data. */
#define        EFI_MD_TYPE_FREE        7       /* Unused/free memory. */
#define        EFI_MD_TYPE_BAD         8       /* Bad memory */
#define        EFI_MD_TYPE_RECLAIM     9       /* ACPI reclaimable memory. */
#define        EFI_MD_TYPE_FIRMWARE    10      /* ACPI NV memory */
#define        EFI_MD_TYPE_IOMEM       11      /* Memory-mapped I/O. */
#define        EFI_MD_TYPE_IOPORT      12      /* I/O port space. */
#define        EFI_MD_TYPE_PALCODE     13      /* PAL */
#define        EFI_MD_TYPE_PMEM        14      /* Persistent memory. */
       uint32_t        __pad;
       uint64_t        md_phys;
       uint64_t        md_virt;
       uint64_t        md_pages;
       uint64_t        md_attr;
#define        EFI_MD_ATTR_UC          0x0000000000000001UL
#define        EFI_MD_ATTR_WC          0x0000000000000002UL
#define        EFI_MD_ATTR_WT          0x0000000000000004UL
#define        EFI_MD_ATTR_WB          0x0000000000000008UL
#define        EFI_MD_ATTR_UCE         0x0000000000000010UL
#define        EFI_MD_ATTR_WP          0x0000000000001000UL
#define        EFI_MD_ATTR_RP          0x0000000000002000UL
#define        EFI_MD_ATTR_XP          0x0000000000004000UL
#define        EFI_MD_ATTR_RT          0x8000000000000000UL
};

struct efi_tm {
       uint16_t        tm_year;                /* 1998 - 20XX */
       uint8_t         tm_mon;                 /* 1 - 12 */
       uint8_t         tm_mday;                /* 1 - 31 */
       uint8_t         tm_hour;                /* 0 - 23 */
       uint8_t         tm_min;                 /* 0 - 59 */
       uint8_t         tm_sec;                 /* 0 - 59 */
       uint8_t         __pad1;
       uint32_t        tm_nsec;                /* 0 - 999,999,999 */
       int16_t         tm_tz;                  /* -1440 to 1440 or 2047 */
       uint8_t         tm_dst;
       uint8_t         __pad2;
};

struct efi_tmcap {
       uint32_t        tc_res;         /* 1e-6 parts per million */
       uint32_t        tc_prec;        /* hertz */
       uint8_t         tc_stz;         /* Set clears sub-second time */
};

struct efi_tblhdr {
       uint64_t        th_sig;
       uint32_t        th_rev;
       uint32_t        th_hdrsz;
       uint32_t        th_crc32;
       uint32_t        __res;
};

struct efi_rt {
       struct efi_tblhdr rt_hdr;
       efi_status      (*rt_gettime)(struct efi_tm *, struct efi_tmcap *);
       efi_status      (*rt_settime)(struct efi_tm *);
       efi_status      (*rt_getwaketime)(uint8_t *, uint8_t *,
           struct efi_tm *);
       efi_status      (*rt_setwaketime)(uint8_t, struct efi_tm *);
       efi_status      (*rt_setvirtual)(u_long, u_long, uint32_t,
           struct efi_md *);
       efi_status      (*rt_cvtptr)(u_long, void **);
       efi_status      (*rt_getvar)(efi_char *, struct uuid *, uint32_t *,
           u_long *, void *);
       efi_status      (*rt_scanvar)(u_long *, efi_char *, struct uuid *);
       efi_status      (*rt_setvar)(efi_char *, struct uuid *, uint32_t,
           u_long, void *);
       efi_status      (*rt_gethicnt)(uint32_t *);
       efi_status      (*rt_reset)(enum efi_reset, efi_status, u_long,
           efi_char *);
};

struct efi_systbl {
       struct efi_tblhdr st_hdr;
#define        EFI_SYSTBL_SIG  0x5453595320494249UL
       efi_char        *st_fwvendor;
       uint32_t        st_fwrev;
#ifdef __amd64__
       uint32_t        __pad;
#endif
       void            *st_cin;
       void            *st_cinif;
       void            *st_cout;
       void            *st_coutif;
       void            *st_cerr;
       void            *st_cerrif;
       struct efi_rt   *st_rt;
       void            *st_bs;
       u_long          st_entries;
       struct efi_cfgtbl *st_cfgtbl;
};

#if defined(__amd64__)
struct efi_cfgtbl32 {
	struct uuid		ct_uuid;
	uint32_t		ct_data;	/* void * */
};

struct efi_systbl32 {
	struct efi_tblhdr	st_hdr;

	uint32_t		st_fwvendor;
	uint32_t		st_fwrev;
	uint32_t		st_cin;		/* = 0 */
	uint32_t		st_cinif;	/* = 0 */
	uint32_t		st_cout;	/* = 0 */
	uint32_t		st_coutif;	/* = 0 */
	uint32_t		st_cerr;	/* = 0 */
	uint32_t		st_cerrif;	/* = 0 */
	uint32_t		st_rt;		/* struct efi_rt32 * */
	uint32_t		st_bs;		/* = 0 */
	uint32_t		st_entries;
	uint32_t		st_cfgtbl;	/* struct efi_cfgtbl32 * */
};
#elif defined(__i386__)
struct efi_cfgtbl64 {
	struct uuid		ct_uuid;
	uint64_t		ct_data;	/* void * */
};

struct efi_systbl64 {
	struct efi_tblhdr	st_hdr;

	uint64_t		st_fwvendor;
	uint32_t		st_fwrev;
	uint32_t		__pad;
	uint64_t		st_cin;		/* = 0 */
	uint64_t		st_cinif;	/* = 0 */
	uint64_t		st_cout;	/* = 0 */
	uint64_t		st_coutif;	/* = 0 */
	uint64_t		st_cerr;	/* = 0 */
	uint64_t		st_cerrif;	/* = 0 */
	uint64_t		st_rt;		/* struct efi_rt64 * */
	uint64_t		st_bs;		/* = 0 */
	uint64_t		st_entries;
	uint64_t		st_cfgtbl;	/* struct efi_cfgtbl64 * */
};
#endif

void               efi_init(void);
bool               efi_probe(void);
paddr_t            efi_getsystblpa(void);
struct efi_systbl *efi_getsystbl(void);
paddr_t            efi_getcfgtblpa(const struct uuid*);
void              *efi_getcfgtbl(const struct uuid*);
int                efi_getbiosmemtype(uint32_t, uint64_t);
const char        *efi_getmemtype_str(uint32_t);
struct btinfo_memmap;
struct btinfo_memmap *efi_get_e820memmap(void);

/*
void efi_boot_finish(void);
int efi_boot_minimal(uint64_t);
void *efi_get_table(struct uuid *);
void efi_get_time(struct efi_tm *);
struct efi_md *efi_md_first(void);
struct efi_md *efi_md_next(struct efi_md *);
void efi_reset_system(void);
efi_status efi_set_time(struct efi_tm *);
*/

#endif /* _X86_EFI_H_ */
