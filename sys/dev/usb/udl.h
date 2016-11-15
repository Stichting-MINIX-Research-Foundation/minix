/*	$NetBSD: udl.h,v 1.1 2009/11/30 16:18:34 tsutsui Exp $	*/

/*-
 * Copyright (c) 2009 FUKAUMI Naoki.
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

/*
 * Copyright (c) 2009 Marcus Glocker <mglocker@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef UDL_EVENT_COUNTERS
#define UDL_EVCNT_INCR(ev)	(ev)->ev_count++
#else
#define UDL_EVCNT_INCR(ev)	do {} while (/* CONSTCOND */ 0)
#endif

/*
 * Bulk command xfer structure.
 */
#define UDL_CMD_BUFFER_SIZE	(64 * 1024)
#define UDL_CMD_HEADER_SIZE	6
#define UDL_CMD_WIDTH_MAX	256
#define UDL_CMD_DRAW_SIZE(width) \
				(UDL_CMD_HEADER_SIZE + (width) * 2)
#define UDL_CMD_FILL_SIZE	(UDL_CMD_HEADER_SIZE + 3)
#define UDL_CMD_COPY_SIZE	(UDL_CMD_HEADER_SIZE + 3)
#define UDL_CMD_COMP_WORD_SIZE	4
#define UDL_CMD_COMP_MIN_SIZE	(UDL_CMD_HEADER_SIZE + UDL_CMD_COMP_WORD_SIZE)
#define UDL_CMD_COMP_BLOCK_SIZE	512
#define UDL_CMD_COMP_THRESHOLD \
    (UDL_CMD_BUFFER_SIZE - (UDL_CMD_COMP_BLOCK_SIZE * 2))

#define UDL_NCMDQ	32

struct udl_cmdq {
	TAILQ_ENTRY(udl_cmdq)	 cq_chain;
	struct udl_softc	*cq_sc;
	usbd_xfer_handle	 cq_xfer;
	uint8_t			*cq_buf;
};

/*
 * Our per device structure.
 */
struct udl_softc {
	device_t		 sc_dev;
	usbd_device_handle	 sc_udev;
	usbd_interface_handle	 sc_iface;
	usbd_pipe_handle	 sc_tx_pipeh;

	struct udl_cmdq		 sc_cmdq[UDL_NCMDQ];
	TAILQ_HEAD(udl_cmdq_head, udl_cmdq)	sc_freecmd,
						sc_xfercmd;

	struct udl_cmdq		*sc_cmd_cur;
	uint8_t			*sc_cmd_buf;
#define UDL_CMD_BUFINIT(sc)	((sc)->sc_cmd_buf = (sc)->sc_cmd_cur->cq_buf)
#define UDL_CMD_BUFSIZE(sc)	((sc)->sc_cmd_buf - (sc)->sc_cmd_cur->cq_buf)
	int			 sc_cmd_cblen;

	struct edid_info	 sc_ei;
	int			 sc_width;
	int			 sc_height;
	int			 sc_offscreen;
	uint8_t			 sc_depth;

	/* wsdisplay glue */
	struct wsscreen_descr	 sc_defaultscreen;
	const struct wsscreen_descr	*sc_screens[1];
	struct wsscreen_list	 sc_screenlist;
	struct rasops_info	 sc_ri;
	device_t		 sc_wsdisplay;
	u_int			 sc_mode;
	u_int			 sc_blank;
	uint8_t			 sc_nscreens;

	uint8_t			*sc_fbmem;	/* framebuffer for X11 */
#define UDL_FBMEM_SIZE(sc) \
    ((sc)->sc_width * (sc)->sc_height * ((sc)->sc_depth / 8))

	uint8_t			*sc_huffman;
	uint8_t			*sc_huffman_base;
	size_t			 sc_huffman_size;

