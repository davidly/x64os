/* rotate32.s -- 32-bit x86 Linux GAS tests for ROL/ROR/RCL/RCR
 *
 * Build:
 *   as --32 -o rotate32.o rotate32.s
 *   ld -m elf_i386 -o rotate32 rotate32.o
 *
 * Run:
 *   ./rotate32
 *   echo $?
 */

.intel_syntax noprefix
.global _start

.section .text

.set SYS_exit,  1
.set SYS_write, 4
.set CF_BIT,    0x001
.set OF_BIT,    0x800
.set CF_OF_MASK, 0x801

.macro FAIL code
    mov eax, SYS_exit
    mov ebx, \code
    int 0x80
.endm

.macro SNAP_FLAGS reg
    pushfd
    pop \reg
.endm

.macro ASSERT_CF flagsreg, expected, code
    mov edx, \flagsreg
    and edx, CF_BIT
    cmp edx, \expected
    jne 1f
    jmp 2f
1:
    FAIL \code
2:
.endm

.macro ASSERT_CF_OF flagsreg, expected, code
    mov edx, \flagsreg
    and edx, CF_OF_MASK
    mov esi, \expected
    cmp edx, esi
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
    /* T01: ROL r32,1 */
    mov eax, 0x80000001
    rol eax, 1
    SNAP_FLAGS edi
    ASSERT_EQ32 eax, 0x00000003, 1
    ASSERT_CF_OF edi, (CF_BIT|OF_BIT), 101

    /* T02: ROR r32,1 */
    mov eax, 0x80000001
    ror eax, 1
    SNAP_FLAGS edi
    ASSERT_EQ32 eax, 0xc0000000, 2
    ASSERT_CF_OF edi, CF_BIT, 102

    /* T03: ROL r32,CL count > 1. OF undefined. */
    mov eax, 0x12345678
    mov cl, 4
    rol eax, cl
    SNAP_FLAGS edi
    ASSERT_EQ32 eax, 0x23456781, 3
    ASSERT_CF edi, CF_BIT, 103

    /* T04: ROR r32,CL count > 1. OF undefined. */
    mov eax, 0x12345678
    mov cl, 4
    ror eax, cl
    SNAP_FLAGS edi
    ASSERT_EQ32 eax, 0x81234567, 4
    ASSERT_CF edi, CF_BIT, 104
    
    /* T05: RCL r8,1 with CF clear */
    clc
    mov al, 0x80
    rcl al, 1
    SNAP_FLAGS edi
    ASSERT_EQ8 al, 0x00, 5
    ASSERT_CF_OF edi, (CF_BIT|OF_BIT), 105

    /* T06: RCL r8,2 with CF set. OF undefined. */
    stc
    mov al, 0x80
    rcl al, 2
    SNAP_FLAGS edi
    ASSERT_EQ8 al, 0x03, 6
    ASSERT_CF edi, 0, 106

    /* T07: RCR r8,1 with CF clear */
    clc
    mov al, 0x81
    rcr al, 1
    SNAP_FLAGS edi
    ASSERT_EQ8 al, 0x40, 7
    ASSERT_CF edi, CF_BIT, 107

    /* T08: RCR r8,1 with CF set */
    stc
    mov al, 0x01
    rcr al, 1
    SNAP_FLAGS edi
    ASSERT_EQ8 al, 0x80, 8
    ASSERT_CF edi, CF_BIT, 108

    /* T09: RCR r8,2 with CF set. OF undefined. */
    stc
    mov al, 0x80
    rcr al, 2
    SNAP_FLAGS edi
    ASSERT_EQ8 al, 0x60, 9
    ASSERT_CF edi, 0, 109

    /* T10: RCR r8,3 with CF clear. OF undefined. */
    clc
    mov al, 0x55
    rcr al, 3
    SNAP_FLAGS edi
    ASSERT_EQ8 al, 0x4a, 10
    ASSERT_CF edi, CF_BIT, 110

    /* T11: RCR m8,1 with CF clear */
    mov byte ptr [mem8], 0x81
    clc
    rcr byte ptr [mem8], 1
    SNAP_FLAGS edi
    mov al, byte ptr [mem8]
    ASSERT_EQ8 al, 0x40, 11
    ASSERT_CF edi, CF_BIT, 111

    /* T12: RCR m8,2 with CF set. OF undefined. */
    mov byte ptr [mem8], 0x80
    stc
    rcr byte ptr [mem8], 2
    SNAP_FLAGS edi
    mov al, byte ptr [mem8]
    ASSERT_EQ8 al, 0x60, 12
    ASSERT_CF edi, 0, 112

    /* T13: RCL m16,1 with CF set */
    mov word ptr [mem16], 0x8000
    stc
    rcl word ptr [mem16], 1
    SNAP_FLAGS edi
    mov ax, word ptr [mem16]
    ASSERT_EQ16 ax, 0x0001, 13
    ASSERT_CF edi, CF_BIT, 113

    /* T14: RCL m32,2 with CF clear. OF undefined. */
    mov dword ptr [mem32], 0x40000000
    clc
    rcl dword ptr [mem32], 2
    SNAP_FLAGS edi
    mov eax, dword ptr [mem32]
    ASSERT_EQ32 eax, 0x00000000, 14
    ASSERT_CF edi, CF_BIT, 114

    /* T15: RCR r32,1 with CF clear */
    mov eax, 1
    clc
    rcr eax, 1
    SNAP_FLAGS edi
    ASSERT_EQ32 eax, 0x00000000, 15
    ASSERT_CF edi, CF_BIT, 115

    /* T16: RCR m32,1 with CF set */
    mov dword ptr [mem32], 0
    stc
    rcr dword ptr [mem32], 1
    SNAP_FLAGS edi
    mov eax, dword ptr [mem32]
    ASSERT_EQ32 eax, 0x80000000, 16
    ASSERT_CF edi, 0, 116

    /* T17: RCR m32,2 with CF set. OF undefined. */
    mov dword ptr [mem32], 0x80000000
    stc
    rcr dword ptr [mem32], 2
    SNAP_FLAGS edi
    mov eax, dword ptr [mem32]
    ASSERT_EQ32 eax, 0x60000000, 17
    ASSERT_CF edi, 0, 117

    /* T18: ROL m32,1 */
    mov dword ptr [mem32], 0x80000001
    rol dword ptr [mem32], 1
    SNAP_FLAGS edi
    mov eax, dword ptr [mem32]
    ASSERT_EQ32 eax, 0x00000003, 18
    ASSERT_CF_OF edi, (CF_BIT|OF_BIT), 118

    /* T19: ROR m16,1 */
    mov word ptr [mem16], 0x8001
    ror word ptr [mem16], 1
    SNAP_FLAGS edi
    mov ax, word ptr [mem16]
    ASSERT_EQ16 ax, 0xc000, 19
    ASSERT_CF edi, CF_BIT, 119

    /* T20: count 0 preserves destination and CF */
    stc
    mov eax, 0x1234
    mov cl, 0
    rcr eax, cl
    SNAP_FLAGS edi
    ASSERT_EQ32 eax, 0x1234, 20
    ASSERT_CF edi, CF_BIT, 120

    /* T21: RCL r32,1 with CF clear */
    clc
    mov eax, 0x80000000
    rcl eax, 1
    SNAP_FLAGS edi
    ASSERT_EQ32 eax, 0x00000000, 21
    ASSERT_CF edi, CF_BIT, 121

    /* T22: RCL r32,1 with CF set */
    xor eax, eax
    stc
    rcl eax, 1
    SNAP_FLAGS edi
    ASSERT_EQ32 eax, 0x00000001, 22
    ASSERT_CF edi, 0, 122

    /* T23: RCL r16,4 with CF set. OF undefined. */
    stc
    mov ax, 0x1000
    mov cl, 4
    rcl ax, cl
    SNAP_FLAGS edi
    ASSERT_EQ16 ax, 0x0008, 23
    ASSERT_CF edi, CF_BIT, 123

    /* T24: RCR r16,4 with CF clear. OF undefined. */
    clc
    mov ax, 0x0001
    mov cl, 4
    rcr ax, cl
    SNAP_FLAGS edi
    ASSERT_EQ16 ax, 0x2000, 24
    ASSERT_CF edi, 0, 124

    /* success */
    mov eax, SYS_write
    mov ebx, 1
    mov ecx, offset success_msg
    mov edx, 8
    int 0x80

    mov eax, SYS_exit
    xor ebx, ebx
    int 0x80


.section .data
.align 4
mem32: .long 0
.align 2
mem16: .word 0
mem8:  .byte 0

.section .rodata
success_msg:
    .ascii "success\n"
    