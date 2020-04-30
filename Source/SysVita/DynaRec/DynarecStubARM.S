.arm

.align 4
.global _EnterDynaRec
.type _EnterDynaRec, %function

_EnterDynaRec:
    push {r4-r11, lr}
    mov r12, r1
    mov r10, r2
    bx    r0
