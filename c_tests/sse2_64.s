/* sse2_pure_0f2a_2c_53.s -- x86-64 Linux GAS tests for SSE/SSE2-only forms of:
 *   0F 2A : CVTSI2SS / CVTSI2SD
 *   0F 2C : CVTTSS2SI / CVTTSD2SI
 *   0F 53 : RCPPS / RCPSS
 *
 * No MMX, no EMMS.
 * Prints "success\n" on pass; exits with test-id on failure.
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

.macro FAIL code
        mov     $SYS_exit, %rax
        mov     $\code, %rdi
        syscall
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

_start:
        /* -------------------------------------------------
         * T01: cvtsi2ss r32 -> xmm
         * low 32 becomes float, upper 96 preserved
         * ------------------------------------------------- */
        movaps  sentinel_ps(%rip), %xmm0
        mov     $1234567, %eax               /* exactly representable in float */
        cvtsi2ss %eax, %xmm0
        movaps  %xmm0, tmp128(%rip)

        movl    tmp128+0(%rip), %eax
        ASSERT_EQ32 %eax, 0x4996b438, 1      /* 1234567.0f */
        movl    tmp128+4(%rip), %eax
        ASSERT_EQ32 %eax, 0x99aabbcc, 1
        movl    tmp128+8(%rip), %eax
        ASSERT_EQ32 %eax, 0x11223344, 1
        movl    tmp128+12(%rip), %eax
        ASSERT_EQ32 %eax, 0x55667788, 1

        /* -------------------------------------------------
         * T02: cvtsi2ss r64 -> xmm
         * 2^30 -> exact float 0x4e800000
         * ------------------------------------------------- */
        pxor    %xmm1, %xmm1
        mov     $1073741824, %rax            /* 2^30 */
        cvtsi2ss %rax, %xmm1
        movd    %xmm1, %eax
        ASSERT_EQ32 %eax, 0x4e800000, 2

        /* -------------------------------------------------
         * T03: cvtsi2ss m32 -> xmm
         * mem32 42 -> 42.0f = 0x42280000
         * upper lanes preserved
         * ------------------------------------------------- */
        movaps  sentinel_ps(%rip), %xmm2
        cvtsi2ss int32_42(%rip), %xmm2
        movaps  %xmm2, tmp128(%rip)

        movl    tmp128+0(%rip), %eax
        ASSERT_EQ32 %eax, 0x42280000, 3
        movl    tmp128+4(%rip), %eax
        ASSERT_EQ32 %eax, 0x99aabbcc, 3
        movl    tmp128+8(%rip), %eax
        ASSERT_EQ32 %eax, 0x11223344, 3
        movl    tmp128+12(%rip), %eax
        ASSERT_EQ32 %eax, 0x55667788, 3

        /* -------------------------------------------------
         * T04: cvtsi2sd r32 -> xmm
         * low 64 becomes double, upper 64 preserved
         * ------------------------------------------------- */
        movapd  sentinel_pd(%rip), %xmm3
        mov     $-123456789, %eax
        cvtsi2sd %eax, %xmm3
        movapd  %xmm3, tmp128(%rip)

        movq    tmp128+0(%rip), %rax
        ASSERT_EQ64 %rax, 0xc19d6f3454000000, 4
        movq    tmp128+8(%rip), %rax
        ASSERT_EQ64 %rax, 0x1122334455667788, 4

        /* -------------------------------------------------
         * T05: cvtsi2sd r64 -> xmm
         * 2^40 -> exact double 0x4270000000000000
         * ------------------------------------------------- */
        xorpd   %xmm4, %xmm4
        movabs  $1099511627776, %rax         /* 2^40 */
        cvtsi2sd %rax, %xmm4
        movq    %xmm4, %rax
        ASSERT_EQ64 %rax, 0x4270000000000000, 5

        /* -------------------------------------------------
         * T06: cvtsi2sd m64 -> xmm
         * mem64 -42 -> -42.0 double = 0xc045000000000000
         * upper 64 preserved
         * ------------------------------------------------- */
        movapd  sentinel_pd(%rip), %xmm5
        cvtsi2sd int64_neg42(%rip), %xmm5
        movapd  %xmm5, tmp128(%rip)

        movq    tmp128+0(%rip), %rax
        ASSERT_EQ64 %rax, 0xc045000000000000, 6
        movq    tmp128+8(%rip), %rax
        ASSERT_EQ64 %rax, 0x1122334455667788, 6

        /* -------------------------------------------------
         * T07: cvttss2si xmm -> r32
         * 3.75f -> 3
         * ------------------------------------------------- */
        movss   f_3_75(%rip), %xmm6
        cvttss2si %xmm6, %eax
        ASSERT_EQ32 %eax, 3, 7

        /* -------------------------------------------------
         * T08: cvttss2si xmm -> r64
         * -2.9f -> -2
         * ------------------------------------------------- */
        movss   f_neg_2_9(%rip), %xmm6
        cvttss2si %xmm6, %rax
        ASSERT_EQ64 %rax, -2, 8

        /* -------------------------------------------------
         * T09: cvttss2si m32 -> r32
         * mem float 42.99 -> 42
         * ------------------------------------------------- */
        cvttss2si f_42_99(%rip), %eax
        ASSERT_EQ32 %eax, 42, 9

        /* -------------------------------------------------
         * T10: cvttsd2si xmm -> r32
         * 3.75 -> 3
         * ------------------------------------------------- */
        movsd   d_3_75(%rip), %xmm7
        cvttsd2si %xmm7, %eax
        ASSERT_EQ32 %eax, 3, 10

        /* -------------------------------------------------
         * T11: cvttsd2si xmm -> r64
         * -2.9 -> -2
         * ------------------------------------------------- */
        movsd   d_neg_2_9(%rip), %xmm7
        cvttsd2si %xmm7, %rax
        ASSERT_EQ64 %rax, -2, 11

        /* -------------------------------------------------
         * T12: cvttsd2si m64 -> r64
         * mem double 42.99 -> 42
         * ------------------------------------------------- */
        cvttsd2si d_42_99(%rip), %rax
        ASSERT_EQ64 %rax, 42, 12

        /* -------------------------------------------------
         * T13: rcpps reg -> xmm
         * Check abs(error) <= tolerance on all 4 lanes
         * inputs: [1,2,4,8], expected [1,.5,.25,.125]
         * ------------------------------------------------- */
        movaps  rcp_inputs(%rip), %xmm0
        rcpps   %xmm0, %xmm0
        movaps  %xmm0, %xmm1
        subps   rcp_expected(%rip), %xmm1
        andps   abs_mask_ps(%rip), %xmm1
        cmpleps rcp_tol(%rip), %xmm1
        movmskps %xmm1, %eax
        ASSERT_EQ32 %eax, 0xF, 13

        /* -------------------------------------------------
         * T14: rcpps mem -> xmm
         * Same check with memory source
         * ------------------------------------------------- */
        rcpps   rcp_inputs(%rip), %xmm2
        movaps  %xmm2, %xmm3
        subps   rcp_expected(%rip), %xmm3
        andps   abs_mask_ps(%rip), %xmm3
        cmpleps rcp_tol(%rip), %xmm3
        movmskps %xmm3, %eax
        ASSERT_EQ32 %eax, 0xF, 14

        /* -------------------------------------------------
         * T15: rcpss reg -> xmm
         * low lane approx reciprocal of 2.0 => ~0.5
         * upper 96 bits preserved
         * ------------------------------------------------- */
        movaps  rcpss_src(%rip), %xmm4
        rcpss   %xmm4, %xmm4
        movaps  %xmm4, tmp128(%rip)

        movss   tmp128(%rip), %xmm5
        subss   f_0_5(%rip), %xmm5
        andps   abs_mask_ps(%rip), %xmm5
        ucomiss f_tol(%rip), %xmm5
        jbe     1f
        FAIL 15
