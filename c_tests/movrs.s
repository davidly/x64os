/* movrs_movdir_cmpccxadd64.s -- x86-64 Linux GAS regression tests
 *
 * Tests MOVRS, MOVDIRI, MOVDIR64B, and all 16 CMPccXADD conditions.
 *
 * Build:
 *   as -o movrs_movdir_cmpccxadd64.o movrs_movdir_cmpccxadd64.s
 *   ld -o movrs_movdir_cmpccxadd64 movrs_movdir_cmpccxadd64.o
 *
 * Run under the emulator:
 *   ./movrs_movdir_cmpccxadd64
 *   echo $?
 *
 * A successful run prints "success" and exits with status 0. A failure exits
 * with the test number. CMPccXADD is emitted as raw bytes because GNU as 2.44
 * does not yet recognize its mnemonics.
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

        /* Feature bits kept in R12D. */
        .set HAVE_MOVRS,      0x01
        .set HAVE_MOVDIRI,    0x02
        .set HAVE_MOVDIR64B,  0x04
        .set HAVE_CMPCCXADD,  0x08

/* ---------- helpers ---------- */
.macro FAIL code
        mov     $SYS_exit, %rax
        mov     $\code, %rdi
        syscall
.endm

/* cmpq $imm64,%reg is not generally encodable. */
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

.macro ASSERT_MEM64 symbol, imm64, code
        movq    \symbol(%rip), %r10
        ASSERT_EQ64 %r10, \imm64, \code
.endm

.macro ASSERT_MEM32 symbol, imm32, code
        movl    \symbol(%rip), %r10d
        ASSERT_EQ32 %r10d, \imm32, \code
.endm

/*
 * CMPccXADD fixed-register encodings used below:
 *
 *   memory operand:  (%rbx)
 *   compare/result:  %eax or %rax       (ModRM.reg = 0)
 *   addend:          %ecx or %rcx       (VEX.vvvv = 1)
 *
 * VEX.128.66.0F38.W0/W1 E0-EF /r, ModRM = 00 000 011b.
 */
.macro CMPCCXADD32 opcode
        .byte   0xc4, 0xe2, 0x71, \opcode, 0x03
.endm

.macro CMPCCXADD64 opcode
        .byte   0xc4, 0xe2, 0xf1, \opcode, 0x03
.endm

/* Run one 32-bit case and verify both outputs.
 * On completion EAX must contain the original memory value.
 */
.macro TEST_CMP32 opcode, memory, compare, addend, expected, code
        lea     cmp_value(%rip), %rbx
        movl    $\memory, (%rbx)
        movl    $\compare, %eax
        movl    $\addend, %ecx
        CMPCCXADD32 \opcode
        ASSERT_EQ32 %eax, \memory, \code
        movl    (%rbx), %r10d
        ASSERT_EQ32 %r10d, \expected, \code
.endm

_start:
        /* ---------------------------------------------------------
         * Detect each instruction family before executing it.
         *
         * CPUID.07H.0:ECX[27] = MOVDIRI
         * CPUID.07H.0:ECX[28] = MOVDIR64B
         * CPUID.07H.1:EAX[7]  = CMPCCXADD
         * CPUID.07H.1:EAX[31] = MOVRS
         * --------------------------------------------------------- */
        xorl    %r12d, %r12d

        xorl    %eax, %eax
        cpuid
        cmpl    $7, %eax
        jb      features_done

        movl    $7, %eax
        xorl    %ecx, %ecx
        cpuid
        btl     $27, %ecx
        jnc     1f
        orl     $HAVE_MOVDIRI, %r12d
1:
        btl     $28, %ecx
        jnc     2f
        orl     $HAVE_MOVDIR64B, %r12d
2:
        /* EAX from sub-leaf 0 is the maximum valid sub-leaf. */
        testl   %eax, %eax
        jz      features_done

        movl    $7, %eax
        movl    $1, %ecx
        cpuid
        btl     $7, %eax
        jnc     3f
        orl     $HAVE_CMPCCXADD, %r12d
3:
        btl     $31, %eax
        jnc     features_done
        orl     $HAVE_MOVRS, %r12d

features_done:
        testl   $HAVE_MOVRS, %r12d
        jz      movrs_skipped

        /* ---------------------------------------------------------
         * MOVRS: byte, word, dword, qword memory loads
         * --------------------------------------------------------- */

        /* T01: byte load, preserving the rest of the destination register. */
        movabs  $0x1122334455667788, %rax
        /* 0F 38 8A /r: MOVRS r8,m8 (raw bytes for older GAS). */
        .byte   0x0f, 0x38, 0x8a, 0x05
        .long   movrs_byte - 1f
1:
        ASSERT_EQ64 %rax, 0x11223344556677a5, 1

        /* T02: word load, preserving bits 63:16. */
        movabs  $0x1122334455667788, %rax
        /* 66 0F 38 8B /r: MOVRS r16,m16. */
        .byte   0x66, 0x0f, 0x38, 0x8b, 0x05
        .long   movrs_word - 1f
