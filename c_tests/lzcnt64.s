/* lzcnt_bsr64.s  --  x86-64 Linux GAS tests: LZCNT and BSR
 *
 * Build:
 *   as -o lzcnt_bsr64.o lzcnt_bsr64.s
 *   ld -o lzcnt_bsr64 lzcnt_bsr64.o
 *
 * Run:
 *   ./lzcnt_bsr64
 *   echo $?
 *
 * Requires a processor/emulator with LZCNT support.
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

        /* RFLAGS bits */
        .set CF_BIT, 0x0000000000000001
        .set ZF_BIT, 0x0000000000000040
        .set CF_ZF_MASK, (CF_BIT | ZF_BIT)

/* ---------- helpers ---------- */
.macro FAIL code
        mov     $SYS_exit, %rax
        mov     $\code, %rdi
        syscall
.endm

.macro SNAP_RFLAGS reg
        pushfq
        popq    \reg
.endm

/* NOTE: cmpq $imm64,%reg is not generally encodable. Use movabs+cmp. */
.macro ASSERT_EQ64 reg, imm64, code
        movabs  $\imm64, %r11
        cmpq    %r11, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ32 reg, imm32, code
        cmpl    $\imm32, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ16 reg, imm16, code
        cmpw    $\imm16, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* Assert (saved_flags & (CF|ZF)) == expected_mask. */
