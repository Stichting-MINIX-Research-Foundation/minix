/* $NetBSD: satafisreg.h,v 1.4 2012/01/24 20:04:07 jakllsch Exp $ */

/*
 * Copyright (c) 2009, 2010 Jonathan A. Kollasch.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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
 */

#ifndef _DEV_ATA_FISREG_H_
#define _DEV_ATA_FISREG_H_

#define fis_type 0

#define RHD_FISTYPE 0x27
#define RHD_FISLEN 20
#define rhd_c 1 /* Command bit and PM port */
#define RHD_C 0x80
#define rhd_command 2
#define rhd_features0 3
#define rhd_lba0 4
#define rhd_lba1 5
#define rhd_lba2 6
#define rhd_dh 7
#define rhd_lba3 8
#define rhd_lba4 9
#define rhd_lba5 10
#define rhd_features1 11
#define rhd_count0 12
#define rhd_count1 13
#define rhd_control 15

#define RDH_FISTYPE 0x34
#define RDH_FISLEN 20
#define rdh_i 1
#define RDH_I 0x40
#define rdh_status 2
#define rdh_error 3
#define rdh_lba0 4
#define rdh_lba1 5
#define rdh_lba2 6
#define rdh_dh 7
#define rdh_lba3 8
#define rdh_lba4 9
#define rdh_lba5 10
#define rdh_count0 12
#define rdh_count1 13

#define SDB_FISTYPE 0xA1
#define SDB_FISLEN 8
#define sdb_i 1
#define sdb_status 2
#define sdb_error 3

#endif /* _DEV_ATA_FISREG_H_ */
