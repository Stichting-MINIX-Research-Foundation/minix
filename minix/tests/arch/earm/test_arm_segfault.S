.text
.global main
main:
	push {lr}
	ldr r0, =0xDEADBEE0 /* Hopefully this is not mapped... */
	ldr r1, [r0]
	ldr r0, =0x01010100 /* In case we survived, try something else */
	ldr r1, [r0]

	ldr r0, =msg
	bl puts

	mov r0, #0  /* test should check for non-zero exit code / signal */
	pop {pc}
msg:
	.ascii "ERROR - caused no segfault\n"