.macro ASSERT_CF_ZF savedflags, expected_mask, code
        movq    \savedflags, %r10
        andq    $CF_ZF_MASK, %r10
        cmpq    $\expected_mask, %r10
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* Assert only ZF; all other BSR flags are undefined. */
.macro ASSERT_ZF savedflags, expected_mask, code
        movq    \savedflags, %r10
        andq    $ZF_BIT, %r10
        cmpq    $\expected_mask, %r10
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        /* ---------------------------------------------------------
         * LZCNT register operands
         *
         * CF=1 only for a zero source.
         * ZF=1 only when the result is zero (source MSB was set).
         * --------------------------------------------------------- */

        /* T01: lzcntw 0 -> 16; CF=1, ZF=0 */
        movw    $0x0000, %ax
        lzcntw  %ax, %dx
        SNAP_RFLAGS %r12
        ASSERT_EQ16  %dx, 16, 1
        ASSERT_CF_ZF %r12, CF_BIT, 1

        /* T02: lzcntw 1 -> 15; CF=0, ZF=0 */
        movw    $0x0001, %ax
        lzcntw  %ax, %dx
        SNAP_RFLAGS %r12
        ASSERT_EQ16  %dx, 15, 2
        ASSERT_CF_ZF %r12, 0, 2

        /* T03: lzcntw 0x8000 -> 0; CF=0, ZF=1 */
        movw    $0x8000, %ax
        lzcntw  %ax, %dx
        SNAP_RFLAGS %r12
        ASSERT_EQ16  %dx, 0, 3
        ASSERT_CF_ZF %r12, ZF_BIT, 3

        /* T04: lzcntl 0 -> 32 and zero-extends destination */
        movabs  $0xffffffffffffffff, %rdx
        xorl    %eax, %eax
        lzcntl  %eax, %edx
        SNAP_RFLAGS %r12
        ASSERT_EQ64  %rdx, 32, 4
        ASSERT_CF_ZF %r12, CF_BIT, 4

        /* T05: lzcntl 0x00010000 -> 15 */
        movl    $0x00010000, %eax
        lzcntl  %eax, %edx
        SNAP_RFLAGS %r12
        ASSERT_EQ32  %edx, 15, 5
        ASSERT_CF_ZF %r12, 0, 5

        /* T06: lzcntl 0x80000000 -> 0; ZF=1 */
        movl    $0x80000000, %eax
        lzcntl  %eax, %edx
        SNAP_RFLAGS %r12
        ASSERT_EQ32  %edx, 0, 6
        ASSERT_CF_ZF %r12, ZF_BIT, 6

        /* T07: lzcntq 0 -> 64; CF=1, ZF=0 */
        xorq    %rax, %rax
        lzcntq  %rax, %rdx
        SNAP_RFLAGS %r12
        ASSERT_EQ64  %rdx, 64, 7
        ASSERT_CF_ZF %r12, CF_BIT, 7

        /* T08: lzcntq 0x0000000100000000 -> 31 */
        movabs  $0x0000000100000000, %rax
        lzcntq  %rax, %rdx
        SNAP_RFLAGS %r12
        ASSERT_EQ64  %rdx, 31, 8
        ASSERT_CF_ZF %r12, 0, 8

        /* T09: lzcntq 0x8000..0000 -> 0; ZF=1 */
        movabs  $0x8000000000000000, %rax
        lzcntq  %rax, %rdx
        SNAP_RFLAGS %r12
        ASSERT_EQ64  %rdx, 0, 9
        ASSERT_CF_ZF %r12, ZF_BIT, 9


        /* ---------------------------------------------------------
         * BSR register operands
         *
         * ZF=0 for nonzero input and the result is the index of the
         * most-significant set bit. For zero input, ZF=1 and the
         * destination value is architecturally undefined.
         * --------------------------------------------------------- */

        /* T10: bsrw 1 -> 0; ZF=0 */
        movw    $0x0001, %ax
        bsrw    %ax, %dx
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %dx, 0, 10
        ASSERT_ZF   %r12, 0, 10

        /* T11: bsrw 0x8000 -> 15; ZF=0 */
        movw    $0x8000, %ax
        bsrw    %ax, %dx
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %dx, 15, 11
        ASSERT_ZF   %r12, 0, 11

        /* T12: bsrw 0 -> ZF=1; destination not checked */
        xorw    %ax, %ax
        bsrw    %ax, %dx
        SNAP_RFLAGS %r12
        ASSERT_ZF   %r12, ZF_BIT, 12

        /* T13: bsrl 0x00010000 -> 16 and zero-extends destination */
        movabs  $0xffffffffffffffff, %rdx
        movl    $0x00010000, %eax
        bsrl    %eax, %edx
        SNAP_RFLAGS %r12
        ASSERT_EQ64 %rdx, 16, 13
        ASSERT_ZF   %r12, 0, 13

        /* T14: bsrl 0x80000000 -> 31; ZF=0 */
        movl    $0x80000000, %eax
        bsrl    %eax, %edx
        SNAP_RFLAGS %r12
        ASSERT_EQ32 %edx, 31, 14
        ASSERT_ZF   %r12, 0, 14

        /* T15: bsrl 0 -> ZF=1; destination not checked */
        xorl    %eax, %eax
        bsrl    %eax, %edx
        SNAP_RFLAGS %r12
        ASSERT_ZF   %r12, ZF_BIT, 15

        /* T16: bsrq 0x0000000100000000 -> 32; ZF=0 */
        movabs  $0x0000000100000000, %rax
        bsrq    %rax, %rdx
        SNAP_RFLAGS %r12
        ASSERT_EQ64 %rdx, 32, 16
        ASSERT_ZF   %r12, 0, 16

        /* T17: bsrq 0x8000..0000 -> 63; ZF=0 */
        movabs  $0x8000000000000000, %rax
        bsrq    %rax, %rdx
        SNAP_RFLAGS %r12
        ASSERT_EQ64 %rdx, 63, 17
        ASSERT_ZF   %r12, 0, 17

        /* T18: bsrq 0 -> ZF=1; destination not checked */
        xorq    %rax, %rax
        bsrq    %rax, %rdx
        SNAP_RFLAGS %r12
        ASSERT_ZF   %r12, ZF_BIT, 18


        /* ---------------------------------------------------------
         * Memory operand sanity and RIP-relative decoding
         * --------------------------------------------------------- */

        /* T19: lzcntw mem16(0x0040) -> 9 */
        lzcntw  mem16(%rip), %ax
        SNAP_RFLAGS %r12
        ASSERT_EQ16  %ax, 9, 19
        ASSERT_CF_ZF %r12, 0, 19

        /* T20: lzcntl mem32(0x00000008) -> 28 */
        lzcntl  mem32(%rip), %eax
        SNAP_RFLAGS %r12
        ASSERT_EQ32  %eax, 28, 20
        ASSERT_CF_ZF %r12, 0, 20

        /* T21: lzcntq mem64(0x0000010000000000) -> 23 */
        lzcntq  mem64(%rip), %rax
        SNAP_RFLAGS %r12
        ASSERT_EQ64  %rax, 23, 21
        ASSERT_CF_ZF %r12, 0, 21

        /* T22: bsrw mem16(0x0040) -> 6 */
        bsrw    mem16(%rip), %ax
        SNAP_RFLAGS %r12
        ASSERT_EQ16 %ax, 6, 22
        ASSERT_ZF   %r12, 0, 22

        /* T23: bsrl mem32(0x00000008) -> 3 */
        bsrl    mem32(%rip), %eax
        SNAP_RFLAGS %r12
        ASSERT_EQ32 %eax, 3, 23
        ASSERT_ZF   %r12, 0, 23

        /* T24: bsrq mem64(0x0000010000000000) -> 40 */
        bsrq    mem64(%rip), %rax
        SNAP_RFLAGS %r12
        ASSERT_EQ64 %rax, 40, 24
        ASSERT_ZF   %r12, 0, 24


        /* success: write(1,"success\n",8) */
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     success_msg(%rip), %rsi
        mov     $8, %rdx
        syscall

        /* exit(0) */
        mov     $SYS_exit, %rax
        xor     %rdi, %rdi
        syscall


        .section .rodata
        .balign 8
mem16:
        .word   0x0040
mem32:
        .long   0x00000008
mem64:
        .quad   0x0000010000000000

success_msg:
        .ascii  "success\n"
