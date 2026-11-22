    .text
    .arm
    .align 2
    .hidden qol_field_routes

@ Replaces the conditional branch and following load at main+0x61BAD0.
    .global qol_map_check
    .hidden qol_map_check
    .type qol_map_check, %function
qol_map_check:
    sub     sp, sp, #4              @ destination slot
    push    {r0, r1, ip}
    mrs     ip, apsr
    ldr     r0, .Lmap_routes
.Lmap_pc:
    add     r0, pc, r0
    bgt     .Lmap_visible          @ the original check allowed the map
    ldrb    r1, [r0, #16]
    cmp     r1, #0
    ldreq   r1, [r0, #4]           @ keep the original hidden-map path
    beq     .Lmap_resume
.Lmap_visible:
    ldr     r1, [r0]
    ldr     r2, [r8, #0xa4]         @ replay the original fall-through load
.Lmap_resume:
    str     r1, [sp, #12]
    msr     apsr_nzcvq, ip
    pop     {r0, r1, ip, pc}
.Lmap_routes:
    .word   qol_field_routes - (.Lmap_pc + 8)
    .size qol_map_check, . - qol_map_check

@ Replaces the armor skill query and mov r5, #8 at main+0x61CC94.
    .global qol_boss_check
    .hidden qol_boss_check
    .type qol_boss_check, %function
qol_boss_check:
    ldr     r5, .Lboss_routes
.Lboss_pc:
    add     r5, pc, r5
    ldrb    ip, [r5, #17]
    cmp     ip, #0
    movne   r0, #1
    ldreq   ip, [r5, #8]
    blxeq   ip                     @ r0/r1 still hold the original query arguments
    ldr     ip, [r5, #12]
    mov     r5, #8
    bx      ip
.Lboss_routes:
    .word   qol_field_routes - (.Lboss_pc + 8)
    .size qol_boss_check, . - qol_boss_check
