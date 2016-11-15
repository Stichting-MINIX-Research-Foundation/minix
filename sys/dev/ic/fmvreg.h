/*	$NetBSD: fmvreg.h,v 1.1 2002/10/05 15:16:11 tsutsui Exp $	*/

/*
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold,
 * in both source and binary form provided that the above copyright,
 * these terms and the following disclaimer are retained.  The name of
 * the author and/or the contributor may not be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Hardware specification of various 86960/86965 based Ethernet cards.
 * Contributed by M.S. <seki@sysrap.cs.fujitsu.co.jp>
 */

/*
 * Registers on FMV-180 series' ISA bus interface ASIC.
 * I'm not sure the following register names are appropriate.
 * Doesn't it look silly, eh?  FIXME.
 */

#define FE_FMV0		16	/* Hardware status.		*/
#define FE_FMV1		17	/* Hardrare type?  Always 0	*/
#define FE_FMV2		18	/* Hardware configuration.	*/
#define FE_FMV3		19	/* Hardware enable.		*/
#define FE_FMV4		20	/* Station address #1		*/
#define FE_FMV5		21	/* Station address #2		*/
#define FE_FMV6		22	/* Station address #3		*/
#define FE_FMV7		23	/* Station address #4		*/
#define FE_FMV8		24	/* Station address #5		*/
#define FE_FMV9		25	/* Station address #6		*/
#define FE_FMV10	26	/* Unknown; to be set to 0.	*/

/*
 * FMV-180 series' ASIC register values.
 */

/* Magic value in FMV0 register.  */
#define FE_FMV0_MAGIC_MASK	0x78
#define FE_FMV0_MAGIC_VALUE	0x50

/* Model identification.  */
#define FE_FMV0_MODEL		0x07
#define FE_FMV0_MODEL_FMV181	0x05	/* FMV-181/181A		*/
#define FE_FMV0_MODEL_FMV182	0x03	/* FMV-182/182A/184	*/
#define FE_FMV0_MODEL_FMV183	0x04	/* FMV-183		*/

/* Card type ID */
#define FE_FMV1_MAGIC_MASK	0xB0
#define FE_FMV1_MAGIC_VALUE	0x00
#define FE_FMV1_CARDID_REV	0x0F
#define FE_FMV1_CARDID_REV_A	0x01	/* FMV-181A/182A	*/
#define FE_FMV1_CARDID_PNP	0x08	/* FMV-183/184		*/

/* I/O port address assignment.  */
#define FE_FMV2_ADDR		0x07
#define FE_FMV2_ADDR_SHIFT	0

/* Boot ROM address assignment.  */
#define FE_FMV2_ROM		0x38
#define FE_FMV2_ROM_SHIFT	3

/* IRQ assignment.  */
#define FE_FMV2_IRQ		0xC0
#define FE_FMV2_IRQ_SHIFT	6

/* Hardware(?) enable flag.  */
#define FE_FMV3_ENABLE_FLAG	0x80

/* Extra bits in FMV3 register.  Always 0?  */
#define FE_FMV3_EXTRA_MASK	0x7F
#define FE_FMV3_EXTRA_VALUE	0x00