1:
        ASSERT_EQ64 %rax, 0x112233445566b6c7, 2

        /* T03: dword load must zero-extend into the 64-bit register. */
        movabs  $0xffffffffffffffff, %rax
        /* 0F 38 8B /r: MOVRS r32,m32. */
        .byte   0x0f, 0x38, 0x8b, 0x05
        .long   movrs_dword - 1f
1:
        ASSERT_EQ64 %rax, 0x0000000089abcdef, 3

        /* T04: qword load. */
        /* REX.W + 0F 38 8B /r: MOVRS r64,m64. */
        .byte   0x48, 0x0f, 0x38, 0x8b, 0x05
        .long   movrs_qword - 1f
1:
        ASSERT_EQ64 %rax, 0x0123456789abcdef, 4

        /* T05: indexed memory addressing and an extended destination. */
        lea     movrs_array(%rip), %rsi
        mov     $1, %rdx
        /* MOVRS r9,qword ptr [rsi+rdx*8]. */
        .byte   0x4c, 0x0f, 0x38, 0x8b, 0x0c, 0xd6
        ASSERT_EQ64 %r9, 0xfedcba9876543210, 5
        jmp     movrs_done

movrs_skipped:
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     skip_movrs_msg(%rip), %rsi
        mov     $skip_movrs_len, %rdx
        syscall
movrs_done:

        testl   $HAVE_MOVDIRI, %r12d
        jz      movdiri_skipped

        /* ---------------------------------------------------------
         * MOVDIRI: dword and qword direct stores
         * --------------------------------------------------------- */

        /* T06: dword store. */
        movl    $0x89abcdef, %r8d
        /* 0F 38 F9 /r: MOVDIRI m32,r32. */
        .byte   0x44, 0x0f, 0x38, 0xf9, 0x05
        .long   movdiri_dword - 1f
1:
        ASSERT_MEM32 movdiri_dword, 0x89abcdef, 6

        /* T07: qword store. */
        movabs  $0x0123456789abcdef, %r9
        /* REX.W + 0F 38 F9 /r: MOVDIRI m64,r64. */
        .byte   0x4c, 0x0f, 0x38, 0xf9, 0x0d
        .long   movdiri_qword - 1f
1:
        ASSERT_MEM64 movdiri_qword, 0x0123456789abcdef, 7

        /* T08: base-plus-displacement destination. */
        lea     movdiri_array(%rip), %rdi
        movabs  $0xfedcba9876543210, %r10
        /* MOVDIRI qword ptr [rdi+8],r10. */
        .byte   0x4c, 0x0f, 0x38, 0xf9, 0x57, 0x08
        movq    8(%rdi), %r10
        ASSERT_EQ64 %r10, 0xfedcba9876543210, 8
        jmp     movdiri_done

movdiri_skipped:
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     skip_movdiri_msg(%rip), %rsi
        mov     $skip_movdiri_len, %rdx
        syscall
movdiri_done:

        testl   $HAVE_MOVDIR64B, %r12d
        jz      movdir64b_skipped

        /* ---------------------------------------------------------
         * MOVDIR64B: copy exactly 64 bytes from memory to the address
         * held in the register operand.
         * --------------------------------------------------------- */

        /* T09: copy and compare all eight qwords. */
        lea     movdir64b_source(%rip), %rax
        lea     movdir64b_dest(%rip), %rcx
        /* 66 0F 38 F8 /r: MOVDIR64B rcx,[rax]. */
        .byte   0x66, 0x0f, 0x38, 0xf8, 0x08

        lea     movdir64b_source(%rip), %rsi
        lea     movdir64b_dest(%rip), %rdi
        mov     $8, %ecx
t9_loop:
        movq    (%rsi), %rax
        cmpq    (%rdi), %rax
        jne     t9_fail
        add     $8, %rsi
        add     $8, %rdi
        dec     %ecx
        jne     t9_loop
        jmp     t9_done
t9_fail:
        FAIL 9
t9_done:
        jmp     movdir64b_done

movdir64b_skipped:
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     skip_movdir64b_msg(%rip), %rsi
        mov     $skip_movdir64b_len, %rdx
        syscall