1:
        movl    tmp128+4(%rip), %eax
        ASSERT_EQ32 %eax, 0x99aabbcc, 15
        movl    tmp128+8(%rip), %eax
        ASSERT_EQ32 %eax, 0x11223344, 15
        movl    tmp128+12(%rip), %eax
        ASSERT_EQ32 %eax, 0x55667788, 15

        /* -------------------------------------------------
         * T16: rcpss mem -> xmm
         * low lane approx reciprocal of 4.0 => ~0.25
         * upper 96 bits preserved from destination
         * ------------------------------------------------- */
        movaps  sentinel_ps(%rip), %xmm6
        rcpss   f_4_0(%rip), %xmm6
        movaps  %xmm6, tmp128(%rip)

        movss   tmp128(%rip), %xmm7
        subss   f_0_25(%rip), %xmm7
        andps   abs_mask_ps(%rip), %xmm7
        ucomiss f_tol(%rip), %xmm7
        jbe     2f
        FAIL 16
2:
        movl    tmp128+4(%rip), %eax
        ASSERT_EQ32 %eax, 0x99aabbcc, 16
        movl    tmp128+8(%rip), %eax
        ASSERT_EQ32 %eax, 0x11223344, 16
        movl    tmp128+12(%rip), %eax
        ASSERT_EQ32 %eax, 0x55667788, 16

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
tmp128: .skip 16

        .section .rodata
        .balign 16
success_msg:
        .ascii "success\n"

        .balign 16
sentinel_ps:
        .long 0xdeadbeef, 0x99aabbcc, 0x11223344, 0x55667788

        .balign 16
sentinel_pd:
        .quad 0xdeadbeefcafebabe, 0x1122334455667788

        .balign 4
int32_42:
        .long 42

        .balign 8
int64_neg42:
        .quad -42

        .balign 4
f_3_75:
        .long 0x40700000
f_neg_2_9:
        .long 0xc039999a
f_42_99:
        .long 0x422bf5c3              /* ~42.99f */
f_0_5:
        .long 0x3f000000
f_0_25:
        .long 0x3e800000
f_4_0:
        .long 0x40800000
f_tol:
        .long 0x3a83126f              /* about 0.001f */

        .balign 8
d_3_75:
        .quad 0x400e000000000000
d_neg_2_9:
        .quad 0xc007333333333333
d_42_99:
        .quad 0x40457eb851eb851f      /* ~42.99 */

        .balign 16
rcp_inputs:
        .long 0x3f800000, 0x40000000, 0x40800000, 0x41000000 /* 1,2,4,8 */

        .balign 16
rcp_expected:
        .long 0x3f800000, 0x3f000000, 0x3e800000, 0x3e000000 /* 1,.5,.25,.125 */

        .balign 16
rcp_tol:
        .long 0x3a83126f, 0x3a83126f, 0x3a83126f, 0x3a83126f

        .balign 16
abs_mask_ps:
        .long 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff

        .balign 16
rcpss_src:
        .long 0x40000000, 0x99aabbcc, 0x11223344, 0x55667788 /* low=2.0f */
        