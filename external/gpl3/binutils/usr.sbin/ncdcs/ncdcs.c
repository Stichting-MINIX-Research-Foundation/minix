/* $NetBSD: ncdcs.c,v 1.1 2009/08/18 20:22:20 skrll Exp $ */
/*
** Program to prepare an ELF file for download to an IBM Network
** Station 300 or 1000 running the NCD firmware.
**
** This may also work for other loaders based on NCD code.
**
** The program expects a signature and some marker fields a couple
** bytes into the image itself, where the checksum gets patched in.
** The checksum itself is a 16bit CRC over the ELF header, the ELF
** program header, and the image portion of the ELF file. The start
** value for the CRC is 0xffff, and no final xor is performed.
**
** Usage: ncdcs <infile> [outfile]
** e.g.   ncdcs zImage zImage.ncd
**
** Copyright 2002 Jochen Roth
** NetBSD changes copyright 2003 John Gordon
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License,
** Version 2, as published by the Free Software Foundation.
**
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __NetBSD__

unsigned short crc16(unsigned short crc, unsigned char *buf, unsigned len);
unsigned short get_be_16(unsigned char *p);
unsigned long get_be_32(unsigned char *p);
void put_be_16(unsigned char *p, unsigned short x);
void put_be_32(unsigned char *p, unsigned long x);

#endif

static const unsigned short crc16_table[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};


unsigned short crc16(unsigned short crc, unsigned char *buf, unsigned len)
{
	while(len--) {
		crc = crc16_table[(crc ^ *buf++) & 0xff] ^ (crc >> 8);
	}
	return(crc);
}


unsigned short get_be_16(unsigned char *p)
{
	unsigned short x = 0;
	x = (unsigned short)p[0];
	x = (x<<8) + (unsigned short)p[1];
	return(x);
}


unsigned long get_be_32(unsigned char *p)
{
	unsigned long x = 0;
	x = (unsigned long)p[0];
	x = (x<<8) + (unsigned long)p[1];
	x = (x<<8) + (unsigned long)p[2];
	x = (x<<8) + (unsigned long)p[3];
	return(x);
}


void put_be_16(unsigned char *p, unsigned short x)
{
	p[0] = (x >> 8);
	p[1] = x;
}


void put_be_32(unsigned char *p, unsigned long x)
{
	p[0] = (x >> 24);
	p[1] = (x >> 16);
	p[2] = (x >> 8);
	p[3] = x;
}


int main(int argc, char *argv[])
{
	char		*infile, *outfile;
	struct stat	st;
	unsigned char	*buf, *image, *p;
	int		fd, filesize, old_filesize;
	unsigned long	hdr_len, img_len, img_offset;
	unsigned short	crc, old_crc;

	/* check arguments */

	if(argc < 2 || argc > 3 || argv[1][0] == '-') {
		printf("usage: %s <elf-image> [ncd-image]\n", argv[0]);
		return(1);
	}

	infile = argv[1];
	outfile = argc > 2 ? argv[2] : NULL;

	/* determine file size, allocate buffer, open and read elf file */

	if ((fd = open(infile, O_RDONLY)) == -1) {
		perror(infile);
		return(-1);
	}

	if (fstat(fd, &st) == -1) {
		perror(infile);
		close(fd);
		return(-1);
	}
	buf = malloc(st.st_size + 4);
	if (buf == NULL) {
		perror(infile);
		close(fd);
		return(-1);
	}
	filesize = read(fd, buf, st.st_size);
	if (filesize != st.st_size) {
		perror(infile);
		close(fd);
		return(-1);
	}
	close(fd);

	/* verify elf header */

	if(memcmp(buf, "\177ELF", 4)) {
		fprintf(stderr, "%s: not an ELF file\n", infile);
		return(-1);
	}
	if(buf[0x04] != 1 || buf[0x05] != 2 || get_be_16(buf+0x12) != 20) {
		fprintf(stderr, "%s: not a 32bit big-endian PPC ELF file\n", infile);
		return(-1);
	}
	if(get_be_32(buf+0x1c) != 0x34) {
		fprintf(stderr, "%s: ELF PPC program header not at expected offset\n", infile);
		return(-1);
	}

	if((img_offset = get_be_32(buf+0x38)) != 0x10000) {
	        /*
	         * Doesn't seem to bother the device when the header is less
		 * than 64K, and the default builds for NetBSD always generate
		 * images that are not padded to a 64K boundary...
	         */
#ifndef __NetBSD__
		fprintf(stderr, "warning: %s: size of ELF header is not 64k\n", infile);
#endif
	}

	/* verify checksum area markers */

	image = buf + img_offset;
	if(memcmp(image + 0x10, "XncdPPC", 8)) {
		fprintf(stderr, "%s: missing XncdPPC marker\n", infile);
		return(-1);
	}

	/* zero out the checksum and size fields, then compute checksum */

	p = image + 0x18;
	old_crc = get_be_16(p);
	old_filesize = get_be_32(p+4);

	memset(p, 0, 8);

	hdr_len = get_be_16(buf+0x28) + get_be_16(buf+0x2a);
	img_len = get_be_32(buf+0x44);

	crc = 0xffff;
	crc = crc16(crc, buf, hdr_len);
	crc = crc16(crc, buf + img_offset, img_len);

	printf("%s: hdr_len=0x%x img_offset=0x%x img_len=0x%x crc=0x%04x fsz=0x%06x\n", infile, (int) hdr_len, (int) img_offset, (int) img_len, crc, filesize);

	if(outfile) {
		/* patch in the crc and the total file size (???) */

		put_be_16(p, crc);
		put_be_32(p+4, filesize);

		if((fd = open(outfile, O_RDWR|O_CREAT|O_TRUNC, st.st_mode)) < 0) {
			perror(outfile);
			return(-1);
		}

		if(write(fd, buf, filesize) != filesize) {
			perror(outfile);
			return(-1);
		}
		close(fd);
	} else {
		/* Check the crc and filesize fields in the file */

		if((crc == old_crc) && (filesize == old_filesize)) {
			printf("%s: NCD crc and filesize seem ok.\n", infile);
		} else {
			printf("%s: NCD current crc=0x%04x fsz=0x%06x\n", infile, old_crc, old_filesize);
		}
	}

	return(0);
}
