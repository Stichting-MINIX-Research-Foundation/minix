#ifndef __DEBUGREG_H__
#define __DEBUGREG_H__

/* DR6: status flags */
#define DR6_B(bp)	(1 << (bp))	/* breakpoint was triggered */
#define DR6_BD		(1 << 13)	/* debug register access detected */
#define DR6_BS		(1 << 14)	/* single step */
#define DR6_BT		(1 << 15)	/* task switch */

/* DR7: control flags */
#define	DR7_L(bp)	(1 << (2*(bp)))		/* breakpoint armed locally */
#define	DR7_G(bp)	(1 << (1+2*(bp)))	/* breakpoint armed globally */
#define	DR7_LE		(1 << 8)		/* exact local breakpoints */
#define	DR7_GE		(1 << 9)		/* exact global breakpoints */
#define	DR7_GD		(1 << 13)		/* detect debug reg movs */

#define	DR7_RW_MASK(bp)		(3 << (16+4*(bp)))	
#define	DR7_RW_EXEC(bp)		(0 << (16+4*(bp)))	/* execute */
#define	DR7_RW_WRITE(bp)	(1 << (16+4*(bp)))	/* write */
#define	DR7_RW_IO(bp)		(2 << (16+4*(bp)))	/* IO */
#define	DR7_RW_RW(bp)		(3 << (16+4*(bp)))	/* read or write */

#define	DR7_LN_MASK(bp)	(3 << (18+4*(bp)))
#define	DR7_LN_1(bp)	(0 << (18+4*(bp)))	/* 1 byte */
#define	DR7_LN_2(bp)	(1 << (18+4*(bp)))	/* 2 bytes */
#define	DR7_LN_8(bp)	(2 << (18+4*(bp)))	/* 8 bytes */
#define	DR7_LN_4(bp)	(3 << (18+4*(bp)))	/* 4 bytes */

/* debugreg.S */
void ld_dr0(phys_bytes value);
void ld_dr1(phys_bytes value);
void ld_dr2(phys_bytes value);
void ld_dr3(phys_bytes value);
void ld_dr6(unsigned long value);
void ld_dr7(unsigned long value);
phys_bytes st_dr0(void); 
phys_bytes st_dr1(void); 
phys_bytes st_dr2(void); 
phys_bytes st_dr3(void); 
unsigned long st_dr6(void); 
unsigned long st_dr7(void); 

#endif /* __DEBUGREG_H__ */

