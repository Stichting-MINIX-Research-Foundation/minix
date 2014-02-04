/*
 *   Z M O D E M . H     Manifest constants for ZMODEM
 *    application to application file transfer protocol
 *    05-23-87  Chuck Forsberg Omen Technology Inc
 */
#define ZPAD '*'	/* 052 Padding character begins frames */
#define ZDLE 030	/* Ctrl-X Zmodem escape - `ala BISYNC DLE */
#define ZDLEE (ZDLE^0100)	/* Escaped ZDLE as transmitted */
#define ZBIN 'A'	/* Binary frame indicator */
#define ZHEX 'B'	/* HEX frame indicator */
#define ZBIN32 'C'	/* Binary frame with 32 bit FCS */

/* Frame types (see array "frametypes" in zm.c) */
#define ZRQINIT	0	/* Request receive init */
#define ZRINIT	1	/* Receive init */
#define ZSINIT 2	/* Send init sequence (optional) */
#define ZACK 3		/* ACK to above */
#define ZFILE 4		/* File name from sender */
#define ZSKIP 5		/* To sender: skip this file */
#define ZNAK 6		/* Last packet was garbled */
#define ZABORT 7	/* Abort batch transfers */
#define ZFIN 8		/* Finish session */
#define ZRPOS 9		/* Resume data trans at this position */
#define ZDATA 10	/* Data packet(s) follow */
#define ZEOF 11		/* End of file */
#define ZFERR 12	/* Fatal Read or Write error Detected */
#define ZCRC 13		/* Request for file CRC and response */
#define ZCHALLENGE 14	/* Receiver's Challenge */
#define ZCOMPL 15	/* Request is complete */
#define ZCAN 16		/* Other end canned session with CAN*5 */
#define ZFREECNT 17	/* Request for free bytes on filesystem */
#define ZCOMMAND 18	/* Command from sending program */
#define ZSTDERR 19	/* Output to standard error, data follows */

/* ZDLE sequences */
#define ZCRCE 'h'	/* CRC next, frame ends, header packet follows */
#define ZCRCG 'i'	/* CRC next, frame continues nonstop */
#define ZCRCQ 'j'	/* CRC next, frame continues, ZACK expected */
#define ZCRCW 'k'	/* CRC next, ZACK expected, end of frame */
#define ZRUB0 'l'	/* Translate to rubout 0177 */
#define ZRUB1 'm'	/* Translate to rubout 0377 */

/* zdlread return values (internal) */
/* -1 is general error, -2 is timeout */
#define GOTOR 0400
#define GOTCRCE (ZCRCE|GOTOR)	/* ZDLE-ZCRCE received */
#define GOTCRCG (ZCRCG|GOTOR)	/* ZDLE-ZCRCG received */
#define GOTCRCQ (ZCRCQ|GOTOR)	/* ZDLE-ZCRCQ received */
#define GOTCRCW (ZCRCW|GOTOR)	/* ZDLE-ZCRCW received */
#define GOTCAN	(GOTOR|030)	/* CAN*5 seen */

/* Byte positions within header array */
#define ZF0	3	/* First flags byte */
#define ZF1	2
#define ZF2	1
#define ZF3	0
#define ZP0	0	/* Low order 8 bits of position */
#define ZP1	1
#define ZP2	2
#define ZP3	3	/* High order 8 bits of file position */

/* Bit Masks for ZRINIT flags byte ZF0 */
#define CANFDX	01	/* Rx can send and receive true FDX */
#define CANOVIO	02	/* Rx can receive data during disk I/O */
#define CANBRK	04	/* Rx can send a break signal */
#define CANCRY	010	/* Receiver can decrypt */
#define CANLZW	020	/* Receiver can uncompress */
#define CANFC32	040	/* Receiver can use 32 bit Frame Check */
#define ESCCTL 0100	/* Receiver expects ctl chars to be escaped */
#define ESC8   0200	/* Receiver expects 8th bit to be escaped */

