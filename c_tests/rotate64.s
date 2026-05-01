/* rotate64.s -- amd64 Linux GAS tests for ROL/ROR/RCL/RCR
 *
 * Build:
 *   as -o rotate64.o rotate64.s
 *   ld -o rotate64 rotate64.o
 *
 * Run:
 *   ./rotate64
 *   echo $?
 */

.intel_syntax noprefix
.global _start

.section .text

.set SYS_write, 1
.set SYS_exit,  60
.set CF_BIT,    0x001
.set OF_BIT,    0x800
.set CF_OF_MASK, 0x801

.macro FAIL code
    mov eax, SYS_exit
    mov edi, \code
    syscall
.endm

.macro SNAP_FLAGS reg
    pushfq
    pop \reg
.endm

.macro ASSERT_CF flagsreg, expected, code
    mov r10, \flagsreg
    and r10, CF_BIT
    cmp r10, \expected
    jne 1f
    jmp 2f
1:
    FAIL \code
2:
.endm

.macro ASSERT_CF_OF flagsreg, expected, code
    mov r10, \flagsreg
    and r10, CF_OF_MASK
    mov r11, \expected
    cmp r10, r11
    jne 1f
    jmp 2f
1:
    FAIL \code
2:
.endm

.macro ASSERT_EQ64 reg, imm, code
    movabs r11, \imm
    cmp \reg, r11
    jne 1f
    jmp 2f
1:
    FAIL \code
2:
.endm

.macro ASSERT_EQ32 reg, imm, code
    cmp \reg, \imm
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

.macro ASSERT_EQ8 reg, imm, code
    cmp \reg, \imm
    jne 1f
    jmp 2f
1:
    FAIL \code
2:
.endm

