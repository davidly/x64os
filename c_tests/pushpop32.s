/* pushpop32.s -- 32-bit Linux (i386) GAS tests: push/pop forms
 *
 * Build:
 *   as --32 -o pushpop32.o pushpop32.s
 *   ld -m elf_i386 -o pushpop32 pushpop32.o
 *
 * Run:
 *   ./pushpop32
 *   echo $?
 */

        .section .text
        .code32
        .globl _start

        .set SYS_exit,  1
        .set SYS_write, 4

/* ---------- helpers ---------- */
.macro FAIL code
        movl    $SYS_exit, %eax
        movl    $\code, %ebx
        int     $0x80
.endm

.macro ASSERT_EQ_IMM reg, imm, code
        cmpl    $\imm, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_REG_EQ r1, r2, code
        cmpl    \r2, \r1
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_MEM32_IMM sym, imm, code
        movl    \sym, %edx
        cmpl    $\imm, %edx
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro SAVE_ESP reg
        movl    %esp, \reg
.endm


_start:
        /* Use a private stack buffer (avoid relying on initial process stack) */
        lea     stack_space_end, %esp

        /* =========================================================
         * T01: push/pop r32 roundtrip + ESP delta
         * ========================================================= */
        SAVE_ESP %esi
        movl    $0xAABBCCDD, %eax
        pushl   %eax
        SAVE_ESP %edi
        leal    -4(%esi), %ecx
        ASSERT_REG_EQ %edi, %ecx, 1

        xorl    %eax, %eax
        popl    %eax
        SAVE_ESP %edi
        ASSERT_EQ_IMM %eax, 0xAABBCCDD, 1
        ASSERT_REG_EQ %edi, %esi, 1

        /* =========================================================
         * T02: push imm8 is sign-extended
         * Force imm8 encoding by using a negative constant.
         * ========================================================= */
        SAVE_ESP %esi
        pushl   $-1                 /* should assemble as 6A FF */
        popl    %eax
        ASSERT_EQ_IMM %eax, 0xFFFFFFFF, 2
        SAVE_ESP %edi
        ASSERT_REG_EQ %edi, %esi, 2

        /* =========================================================
         * T03: push imm32 exact (0x80000000)
         * ========================================================= */
        SAVE_ESP %esi
        pushl   $0x80000000
        popl    %eax
        ASSERT_EQ_IMM %eax, 0x80000000, 3
        SAVE_ESP %edi
        ASSERT_REG_EQ %edi, %esi, 3

        /* =========================================================
         * T04: push r/m32 (memory) and pop r/m32
         * ========================================================= */
        movl    $0x12345678, memword
        SAVE_ESP %esi
        pushl   memword
        popl    %eax
        ASSERT_EQ_IMM %eax, 0x12345678, 4
        SAVE_ESP %edi
        ASSERT_REG_EQ %edi, %esi, 4

        movl    $0, memword
        pushl   $0xDEADBEEF
        popl    memword
        ASSERT_MEM32_IMM memword, 0xDEADBEEF, 4

        /* =========================================================
         * T05: push %esp pushes original ESP (before decrement)
         * ========================================================= */
        lea     stack_space_end, %esp
        SAVE_ESP %esi                /* original */
        pushl   %esp
        movl    (%esp), %eax         /* top contains original esp */
        ASSERT_REG_EQ %eax, %esi, 5
        popl    %eax                 /* clean stack */

        /* =========================================================
         * T06: pop %esp loads new ESP from stack
         * ========================================================= */
        lea     stack_space_end, %esp
        lea     stack_space_end-64, %eax
        pushl   %eax                 /* value to load into esp */
        popl    %esp                 /* esp becomes that value */
        SAVE_ESP %ebx
        ASSERT_REG_EQ %ebx, %eax, 6
        lea     stack_space_end, %esp

        /* =========================================================
         * T07: pusha/popa (popa ignores stored ESP slot)
         * ========================================================= */
        lea     stack_space_end, %esp
        movl    $0x01020304, %eax
        movl    $0x11121314, %ecx
        movl    $0x21222324, %edx
        movl    $0x31323334, %ebx
        movl    $0x51525354, %ebp
        movl    $0x61626364, %esi
        movl    $0x71727374, %edi

        pusha
        xorl    %eax, %eax
        xorl    %ebx, %ebx
        xorl    %ecx, %ecx
        xorl    %edx, %edx
        xorl    %ebp, %ebp
        xorl    %esi, %esi
        xorl    %edi, %edi
        popa

        ASSERT_EQ_IMM %eax, 0x01020304, 7
        ASSERT_EQ_IMM %ecx, 0x11121314, 7
        ASSERT_EQ_IMM %edx, 0x21222324, 7
        ASSERT_EQ_IMM %ebx, 0x31323334, 7
        ASSERT_EQ_IMM %ebp, 0x51525354, 7
        ASSERT_EQ_IMM %esi, 0x61626364, 7
        ASSERT_EQ_IMM %edi, 0x71727374, 7

        /* =========================================================
         * T08: pushf/popf sanity:
         *  - verify CF reflected by pushf
         *  - verify popf can restore CF from stack image
         * ========================================================= */
        /* CF=0 reflected */
        clc
        pushfl
        popl    %eax
        testl   $1, %eax
        jz      t08_ok0
        FAIL 8
t08_ok0:

        /* CF=1 reflected */
        stc
        pushfl
        popl    %eax
        testl   $1, %eax
        jnz     t08_ok1
        FAIL 8
t08_ok1:

        /* popf restores CF=1 */
        clc
        pushfl
        popl    %eax
        orl     $1, %eax
        pushl   %eax
        popfl
        pushfl
        popl    %eax
        testl   $1, %eax
        jnz     t08_ok2
        FAIL 8
t08_ok2:

        /* =========================================================
         * success
         * ========================================================= */
        movl    $SYS_write, %eax
        movl    $1, %ebx
        movl    $success_msg, %ecx
        movl    $8, %edx
        int     $0x80

        movl    $SYS_exit, %eax
        xorl    %ebx, %ebx
        int     $0x80


        .section .bss
        .balign 16
stack_space:
        .skip  4096
stack_space_end:

        .section .data
        .balign 4
memword:
        .long 0

        .section .rodata
success_msg:
        .ascii "success\n"
