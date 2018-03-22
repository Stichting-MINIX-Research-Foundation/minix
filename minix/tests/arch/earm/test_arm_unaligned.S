.text
.global main
main:
	push {lr}
	mov r0, sp

	/* This should work */
	ldr r0, [sp]

	/* Unalign it */
	add r0, #2

	/* Try a non-word aligned word-load, this may work if SCTRL.A == 0 */
	ldr r1, [r0]

	/* Load non-word aligned dword, should die even with SCTRL.A == 0 */
	ldrd r2, r3, [r0]


	ldr r0, =msg
	bl puts

	mov r0, #0  /* test should check for non-zero exit code / signal */
	pop {pc}
msg:
	.ascii "ERROR - caused no sigbus\n"
