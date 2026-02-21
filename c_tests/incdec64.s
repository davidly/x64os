/* incdec64.s  --  x86-64 Linux GAS tests: INC/DEC (incl. OF edge cases, CF unchanged)
 *
 * Build:
 *   as -o incdec64.o incdec64.s
 *   ld -o incdec64 incdec64.o
 *
 * Run:
 *   ./incdec64
 *   echo $?
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

        /* RFLAGS bits */
        .set CF_BIT, 0x0000000000000001
        .set OF_BIT, 0x0000000000000800
        .set CF_OF_MASK, (CF_BIT | OF_BIT)

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

/* NOTE: cmpq $imm64,%reg is not generally encodable. Use movabs+cmp.  */
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

.macro ASSERT_EQ8 reg, imm8, code
        cmpb    $\imm8, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* Assert (saved_flags & (CF|OF)) == expected_mask (small immediate OK) */
.macro ASSERT_CF_OF savedflags, expected_mask, code
        movq    \savedflags, %r10
        andq    $CF_OF_MASK, %r10
        cmpq    $\expected_mask, %r10
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        /* ---------------------------------------------------------
         * Register tests: 8/16/32/64 bit
         * INC: OF set when max positive -> min negative
         * DEC: OF set when min negative -> max positive
         * Also verify CF unchanged by INC/DEC
         * --------------------------------------------------------- */

        /* T01: incb 0x7F->0x80 sets OF, CF=0 preserved */
        clc
        movb    $0x7f, %al
        incb    %al
        SNAP_RFLAGS %r12
        ASSERT_EQ8   %al, 0x80, 1
        ASSERT_CF_OF %r12, OF_BIT, 1

        /* T02: incb 0x7F->0x80 sets OF, CF=1 preserved */
        stc
        movb    $0x7f, %al
        incb    %al
        SNAP_RFLAGS %r12
        ASSERT_EQ8   %al, 0x80, 2
        ASSERT_CF_OF %r12, (OF_BIT|CF_BIT), 2

        /* T03: incb 0xFF->0x00, OF=0, CF=1 preserved */
        stc
        movb    $0xff, %al
        incb    %al
        SNAP_RFLAGS %r12
        ASSERT_EQ8   %al, 0x00, 3
        ASSERT_CF_OF %r12, CF_BIT, 3

        /* T04: incw 0x7FFF->0x8000 sets OF, CF=0 preserved */
        clc
        movw    $0x7fff, %ax
        incw    %ax
        SNAP_RFLAGS %r12
        ASSERT_EQ16  %ax, 0x8000, 4
        ASSERT_CF_OF %r12, OF_BIT, 4

        /* T05: incl 0x7FFFFFFF->0x80000000 sets OF, CF=1 preserved */
        stc
        movl    $0x7fffffff, %eax
        incl    %eax
        SNAP_RFLAGS %r12
        ASSERT_EQ32  %eax, 0x80000000, 5
        ASSERT_CF_OF %r12, (OF_BIT|CF_BIT), 5

        /* T06: incq 0x7FFF..FFFF->0x8000..0000 sets OF, CF=0 preserved */
        clc
        movabs  $0x7fffffffffffffff, %rax
        incq    %rax
        SNAP_RFLAGS %r12
        ASSERT_EQ64  %rax, 0x8000000000000000, 6
        ASSERT_CF_OF %r12, OF_BIT, 6


        /* T07: decb 0x80->0x7F sets OF, CF=0 preserved */
        clc
        movb    $0x80, %al
        decb    %al
        SNAP_RFLAGS %r12
        ASSERT_EQ8   %al, 0x7f, 7
        ASSERT_CF_OF %r12, OF_BIT, 7

        /* T08: decb 0x80->0x7F sets OF, CF=1 preserved */
        stc
        movb    $0x80, %al
        decb    %al
        SNAP_RFLAGS %r12
        ASSERT_EQ8   %al, 0x7f, 8
        ASSERT_CF_OF %r12, (OF_BIT|CF_BIT), 8

        /* T09: decb 0x00->0xFF, OF=0, CF=0 preserved */
        clc
        movb    $0x00, %al
        decb    %al
        SNAP_RFLAGS %r12
        ASSERT_EQ8   %al, 0xff, 9
        ASSERT_CF_OF %r12, 0x0, 9

        /* T10: decw 0x8000->0x7FFF sets OF, CF=1 preserved */
        stc
        movw    $0x8000, %ax
        decw    %ax
        SNAP_RFLAGS %r12
        ASSERT_EQ16  %ax, 0x7fff, 10
        ASSERT_CF_OF %r12, (OF_BIT|CF_BIT), 10

        /* T11: decl 0x80000000->0x7FFFFFFF sets OF, CF=0 preserved */
        clc
        movl    $0x80000000, %eax
        decl    %eax
        SNAP_RFLAGS %r12
        ASSERT_EQ32  %eax, 0x7fffffff, 11
        ASSERT_CF_OF %r12, OF_BIT, 11

        /* T12: decq 0x8000..0000->0x7FFF..FFFF sets OF, CF=1 preserved */
        stc
        movabs  $0x8000000000000000, %rax
        decq    %rax
        SNAP_RFLAGS %r12
        ASSERT_EQ64  %rax, 0x7fffffffffffffff, 12
        ASSERT_CF_OF %r12, (OF_BIT|CF_BIT), 12


        /* ---------------------------------------------------------
         * Memory operand sanity
         * --------------------------------------------------------- */

        /* T13: incb (mem) 0x7F->0x80 sets OF, CF=1 preserved */
        stc
        movb    $0x7f, mem8(%rip)
        incb    mem8(%rip)
        SNAP_RFLAGS %r12
        movb    mem8(%rip), %al
        ASSERT_EQ8   %al, 0x80, 13
        ASSERT_CF_OF %r12, (OF_BIT|CF_BIT), 13

        /* T14: decb (mem) 0x80->0x7F sets OF, CF=0 preserved */
        clc
        movb    $0x80, mem8(%rip)
        decb    mem8(%rip)
        SNAP_RFLAGS %r12
        movb    mem8(%rip), %al
        ASSERT_EQ8   %al, 0x7f, 14
        ASSERT_CF_OF %r12, OF_BIT, 14


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


        .section .bss
        .balign 8
mem8:   .skip 1

        .section .rodata
success_msg:
        .ascii  "success\n"
        