/* Parameters for ZSINIT frame */
#define ZATTNLEN 32	/* Max length of attention string */
/* Bit Masks for ZSINIT flags byte ZF0 */
#define TESCCTL 0100	/* Transmitter expects ctl chars to be escaped */
#define TESC8   0200	/* Transmitter expects 8th bit to be escaped */

/* Parameters for ZFILE frame */
/* Conversion options one of these in ZF0 */
#define ZCBIN	1	/* Binary transfer - inhibit conversion */
#define ZCNL	2	/* Convert NL to local end of line convention */
#define ZCRESUM	3	/* Resume interrupted file transfer */
/* Management include options, one of these ored in ZF1 */
#define ZMSKNOLOC	0200	/* Skip file if not present at rx */
/* Management options, one of these ored in ZF1 */
#define ZMMASK	037	/* Mask for the choices below */
#define ZMNEWL	1	/* Transfer if source newer or longer */
#define ZMCRC	2	/* Transfer if different file CRC or length */
#define ZMAPND	3	/* Append contents to existing file (if any) */
#define ZMCLOB	4	/* Replace existing file */
#define ZMNEW	5	/* Transfer if source newer */
	/* Number 5 is alive ... */
#define ZMDIFF	6	/* Transfer if dates or lengths different */
#define ZMPROT	7	/* Protect destination file */
/* Transport options, one of these in ZF2 */
#define ZTLZW	1	/* Lempel-Ziv compression */
#define ZTCRYPT	2	/* Encryption */
#define ZTRLE	3	/* Run Length encoding */
/* Extended options for ZF3, bit encoded */
#define ZXSPARS	64	/* Encoding for sparse file operations */

/* Parameters for ZCOMMAND frame ZF0 (otherwise 0) */
#define ZCACK1	1	/* Acknowledge, then do command */

/* Globals used by ZMODEM functions */
extern int Rxframeind;	/* ZBIN ZBIN32, or ZHEX type of frame received */
extern int Rxtype;		/* Type of header received */
extern int Rxcount;		/* Count of data bytes received */
extern int Zrwindow;	/* RX window size (controls garbage count) */
extern int Rxtimeout;	/* Tenths of seconds to wait for something */
extern char Rxhdr[4];	/* Received header */
extern char Txhdr[4];	/* Transmitted header */
extern long Rxpos;	/* Received file position */
extern long Txpos;	/* Transmitted file position */
extern int Txfcs32;		/* TURE means send binary frames with 32 bit FCS */
extern int Crc32t;		/* Display flag indicating 32 bit CRC being sent */
extern int Crc32;		/* Display flag indicating 32 bit CRC being received */
extern int Znulls;		/* Number of nulls to send at beginning of ZDATA hdr */
extern char Attn[ZATTNLEN+1];	/* Attention string rx sends to tx on err */

/* crctab.c */

long UPDC32(int b , long c );

/* rbsb.c */

void from_cu(void);
void cucheck(void);
int rdchk(int f );
int rdchk(int f );
int mode(int n );
void sendbrk(void);

/* zm.c */

void zsbhdr(int type , char *hdr );
void zsbh32(char *hdr , int type );
void zshhdr(int type , char *hdr );
void zsdata(char *buf , int length , int frameend );
void zsda32(char *buf , int length , int frameend );
int zrdata(char *buf , int length );
int zrdat32(char *buf , int length );
int zgethdr(char *hdr , int eflag );
int zrbhdr(char *hdr );
int zrbhdr32(char *hdr );
int zrhhdr(char *hdr );
void zputhex(int c );
void zsendline(int c );
int zgethex(void);
int zgeth1(void);
int zdlread(void);
int noxrd7(void);
void stohdr(long pos );
long rclhdr(char *hdr );

/* rz.c sz.c */

void vfile(const char *s, ...);
void bibi(int n );

/* End of ZMODEM.H */
