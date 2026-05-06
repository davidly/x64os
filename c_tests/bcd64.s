/* x87_bcd64.s -- amd64 Linux GAS tests for FBLD m80bcd / FBSTP m80bcd
 *
 * Build:
 *   as -o x87_bcd64.o x87_bcd64.s
 *   ld -o x87_bcd64 x87_bcd64.o
 *
 * Run:
 *   ./x87_bcd64
 *   echo $?
 */

.intel_syntax noprefix
.global _start

.section .text

.set SYS_write, 1
.set SYS_exit,  60

.macro FAIL code
    mov eax, SYS_exit
    mov edi, \code
    syscall
.endm

.macro ASSERT_EQ8_MEM label, imm, code
    movzx eax, byte ptr [rip + \label]
    cmp eax, \imm
    jne 1f
    jmp 2f
1:
    FAIL \code
2:
.endm

.macro ASSERT_EQ16 reg, imm, code
    cmp \reg, \imm
    jne 1f
    jmp 2f
1:
    FAIL \code
2:
.endm

_start:
    fninit

    call test_bcd_roundtrip
    call test_bcd_sign
    call test_bcd_stack_pop
    call test_bcd_integer_value

    mov eax, SYS_write
    mov edi, 1
    lea rsi, [rip + success_msg]
    mov edx, 8
    syscall

    mov eax, SYS_exit
    xor edi, edi
    syscall


/* ------------------------------------------------------------
 * T01-T04: exact BCD roundtrip:
 *   fbld  m80bcd
 *   fbstp m80bcd
 *
 * Each source should reproduce exactly.
 * ------------------------------------------------------------ */
test_bcd_roundtrip:
    lea rsi, [rip + bcd_patterns]
    lea rdi, [rip + bcd_patterns_end]
    mov ebx, 1

.rt_loop:
    cmp rsi, rdi
    je .done

    lea rdx, [rip + bcd_out]
    mov rcx, 10
.clear:
    mov byte ptr [rdx], 0xcc
    inc rdx
    loop .clear

    fbld tbyte ptr [rsi]
    fbstp tbyte ptr [rip + bcd_out]

    mov rcx, 10
    lea rdx, [rip + bcd_out]
    mov r8, rsi
.cmp_loop:
    mov al, byte ptr [r8]
    cmp al, byte ptr [rdx]
    jne .fail
    inc r8
    inc rdx
    loop .cmp_loop

    add rsi, 10
    inc ebx
    jmp .rt_loop

.fail:
    mov eax, SYS_exit
    mov edi, ebx
    syscall

.done:
    ret


/***************************************************************
 * T10-T11: sign nibble handling.
 *
 * 10-byte packed BCD:
 *   bytes 0..8  = 18 decimal digits, two digits per byte
 *   byte 9 bit7 = sign, 0 positive, 1 negative
 ***************************************************************/
test_bcd_sign:
    fbld tbyte ptr [rip + bcd_pos_123]
    fbstp tbyte ptr [rip + bcd_out]
    ASSERT_EQ8_MEM bcd_out+9, 0x00, 10

    fbld tbyte ptr [rip + bcd_neg_123]
    fbstp tbyte ptr [rip + bcd_out]
    ASSERT_EQ8_MEM bcd_out+9, 0x80, 11

    ret


/***************************************************************
 * T20: FBSTP pops x87 stack.
 ***************************************************************/
test_bcd_stack_pop:
    fnstsw ax
    and ax, 0x3800
    mov word ptr [rip + top_before], ax

    fbld tbyte ptr [rip + bcd_pos_123]
    fbstp tbyte ptr [rip + bcd_out]

    fnstsw ax
    and ax, 0x3800
    mov bx, word ptr [rip + top_before]
    cmp ax, bx
    jne 1f
    ret
1:
    FAIL 20


/***************************************************************
 * T30-T32: numeric value sanity.
 *
 * Load BCD, store as integer qword using FISTP.
 * This catches digit-order bugs independent of exact BCD output.
 ***************************************************************/
test_bcd_integer_value:
    fbld tbyte ptr [rip + bcd_pos_123]
    fistp qword ptr [rip + int_out]
    mov rax, qword ptr [rip + int_out]
    cmp rax, 123
    jne 1f

    fbld tbyte ptr [rip + bcd_neg_123]
    fistp qword ptr [rip + int_out]
    mov rax, qword ptr [rip + int_out]
    cmp rax, -123
    jne 2f

    fbld tbyte ptr [rip + bcd_ascii_hello]
    fistp qword ptr [rip + int_out]
    mov rax, qword ptr [rip + int_out]
    movabs r11, 0x0000006f6c6c6548
    cmp rax, r11
    jne 3f

    ret
1:
    FAIL 30
2:
    FAIL 31
3:
    FAIL 32


.section .rodata
success_msg:
    .ascii "success\n"

/*
 * Packed BCD digit order is little-endian decimal:
 * byte0 low nibble = 10^0 digit
 * byte0 high nibble = 10^1 digit
 * ...
 * byte8 high nibble = 10^17 digit
 * byte9 bit7 = sign
 */

/* +0 */
bcd_zero:
    .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00

/* +1 */
bcd_one:
    .byte 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00

/* -1 */
bcd_neg_one:
    .byte 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80

/* +123 */
bcd_pos_123:
    .byte 0x23,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00

/* -123 */
bcd_neg_123:
    .byte 0x23,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80

/* +987654321012345678 */
bcd_big_pos:
    .byte 0x78,0x56,0x34,0x12,0x10,0x32,0x54,0x76,0x98,0x00

/* -987654321012345678 */
bcd_big_neg:
    .byte 0x78,0x56,0x34,0x12,0x10,0x32,0x54,0x76,0x98,0x80

/* +0x0000006f6c6c6548 as decimal: 478560527688 */
bcd_ascii_hello:
    .byte 0x00,0x30,0x41,0x60,0x85,0x47,0x00,0x00,0x00,0x00

bcd_patterns:
    .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    .byte 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    .byte 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80
    .byte 0x23,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    .byte 0x23,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80
    .byte 0x78,0x56,0x34,0x12,0x10,0x32,0x54,0x76,0x98,0x00
    .byte 0x78,0x56,0x34,0x12,0x10,0x32,0x54,0x76,0x98,0x80
    .byte 0x00,0x30,0x41,0x60,0x85,0x47,0x00,0x00,0x00,0x00
bcd_patterns_end:

.section .bss
.align 8
bcd_out:    .skip 10
.align 8
int_out:    .skip 8
.align 2
top_before: .skip 2