_start:
    /* T01: ROL r64,1 */
    movabs rax, 0x8000000000000001
    rol rax, 1
    SNAP_FLAGS r12
    ASSERT_EQ64 rax, 0x0000000000000003, 1
    ASSERT_CF_OF r12, (CF_BIT|OF_BIT), 101

    /* T02: ROR r64,1 */
    movabs rax, 0x8000000000000001
    ror rax, 1
    SNAP_FLAGS r12
    ASSERT_EQ64 rax, 0xc000000000000000, 2
    ASSERT_CF_OF r12, CF_BIT, 102

    /* T03: ROL r64,CL count > 1. OF undefined; check CF only. */
    movabs rax, 0x123456789abcdef0
    mov cl, 4
    rol rax, cl
    SNAP_FLAGS r12
    ASSERT_EQ64 rax, 0x23456789abcdef01, 3
    ASSERT_CF r12, CF_BIT, 103

    /* T04: ROR r64,CL count > 1. OF undefined; check CF only. */
    movabs rax, 0x123456789abcdef0
    mov cl, 4
    ror rax, cl
    SNAP_FLAGS r12
    ASSERT_EQ64 rax, 0x0123456789abcdef, 4
    ASSERT_CF r12, 0, 104

    /* T05: RCL r8,1 with CF initially clear */
    clc
    mov al, 0x80
    rcl al, 1
    SNAP_FLAGS r12
    ASSERT_EQ8 al, 0x00, 5
    ASSERT_CF_OF r12, (CF_BIT|OF_BIT), 105

    /* T06: RCL r8,2 with CF initially set. OF undefined. */
    stc
    mov al, 0x80
    rcl al, 2
    SNAP_FLAGS r12
    ASSERT_EQ8 al, 0x03, 6
    ASSERT_CF r12, 0, 106

    /* T07: RCR r8,1 with CF initially clear */
    clc
    mov al, 0x81
    rcr al, 1
    SNAP_FLAGS r12
    ASSERT_EQ8 al, 0x40, 7
    ASSERT_CF r12, CF_BIT, 107

    /* T08: RCR r8,1 with CF initially set */
    stc
    mov al, 0x01
    rcr al, 1
    SNAP_FLAGS r12
    ASSERT_EQ8 al, 0x80, 8
    ASSERT_CF r12, CF_BIT, 108

    /* T09: RCR r8,2 with CF initially set. OF undefined. */
    stc
    mov al, 0x80
    rcr al, 2
    SNAP_FLAGS r12
    ASSERT_EQ8 al, 0x60, 9
    ASSERT_CF r12, 0, 109

    /* T10: RCR r8,3 with CF initially clear. OF undefined. */
    clc
    mov al, 0x55
    rcr al, 3
    SNAP_FLAGS r12
    ASSERT_EQ8 al, 0x4a, 10
    ASSERT_CF r12, CF_BIT, 110

    /* T11: RCR m8,1 with CF initially clear */
    mov byte ptr [rip + mem8], 0x81
    clc
    rcr byte ptr [rip + mem8], 1
    SNAP_FLAGS r12
    mov al, byte ptr [rip + mem8]
    ASSERT_EQ8 al, 0x40, 11
    ASSERT_CF r12, CF_BIT, 111

    /* T12: RCR m8,2 with CF initially set. OF undefined. */
    mov byte ptr [rip + mem8], 0x80
    stc
    rcr byte ptr [rip + mem8], 2
    SNAP_FLAGS r12
    mov al, byte ptr [rip + mem8]
    ASSERT_EQ8 al, 0x60, 12
    ASSERT_CF r12, 0, 112

    /* T13: RCL m16,1 with CF initially set */
    mov word ptr [rip + mem16], 0x8000
    stc
    rcl word ptr [rip + mem16], 1
    SNAP_FLAGS r12
    mov ax, word ptr [rip + mem16]
    ASSERT_EQ16 ax, 0x0001, 13
    ASSERT_CF r12, CF_BIT, 113

    /* T14: RCL m32,2 with CF initially clear. OF undefined. */
    mov dword ptr [rip + mem32], 0x40000000
    clc
    rcl dword ptr [rip + mem32], 2
    SNAP_FLAGS r12
    mov eax, dword ptr [rip + mem32]
    ASSERT_EQ32 eax, 0x00000000, 14
    ASSERT_CF r12, CF_BIT, 114

    /* T15: RCR r64,1 with CF initially clear */
    mov rax, 1
    clc
    rcr rax, 1
    SNAP_FLAGS r12
    ASSERT_EQ64 rax, 0x0000000000000000, 15
    ASSERT_CF r12, CF_BIT, 115

    /* T16: RCR m64,1 with CF initially set */
    mov qword ptr [rip + mem64], 0
    stc
    rcr qword ptr [rip + mem64], 1
    SNAP_FLAGS r12
    mov rax, qword ptr [rip + mem64]
    ASSERT_EQ64 rax, 0x8000000000000000, 16
    ASSERT_CF r12, 0, 116

    /* T17: RCR m64,2 with CF initially set. OF undefined. */
    movabs rax, 0x8000000000000000
    mov qword ptr [rip + mem64], rax
    stc
    rcr qword ptr [rip + mem64], 2
    SNAP_FLAGS r12
    mov rax, qword ptr [rip + mem64]
    ASSERT_EQ64 rax, 0x6000000000000000, 17
    ASSERT_CF r12, 0, 117

    /* T18: ROL m32,1 */
    mov dword ptr [rip + mem32], 0x80000001
    rol dword ptr [rip + mem32], 1
    SNAP_FLAGS r12
    mov eax, dword ptr [rip + mem32]
    ASSERT_EQ32 eax, 0x00000003, 18
    ASSERT_CF_OF r12, (CF_BIT|OF_BIT), 118

    /* T19: ROR m16,1 */
    mov word ptr [rip + mem16], 0x8001
    ror word ptr [rip + mem16], 1
    SNAP_FLAGS r12
    mov ax, word ptr [rip + mem16]
    ASSERT_EQ16 ax, 0xc000, 19
    ASSERT_CF r12, CF_BIT, 119

    /* T20: count 0 preserves destination and CF */
    stc
    mov rax, 0x1234
    mov cl, 0
    rcr rax, cl
    SNAP_FLAGS r12
    ASSERT_EQ64 rax, 0x1234, 20
    ASSERT_CF r12, CF_BIT, 120

    /* T21: RCL r64,1 with CF clear */
    clc
    movabs rax, 0x8000000000000000
    rcl rax, 1
    SNAP_FLAGS r12
    ASSERT_EQ64 rax, 0x0000000000000000, 21
    ASSERT_CF r12, CF_BIT, 121

    /* T22: RCL r64,1 with CF set */
    xor rax, rax
    stc
    rcl rax, 1
    SNAP_FLAGS r12
    ASSERT_EQ64 rax, 0x0000000000000001, 22
    ASSERT_CF r12, 0, 122

    /* T23: RCL r16,4 with CF set. OF undefined. */
    stc
    mov ax, 0x1000
    mov cl, 4
    rcl ax, cl
    SNAP_FLAGS r12
    ASSERT_EQ16 ax, 0x0008, 23
    ASSERT_CF r12, CF_BIT, 123
    
    /* T24: RCR r16,4 with CF clear. OF undefined. */
    clc
    mov ax, 0x0001
    mov cl, 4
    rcr ax, cl
    SNAP_FLAGS r12
    ASSERT_EQ16 ax, 0x2000, 24
    ASSERT_CF r12, 0, 124

    /* success */
    mov eax, SYS_write
    mov edi, 1
    lea rsi, [rip + success_msg]
    mov edx, 8
    syscall

    mov eax, SYS_exit
    xor edi, edi
    syscall


.section .data
.align 8
mem64: .quad 0
.align 4
mem32: .long 0
.align 2
mem16: .word 0
mem8:  .byte 0

.section .rodata
success_msg:
    .ascii "success\n"