movdir64b_done:

        testl   $HAVE_CMPCCXADD, %r12d
        jz      cmpccxadd_skipped

        /* ---------------------------------------------------------
         * CMPccXADD, 32-bit form
         *
         * Each condition is tested once true and once false. The
         * addend is 7. A true condition changes memory to old+7;
         * a false condition leaves memory unchanged. EAX always
         * receives the original memory value.
         * --------------------------------------------------------- */

        /* O / NO */
        TEST_CMP32 0xe0, 0x80000000, 1, 7, 0x80000007, 10
        TEST_CMP32 0xe0, 5,          1, 7, 5,          11
        TEST_CMP32 0xe1, 5,          1, 7, 12,         12
        TEST_CMP32 0xe1, 0x80000000, 1, 7, 0x80000000, 13

        /* B / AE */
        TEST_CMP32 0xe2, 1, 2, 7, 8, 14
        TEST_CMP32 0xe2, 2, 1, 7, 2, 15
        TEST_CMP32 0xe3, 2, 1, 7, 9, 16
        TEST_CMP32 0xe3, 1, 2, 7, 1, 17

        /* E / NE */
        TEST_CMP32 0xe4, 5, 5, 7, 12, 18
        TEST_CMP32 0xe4, 5, 4, 7, 5,  19
        TEST_CMP32 0xe5, 5, 4, 7, 12, 20
        TEST_CMP32 0xe5, 5, 5, 7, 5,  21

        /* BE / A */
        TEST_CMP32 0xe6, 1, 2, 7, 8, 22
        TEST_CMP32 0xe6, 3, 2, 7, 3, 23
        TEST_CMP32 0xe7, 3, 2, 7, 10, 24
        TEST_CMP32 0xe7, 1, 2, 7, 1,  25

        /* S / NS */
        TEST_CMP32 0xe8, 1, 2, 7, 8, 26
        TEST_CMP32 0xe8, 2, 1, 7, 2, 27
        TEST_CMP32 0xe9, 2, 1, 7, 9, 28
        TEST_CMP32 0xe9, 1, 2, 7, 1, 29

        /* P / NP */
        TEST_CMP32 0xea, 5, 5, 7, 12, 30
        TEST_CMP32 0xea, 2, 1, 7, 2,  31
        TEST_CMP32 0xeb, 2, 1, 7, 9,  32
        TEST_CMP32 0xeb, 5, 5, 7, 5,  33

        /* L / GE */
        TEST_CMP32 0xec, 0x80000000, 0, 7, 0x80000007, 34
        TEST_CMP32 0xec, 1,          0, 7, 1,          35
        TEST_CMP32 0xed, 1,          0, 7, 8,          36
        TEST_CMP32 0xed, 0x80000000, 0, 7, 0x80000000, 37

        /* LE / G */
        TEST_CMP32 0xee, 0xffffffff, 0, 7, 6,          38
        TEST_CMP32 0xee, 1,          0, 7, 1,          39
        TEST_CMP32 0xef, 1,          0, 7, 8,          40
        TEST_CMP32 0xef, 0xffffffff, 0, 7, 0xffffffff, 41

        /* ---------------------------------------------------------
         * CMPccXADD, 64-bit form
         * --------------------------------------------------------- */

        /* T42: CMPEXADDQ true; verifies a full-width add and result. */
        lea     cmp_value(%rip), %rbx
        movabs  $0x1000000000000005, %r10
        movq    %r10, (%rbx)
        movabs  $0x1000000000000005, %rax
        movabs  $0x2000000000000007, %rcx
        CMPCCXADD64 0xe4
        ASSERT_EQ64 %rax, 0x1000000000000005, 42
        movq    (%rbx), %r10
        ASSERT_EQ64 %r10, 0x300000000000000c, 42

        /* T43: CMPGXADDQ false; memory unchanged, RAX gets old memory. */
        movabs  $0xffffffffffffffff, %r10
        movq    %r10, (%rbx)
        xorq    %rax, %rax
        movabs  $0x1111111111111111, %rcx
        CMPCCXADD64 0xef
        ASSERT_EQ64 %rax, 0xffffffffffffffff, 43
        movq    (%rbx), %r10
        ASSERT_EQ64 %r10, 0xffffffffffffffff, 43
        jmp     cmpccxadd_done

cmpccxadd_skipped:
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     skip_cmpccxadd_msg(%rip), %rsi
        mov     $skip_cmpccxadd_len, %rdx
        syscall
cmpccxadd_done:

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


        .section .data
        .balign 8
movrs_byte:
        .byte   0xa5
        .balign 2
movrs_word:
        .word   0xb6c7
        .balign 4
movrs_dword:
        .long   0x89abcdef
        .balign 8
movrs_qword:
        .quad   0x0123456789abcdef
movrs_array:
        .quad   0x1111111111111111
        .quad   0xfedcba9876543210

        .balign 8
movdir64b_source:
        .quad   0x0011223344556677
        .quad   0x8899aabbccddeeff
        .quad   0x0123456789abcdef
        .quad   0xfedcba9876543210
        .quad   0x0f1e2d3c4b5a6978
        .quad   0x8070605040302010
        .quad   0xaaaaaaaa55555555
        .quad   0x13579bdf2468ace0

        .section .bss
        .balign 64
movdir64b_dest:
        .skip   64

        .balign 8
movdiri_dword:
        .skip   4
        .balign 8
movdiri_qword:
        .skip   8
movdiri_array:
        .skip   16
cmp_value:
        .skip   8

        .section .rodata
skip_movrs_msg:
        .ascii  "skip: MOVRS unsupported\n"
skip_movrs_len = . - skip_movrs_msg
skip_movdiri_msg:
        .ascii  "skip: MOVDIRI unsupported\n"
skip_movdiri_len = . - skip_movdiri_msg
skip_movdir64b_msg:
        .ascii  "skip: MOVDIR64B unsupported\n"
skip_movdir64b_len = . - skip_movdir64b_msg
skip_cmpccxadd_msg:
        .ascii  "skip: CMPCCXADD unsupported\n"
skip_cmpccxadd_len = . - skip_cmpccxadd_msg
success_msg:
        .ascii  "success\n"
