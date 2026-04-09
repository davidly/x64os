/* muldiv64_ext.s -- x86-64 Linux GAS tests for:
 *   mul / imul (one-operand implicit forms)
 *   div / idiv
 * across 8/16/32/64-bit widths
 *
 * Includes:
 *   - register and memory source variants
 *   - CF/OF checks for mul / one-operand imul
 *   - more edge cases
 *
 * No libc. Prints "success\n" on pass; exits with test-id on failure.
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

        .set CF_BIT, 0x0000000000000001
        .set OF_BIT, 0x0000000000000800
        .set CF_OF_MASK, (CF_BIT | OF_BIT)

.macro FAIL code
        mov     $SYS_exit, %rax
        mov     $\code, %rdi
        syscall
.endm

.macro SNAP_RFLAGS reg
        pushfq
        popq    \reg
.endm

.macro ASSERT_EQ64 reg, imm, code
        movabs  $\imm, %r11
        cmpq    %r11, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ32 reg, imm, code
        cmpl    $\imm, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ16 reg, imm, code
        cmpw    $\imm, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ8 reg, imm, code
        cmpb    $\imm, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_CF_OF saved, expected, code
        movq    \saved, %r10
        andq    $CF_OF_MASK, %r10
        movabs  $\expected, %r11
        cmpq    %r11, %r10
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        /* -------------------------------------------------
         * Unsigned MUL
         * CF=OF=0 iff upper half of product is zero
         * ------------------------------------------------- */

        /* T01: mulb reg source, no overflow: 3 * 4 = 12 */
        movb    $3, %al
        movb    $4, %bl
        mulb    %bl
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %ax, 12, 1
        ASSERT_CF_OF %r12, 0, 1

        /* T02: mulb mem source, overflow: 16 * 16 = 256 = 0x0100 */
        movb    $16, %al
        mulb    u8_16(%rip)
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %ax, 0x0100, 2
        ASSERT_CF_OF %r12, (CF_BIT|OF_BIT), 2

        /* T03: mulw reg source, no overflow: 100 * 200 = 20000 */
        movw    $100, %ax
        movw    $200, %bx
        mulw    %bx
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %dx, 0x0000, 3
        ASSERT_EQ16 %ax, 0x4E20, 3
        ASSERT_CF_OF %r12, 0, 3

        /* T04: mulw mem source, overflow: 0x8000 * 2 = 0x0001:0000 */
        movw    $0x8000, %ax
        mulw    u16_2(%rip)
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %dx, 0x0001, 4
        ASSERT_EQ16 %ax, 0x0000, 4
        ASSERT_CF_OF %r12, (CF_BIT|OF_BIT), 4

        /* T05: mull reg source, no overflow: 65536 * 16 = 1048576 */
        movl    $65536, %eax
        movl    $16, %ecx
        mull    %ecx
        SNAP_RFLAGS %r12
        ASSERT_EQ32 %edx, 0x00000000, 5
        ASSERT_EQ32 %eax, 0x00100000, 5
        ASSERT_CF_OF %r12, 0, 5

        /* T06: mull mem source, overflow: 0xFFFFFFFF * 2 = 0x00000001:FFFFFFFE */
        movl    $0xFFFFFFFF, %eax
        mull    u32_2(%rip)
        SNAP_RFLAGS %r12
        ASSERT_EQ32 %edx, 0x00000001, 6
        ASSERT_EQ32 %eax, 0xFFFFFFFE, 6
        ASSERT_CF_OF %r12, (CF_BIT|OF_BIT), 6

        /* T07: mulq reg source, no overflow: 2^32 * 2 = 2^33 */
        movabs  $0x0000000100000000, %rax
        movq    $2, %rbx
        mulq    %rbx
        SNAP_RFLAGS %r12
        ASSERT_EQ64 %rdx, 0x0000000000000000, 7
        ASSERT_EQ64 %rax, 0x0000000200000000, 7
        ASSERT_CF_OF %r12, 0, 7

        /* T08: mulq mem source, overflow: -1u * 2 = 0x1:FFFFFFFFFFFFFFFE */
        movabs  $0xFFFFFFFFFFFFFFFF, %rax
        mulq    u64_2(%rip)
        SNAP_RFLAGS %r12
        ASSERT_EQ64 %rdx, 0x0000000000000001, 8
        ASSERT_EQ64 %rax, 0xFFFFFFFFFFFFFFFE, 8
        ASSERT_CF_OF %r12, (CF_BIT|OF_BIT), 8


        /* -------------------------------------------------
         * Signed IMUL (one-operand form)
         * CF=OF=0 iff upper half is sign-extension of lower half
         * ------------------------------------------------- */

        /* T09: imulb reg source, no overflow: -3 * 4 = -12 */
        movb    $-3, %al
        movb    $4, %bl
        imulb   %bl
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %ax, 0xFFF4, 9
        ASSERT_CF_OF %r12, 0, 9

        /* T10: imulb mem source, overflow: 100 * 2 = 200 (doesn't fit signed 8-bit) */
        movb    $100, %al
        imulb   s8_2(%rip)
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %ax, 0x00C8, 10
        ASSERT_CF_OF %r12, (CF_BIT|OF_BIT), 10

        /* T11: imulw reg source, no overflow: -100 * 2 = -200 */
        movw    $-100, %ax
        movw    $2, %bx
        imulw   %bx
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %dx, 0xFFFF, 11
        ASSERT_EQ16 %ax, 0xFF38, 11
        ASSERT_CF_OF %r12, 0, 11

        /* T12: imulw mem source, overflow: 20000 * 2 = 40000 */
        movw    $20000, %ax
        imulw   s16_2(%rip)
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %dx, 0x0000, 12
        ASSERT_EQ16 %ax, 0x9C40, 12
        ASSERT_CF_OF %r12, (CF_BIT|OF_BIT), 12

        /* T13: imull reg source, no overflow: -100000 * 2 = -200000 */
        movl    $-100000, %eax
        movl    $2, %ecx
        imull   %ecx
        SNAP_RFLAGS %r12
        ASSERT_EQ32 %edx, 0xFFFFFFFF, 13
        ASSERT_EQ32 %eax, 0xFFFCF2C0, 13
        ASSERT_CF_OF %r12, 0, 13

        /* T14: imull mem source, overflow: 0x40000000 * 4 = 0x00000001:00000000 */
        movl    $0x40000000, %eax
        imull   s32_4(%rip)
        SNAP_RFLAGS %r12
        ASSERT_EQ32 %edx, 0x00000001, 14
        ASSERT_EQ32 %eax, 0x00000000, 14
        ASSERT_CF_OF %r12, (CF_BIT|OF_BIT), 14

        /* T15: imulq reg source, no overflow: -2^32 * 2 = -2^33 */
        movabs  $-4294967296, %rax
        movq    $2, %rbx
        imulq   %rbx
        SNAP_RFLAGS %r12
        ASSERT_EQ64 %rdx, 0xFFFFFFFFFFFFFFFF, 15
        ASSERT_EQ64 %rax, 0xFFFFFFFE00000000, 15
        ASSERT_CF_OF %r12, 0, 15

        /* T16: imulq mem source, overflow: 2^62 * 2 = 2^63 (doesn't fit signed 64-bit) */
        movabs  $0x4000000000000000, %rax
        imulq   s64_2(%rip)
        SNAP_RFLAGS %r12
        ASSERT_EQ64 %rdx, 0x0000000000000000, 16
        ASSERT_EQ64 %rax, 0x8000000000000000, 16
        ASSERT_CF_OF %r12, (CF_BIT|OF_BIT), 16


        /* -------------------------------------------------
         * Unsigned DIV
         * Flags undefined; not tested
         * ------------------------------------------------- */

        /* T17: divb reg source: AX=1000, BL=30 -> AL=33, AH=10 */
        movw    $1000, %ax
        movb    $30, %bl
        divb    %bl
        ASSERT_EQ8  %al, 33, 17
        ASSERT_EQ8  %ah, 10, 17

        /* T18: divw mem source: DX:AX=100000, mem=300 -> AX=333, DX=100 */
        movw    $0x0001, %dx
        movw    $0x86A0, %ax
        divw    u16_300(%rip)
        ASSERT_EQ16 %ax, 333, 18
        ASSERT_EQ16 %dx, 100, 18

        /* T19: divl reg source: EDX:EAX=2^32, ECX=2 -> EAX=0x80000000, EDX=0 */
        movl    $0x00000001, %edx
        xorl    %eax, %eax
        movl    $2, %ecx
        divl    %ecx
        ASSERT_EQ32 %eax, 0x80000000, 19
        ASSERT_EQ32 %edx, 0, 19

        /* T20: divq mem source: 0x0000001000000000 / 16 -> 0x0000000100000000 rem 0 */
        xorq    %rdx, %rdx
        movabs  $0x0000001000000000, %rax
        divq    u64_16(%rip)
        ASSERT_EQ64 %rax, 0x0000000100000000, 20
        ASSERT_EQ64 %rdx, 0, 20


        /* -------------------------------------------------
         * Signed IDIV
         * Flags undefined; not tested
         * ------------------------------------------------- */

        /* T21: idivb mem source: AX=-1000, mem=30 -> AL=-33, AH=-10 */
        movw    $-1000, %ax
        idivb   s8_30(%rip)
        ASSERT_EQ8  %al, 0xDF, 21            /* -33 */
        ASSERT_EQ8  %ah, 0xF6, 21            /* -10 */

        /* T22: idivw reg source: DX:AX=100000, BX=-300 -> AX=-333, DX=100 */
        movw    $0x0001, %dx
        movw    $0x86A0, %ax
        movw    $-300, %bx
        idivw   %bx
        ASSERT_EQ16 %ax, 0xFEB3, 22          /* -333 */
        ASSERT_EQ16 %dx, 0x0064, 22          /* +100 */

        /* T23: idivl mem source: EDX:EAX=-4200000000, mem=60000 -> EAX=-70000, EDX=0 */
        movl    $0xFFFFFFFF, %edx
        movl    $0x05A91600, %eax
        idivl   s32_60000(%rip)
        ASSERT_EQ32 %eax, 0xFFFEEE90, 23     /* -70000 */
        ASSERT_EQ32 %edx, 0x00000000, 23

        /* T24: idivq reg source: -2^63 / 2 -> -2^62, remainder 0 */
        movabs  $0x8000000000000000, %rax
        cqto
        movq    $2, %rbx
        idivq   %rbx
        ASSERT_EQ64 %rax, 0xC000000000000000, 24
        ASSERT_EQ64 %rdx, 0x0000000000000000, 24


        /* success */
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     success_msg(%rip), %rsi
        mov     $8, %rdx
        syscall

        mov     $SYS_exit, %rax
        xor     %rdi, %rdi
        syscall


        .section .rodata
success_msg:
        .ascii "success\n"

        .balign 8
u8_16:      .byte 16
s8_2:       .byte 2
s8_30:      .byte 30

        .balign 8
u16_2:      .word 2
u16_300:    .word 300
s16_2:      .word 2

        .balign 8
u32_2:      .long 2
s32_4:      .long 4
s32_60000:  .long 60000

        .balign 8
u64_2:      .quad 2
u64_16:     .quad 16
s64_2:      .quad 2
