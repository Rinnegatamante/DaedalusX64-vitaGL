.arm

.global _EnterDynaRec

_EnterDynaRec:
	push {r4-r11, lr}
	mov r12, r1
	mov r10, r2
	bx	r0