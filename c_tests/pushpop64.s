/* pushpop64.s -- x86-64 Linux GAS tests: push/pop forms (incl. 16-bit in long mode)
 *
 * Build:
 *   as -o pushpop64.o pushpop64.s
 *   ld -o pushpop64 pushpop64.o
 *
 * Run:
 *   ./pushpop64
 *   echo $?
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

/* ---------- helpers ---------- */
.macro FAIL code
        mov     $SYS_exit, %rax
        mov     $\code, %rdi
        syscall
.endm

.macro SAVE_RSP reg
        movq    %rsp, \reg
.endm

.macro ASSERT_REG_EQ r1, r2, code
        cmpq    \r2, \r1
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ64 reg, imm64, code
        movabs  $\imm64, %r11
        cmpq    %r11, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ16 reg16, imm16, code
        cmpw    $\imm16, \reg16
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_MEM64_IMM sym, imm64, code
        movq    \sym, %r11
        movabs  $\imm64, %r10
        cmpq    %r10, %r11
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_MEM16_IMM sym, imm16, code
        movzwq  \sym, %r15
        movabs  $\imm16, %r11
        cmpq    %r11, %r15
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        /* Use private stack buffer */
        lea     stack_space_end(%rip), %rsp

        /* =========================================================
         * T01: push/pop r64 + RSP delta
         * ========================================================= */
        SAVE_RSP %r12
        movabs  $0xAABBCCDDEEFF0011, %rax
        pushq   %rax
        SAVE_RSP %r13
        lea     -8(%r12), %r14
        ASSERT_REG_EQ %r13, %r14, 1

        xor     %rax, %rax
        popq    %rax
        SAVE_RSP %r13
        ASSERT_EQ64 %rax, 0xAABBCCDDEEFF0011, 1
        ASSERT_REG_EQ %r13, %r12, 1

        /* =========================================================
         * T02: push imm8 sign-extended to 64-bit (use push, no suffix)
         * ========================================================= */
        SAVE_RSP %r12
        push    $-1                 /* imm8 encoding likely; sign-extends */
        popq    %rax
        ASSERT_EQ64 %rax, 0xFFFFFFFFFFFFFFFF, 2
        SAVE_RSP %r13
        ASSERT_REG_EQ %r13, %r12, 2

        /* =========================================================
         * T03: push imm32 sign-extended to 64-bit (use push, no suffix)
         * 0x80000000 -> 0xFFFFFFFF80000000
         * ========================================================= */
        SAVE_RSP %r12
        push    $-2147483648
        popq    %rax
        ASSERT_EQ64 %rax, 0xFFFFFFFF80000000, 3
        SAVE_RSP %r13
        ASSERT_REG_EQ %r13, %r12, 3

        /* =========================================================
         * T04: push r/m64 and pop r/m64
         * ========================================================= */
        movabs  $0x0123456789ABCDEF, %rax
        movq    %rax, memq(%rip)

        SAVE_RSP %r12
        pushq   memq(%rip)
        popq    %rax
        ASSERT_EQ64 %rax, 0x0123456789ABCDEF, 4
        SAVE_RSP %r13
        ASSERT_REG_EQ %r13, %r12, 4

        /* pop into memory */
        movq    $0, memq(%rip)
        movabs  $0xDEADBEEFCAFEBABE, %rax
        pushq   %rax
        popq    memq(%rip)
        ASSERT_MEM64_IMM memq(%rip), 0xDEADBEEFCAFEBABE, 4

        /* =========================================================
         * T05: push %rsp pushes original RSP (before decrement)
         * ========================================================= */
        lea     stack_space_end(%rip), %rsp
        SAVE_RSP %r12
        pushq   %rsp
        movq    (%rsp), %rax
        ASSERT_REG_EQ %rax, %r12, 5
        popq    %rax

        /* =========================================================
         * T06: pop %rsp loads new RSP from stack
         * ========================================================= */
        lea     stack_space_end(%rip), %rsp
        lea     stack_space_end-128(%rip), %rax
        pushq   %rax
        popq    %rsp
        SAVE_RSP %r13
        ASSERT_REG_EQ %r13, %rax, 6
        lea     stack_space_end(%rip), %rsp

        /* =========================================================
         * T07: pushfq/popfq sanity (CF reflected and restorable)
         * ========================================================= */
        clc
        pushfq
        popq    %rax
        testq   $1, %rax
        jz      t07_ok0
        FAIL 7
