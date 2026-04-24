.intel_syntax noprefix
.global _start

.section .text

_start:
    fninit

    /* Save original control word */
    fnstcw word ptr [orig_cw]

    call test_roundtrip
    call test_stack_pop
    call test_rounding

    /* success */
    mov eax, 1                  /* write */
    mov edi, 1
    lea rsi, success_msg[rip]
    mov edx, 8
    syscall

    mov eax, 60                 /* exit */
    xor edi, edi
    syscall


/* ------------------------------------------------------------
 * Test 1:
 *   exact 64-bit round-trip through:
 *       fild qword [src]
 *       fistp qword [dst]
 *
 * This should preserve every 64-bit integer bit pattern exactly.
 * ------------------------------------------------------------ */
test_roundtrip:
    lea rsi, patterns[rip]
    lea rcx, patterns_end[rip]
.rt_loop:
    cmp rsi, rcx
    je .rt_done

    mov rax, qword ptr [rsi]
    mov qword ptr [src_q], rax

    fild qword ptr [src_q]
    fistp qword ptr [dst_q]

    mov rax, qword ptr [src_q]
    mov rdx, qword ptr [dst_q]
    cmp rax, rdx
    jne fail_1

    add rsi, 8
    jmp .rt_loop

.rt_done:
    ret


/* ------------------------------------------------------------
 * Test 2:
 *   x87 stack push/pop behavior.
 *
 * Check that TOP in the x87 status word is the same before and
 * after a fild + fistp pair.
 * ------------------------------------------------------------ */
test_stack_pop:
    fnstsw ax
    and eax, 0x3800
    mov dword ptr [top_before], eax

    mov qword ptr [src_q], 123456789
    fild qword ptr [src_q]
    fistp qword ptr [dst_q]

    fnstsw ax
    and eax, 0x3800
    mov dword ptr [top_after], eax

    mov eax, dword ptr [top_before]
    cmp eax, dword ptr [top_after]
    jne fail_2

    ret


/* ------------------------------------------------------------
 * Test 3:
 *   FISTP obeys x87 rounding mode.
 *
 * Uses exact halfway values to distinguish "nearest-even"
 * from other modes.
 * ------------------------------------------------------------ */
test_rounding:
    /* nearest-even */
    mov ax, 0x037f
    mov word ptr [tmp_cw], ax
    fldcw word ptr [tmp_cw]

    fld qword ptr [d_pos_2_5]
    fistp qword ptr [dst_q]
    mov rax, qword ptr [dst_q]
    cmp rax, 2
    jne fail_3

    fld qword ptr [d_neg_2_5]
    fistp qword ptr [dst_q]
    mov rax, qword ptr [dst_q]
    cmp rax, -2
    jne fail_3

    /* round down (toward -inf) */
    mov ax, 0x077f
    mov word ptr [tmp_cw], ax
    fldcw word ptr [tmp_cw]

    fld qword ptr [d_pos_1_5]
    fistp qword ptr [dst_q]
    mov rax, qword ptr [dst_q]
    cmp rax, 1
    jne fail_3

    fld qword ptr [d_neg_1_5]
    fistp qword ptr [dst_q]
    mov rax, qword ptr [dst_q]
    cmp rax, -2
    jne fail_3

    /* round up (toward +inf) */
    mov ax, 0x0b7f
    mov word ptr [tmp_cw], ax
    fldcw word ptr [tmp_cw]

    fld qword ptr [d_pos_1_5]
    fistp qword ptr [dst_q]
    mov rax, qword ptr [dst_q]
    cmp rax, 2
    jne fail_3

    fld qword ptr [d_neg_1_5]
    fistp qword ptr [dst_q]
    mov rax, qword ptr [dst_q]
    cmp rax, -1
    jne fail_3

    /* truncate toward zero */
    mov ax, 0x0f7f
    mov word ptr [tmp_cw], ax
    fldcw word ptr [tmp_cw]

    fld qword ptr [d_pos_1_5]
    fistp qword ptr [dst_q]
    mov rax, qword ptr [dst_q]
    cmp rax, 1
    jne fail_3

    fld qword ptr [d_neg_1_5]
    fistp qword ptr [dst_q]
    mov rax, qword ptr [dst_q]
    cmp rax, -1
    jne fail_3

    /* restore original CW */
    fldcw word ptr [orig_cw]
    ret


fail_1:
    fldcw word ptr [orig_cw]
    mov eax, 60
    mov edi, 1
    syscall

fail_2:
    fldcw word ptr [orig_cw]
    mov eax, 60
    mov edi, 2
    syscall

fail_3:
    fldcw word ptr [orig_cw]
    mov eax, 60
    mov edi, 3
    syscall


.section .rodata
success_msg:
    .ascii "success\n"

/* exact round-trip patterns */
patterns:
    .quad 0x0000000000000000
    .quad 0x0000000000000001
    .quad 0xffffffffffffffff
    .quad 0x8000000000000000
    .quad 0x7fffffffffffffff
    .quad 0x0000006f6c6c6548      /* "Hello" little-endian in low bytes */
    .quad 0x1122334455667788
    .quad 0x8877665544332211
    .quad 0x00ff00ff00ff00ff
    .quad 0xff00ff00ff00ff00
    .quad 0xaaaaaaaaaaaaaaaa
    .quad 0x5555555555555555
    .quad 0x0123456789abcdef
    .quad 0xfedcba9876543210
patterns_end:

/* doubles for rounding-mode tests */
d_pos_1_5: .quad 0x3ff8000000000000
d_neg_1_5: .quad 0xbff8000000000000
d_pos_2_5: .quad 0x4004000000000000
d_neg_2_5: .quad 0xc004000000000000

.section .bss
.align 8
src_q:       .skip 8
dst_q:       .skip 8
top_before:  .skip 4
top_after:   .skip 4
orig_cw:     .skip 2
tmp_cw:      .skip 2