	kcondvar_t		 sc_cv;
	kmutex_t		 sc_mtx;

#define UDL_DECOMPRDY	(1 << 0)
#define UDL_COMPRDY	(1 << 1)
	uint32_t		 sc_flags;
#ifdef UDL_EVENT_COUNTERS
	struct evcnt		 sc_ev_cmdq_get;
	struct evcnt		 sc_ev_cmdq_put;
	struct evcnt		 sc_ev_cmdq_wait;
	struct evcnt		 sc_ev_cmdq_timeout;
#endif
};

/*
 * Chip commands.
 */
#define UDL_CTRL_CMD_READ_EDID		0x02
#define UDL_CTRL_CMD_WRITE_1		0x03
#define UDL_CTRL_CMD_READ_1		0x04
#define UDL_CTRL_CMD_READ_STATUS	0x06
#define UDL_CTRL_CMD_SET_KEY		0x12

#define UDL_BULK_SOC			0xaf	/* start of command token */

#define UDL_BULK_CMD_REG_WRITE_1	0x20	/* write 1 byte to register */
#define UDL_BULK_CMD_EOC		0xa0	/* end of command stack */
#define UDL_BULK_CMD_DECOMP		0xe0	/* send decompression table */

#define UDL_BULK_CMD_FB_BASE8		0x60
#define UDL_BULK_CMD_FB_WRITE8		(UDL_BULK_CMD_FB_BASE8 | 0x00)
#define UDL_BULK_CMD_FB_RLE8		(UDL_BULK_CMD_FB_BASE8 | 0x01)
#define UDL_BULK_CMD_FB_COPY8		(UDL_BULK_CMD_FB_BASE8 | 0x02)
#define UDL_BULK_CMD_FB_BASE16		0x68
#define UDL_BULK_CMD_FB_WRITE16		(UDL_BULK_CMD_FB_BASE16 | 0x00)
#define UDL_BULK_CMD_FB_RLE16		(UDL_BULK_CMD_FB_BASE16 | 0x01)
#define UDL_BULK_CMD_FB_COPY16		(UDL_BULK_CMD_FB_BASE16 | 0x02)
#define UDL_BULK_CMD_FB_COMP		0x10

/*
 * Chip registers.
 */
#define UDL_REG_COLORDEPTH		0x00
 #define UDL_REG_COLORDEPTH_16		0x00
 #define UDL_REG_COLORDEPTH_24		0x01
#define UDL_REG_XDISPLAYSTART		0x01
#define UDL_REG_XDISPLAYEND		0x03
#define UDL_REG_YDISPLAYSTART		0x05
#define UDL_REG_YDISPLAYEND		0x07
#define UDL_REG_XENDCOUNT		0x09
#define UDL_REG_HSYNCSTART		0x0b
#define UDL_REG_HSYNCEND		0x0d
#define UDL_REG_HPIXELS			0x0f
#define UDL_REG_YENDCOUNT		0x11
#define UDL_REG_VSYNCSTART		0x13
#define UDL_REG_VSYNCEND		0x15
#define UDL_REG_VPIXELS			0x17
#define UDL_REG_PIXELCLOCK5KHZ		0x1b
#define UDL_REG_BLANK			0x1f
 #define UDL_REG_BLANK_OFF		0x00
 #define UDL_REG_BLANK_ON		0x01
#define UDL_REG_ADDR_START16		0x20
#define UDL_REG_ADDR_STRIDE16		0x23
#define UDL_REG_ADDR_START8		0x26
#define UDL_REG_ADDR_STRIDE8		0x29
#define UDL_REG_SYNC			0xff

/*
 * Compression.
 */
struct udl_huffman {
	uint8_t		bit_count;
	uint8_t		pad[3];
	uint32_t	bit_pattern;
};
#define UDL_HUFFMAN_RECORD_SIZE		sizeof(struct udl_huffman)
#define UDL_HUFFMAN_RECORDS		(65536 + 1)
#define UDL_HUFFMAN_BASE		(((UDL_HUFFMAN_RECORDS - 1) / 2) * \
					    UDL_HUFFMAN_RECORD_SIZE)