t07_ok0:
        stc
        pushfq
        popq    %rax
        testq   $1, %rax
        jnz     t07_ok1
        FAIL 7
t07_ok1:
        clc
        pushfq
        popq    %rax
        orq     $1, %rax
        pushq   %rax
        popfq
        pushfq
        popq    %rax
        testq   $1, %rax
        jnz     t07_ok2
        FAIL 7
t07_ok2:

        /* =========================================================
         * 16-bit PUSH/POP in 64-bit mode (0x66 operand-size override)
         * ========================================================= */

        /* T08: pushw %ax / popw %cx */
        lea     stack_space_end(%rip), %rsp
        movabs  $0x1122334455667788, %rcx
        movw    $0xBEEF, %ax

        SAVE_RSP %r12
        pushw   %ax
        SAVE_RSP %r13
        lea     -2(%r12), %r14
        ASSERT_REG_EQ %r13, %r14, 8

        movzwq  (%rsp), %r15
        ASSERT_EQ64 %r15, 0x000000000000BEEF, 8

        popw    %cx
        SAVE_RSP %r13
        ASSERT_REG_EQ %r13, %r12, 8

        ASSERT_EQ16 %cx, 0xBEEF, 8
        ASSERT_EQ64 %rcx, 0x112233445566BEEF, 8

        /* T09: 16-bit immediate push via bytes: 66 6A FF */
        lea     stack_space_end(%rip), %rsp
        movabs  $0xA1A2A3A4A5A6A7A8, %rdx

        SAVE_RSP %r12
        .byte   0x66, 0x6A, 0xFF           /* pushw $-1 (imm8) */
        SAVE_RSP %r13
        lea     -2(%r12), %r14
        ASSERT_REG_EQ %r13, %r14, 9

        movzwq  (%rsp), %r15
        ASSERT_EQ64 %r15, 0x000000000000FFFF, 9

        popw    %dx
        SAVE_RSP %r13
        ASSERT_REG_EQ %r13, %r12, 9

        ASSERT_EQ16 %dx, 0xFFFF, 9
        ASSERT_EQ64 %rdx, 0xA1A2A3A4A5A6FFFF, 9

        /* T10: pushw r/m16 */
        lea     stack_space_end(%rip), %rsp
        movw    $0x1357, memw(%rip)

        SAVE_RSP %r12
        pushw   memw(%rip)
        SAVE_RSP %r13
        lea     -2(%r12), %r14
        ASSERT_REG_EQ %r13, %r14, 10

        movzwq  (%rsp), %r15
        ASSERT_EQ64 %r15, 0x0000000000001357, 10

        popw    %ax
        SAVE_RSP %r13
        ASSERT_REG_EQ %r13, %r12, 10
        ASSERT_EQ16 %ax, 0x1357, 10

        /* T11: popw r/m16 */
        lea     stack_space_end(%rip), %rsp
        movw    $0x0000, memw(%rip)
        movw    $0xB00B, %ax

        SAVE_RSP %r12
        pushw   %ax
        SAVE_RSP %r13
        lea     -2(%r12), %r14
        ASSERT_REG_EQ %r13, %r14, 11

        popw    memw(%rip)
        SAVE_RSP %r13
        ASSERT_REG_EQ %r13, %r12, 11
        ASSERT_MEM16_IMM memw(%rip), 0xB00B, 11

        /* success */
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     success_msg(%rip), %rsi
        mov     $8, %rdx
        syscall

        mov     $SYS_exit, %rax
        xor     %rdi, %rdi
        syscall


        .section .bss
        .balign 16
stack_space:
        .skip  8192
stack_space_end:

        .section .data
        .balign 8
memq:
        .quad 0
        .balign 2
memw:
        .word 0

        .section .rodata
success_msg:
        .ascii "success\n"
