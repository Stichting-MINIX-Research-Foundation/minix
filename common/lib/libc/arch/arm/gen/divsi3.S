/*	$NetBSD: divsi3.S,v 1.13 2013/09/12 15:36:14 joerg Exp $	*/

/*
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>

#if defined(__thumb__) && !defined(_ARM_ARCH_T2)
ARM_ENTRY(__divsi3)
#else
ENTRY(__divsi3)
#endif
#if defined(__ARM_ARCH_EXT_IDIV__)
# if defined(__ARM_EABI__)
	mov	r3, r0		@ save for mls
# endif
	sdiv	r0, r0, r1
# if defined(__ARM_EABI__)
	mls	r1, r0, r1, r3	@ return modulus in r1
# endif
	RET
#elif defined(__ARM_EABI__) && defined(_LIBC)
	cmp	r1, #0		@ dividing by 0?
	beq	.Ldiv0		@ call __aeabi_idiv0
	ldr	r2, .Lhwdiv_present
#ifdef __PIC__
	add	r2, r2, pc	@ pc = &.LPIC0
# endif
	ldr	r2, [r2]
.LPIC0:	cmp	r2, #0
	beq	__divide
	mov	r3, r0
# if defined(__ARM_ARCH_EXT_IDIV__)
	sdiv	r0, r0, r1
	mls	r1, r0, r1, r3	@ return modulus in r1
# elif defined(__thumb__) && defined(_ARM_ARCH_T2)
	.inst.w	0xfb90f0f1
	.inst.w	0xfb003111
# else
	.inst	0xe710f110
	.inst	0xe0613190
# endif
	RET

	.align	0
.Lhwdiv_present:
	.word	REL_SYM(_libc_arm_hwdiv_present, .LPIC0)

	.align	0
.Ldiv0:	push	{r0, lr}	/* save r0 */
	cmp	r0, #0
	mvnge	r0, #0x80000000	/* INT_MAX = 0x7fffffff */
	movlt	r0, #0x80000000	/* INT_MIN = 0x80000000 */
	bl	_C_LABEL(__aeabi_idiv0)
	pop	{r1, pc}	/* restore r0 as r1 */
#else /* !__ARM_EABI__ */
	b	__divide
#endif
END(__divsi3)

#if defined(__ARM_EABI__)
STRONG_ALIAS(__aeabi_idivmod, __divsi3)
STRONG_ALIAS(__aeabi_idiv, __divsi3)
#if defined(PIC_SYMVER)
	.symver	__aeabi_idiv,__aeabi_idiv@@GCC_3.5
	.symver	__aeabi_idivmod,__aeabi_idivmod@@GCC_3.5
#endif
#endif
