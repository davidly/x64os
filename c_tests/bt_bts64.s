/* bt_bts64.s -- amd64 Linux GAS tests for BT/BTS register + memory forms
 *
 * Build:
 *   as -o bt_bts64.o bt_bts64.s
 *   ld -o bt_bts64 bt_bts64.o
 *
 * Run:
 *   ./bt_bts64
 *   echo $?
 */

.intel_syntax noprefix
.global _start

.section .text

.set SYS_write, 1
.set SYS_exit,  60
.set CF_BIT,    1

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

_start:
    /* T01: bt r64, imm: bit clear */
    mov rax, 0
    bt rax, 5
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 1
    ASSERT_EQ64 rax, 0, 1

    /* T02: bt r64, imm: bit set */
    mov rax, 0x20
    bt rax, 5
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 2
    ASSERT_EQ64 rax, 0x20, 2

    /* T03: bt r64, reg: register bit index is modulo 64 */
    mov rax, 1
    mov rcx, 64
    bt rax, rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 3
    ASSERT_EQ64 rax, 1, 3

    /* T04: bts r64, imm: old bit clear, bit becomes set */
    xor rax, rax
    bts rax, 5
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 4
    ASSERT_EQ64 rax, 0x20, 4

    /* T05: bts r64, imm: old bit set, remains set */
    mov rax, 0x20
    bts rax, 5
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 5
    ASSERT_EQ64 rax, 0x20, 5

    /* T06: bts r64, reg: index 65 maps to bit 1 */
    xor rax, rax
    mov rcx, 65
    bts rax, rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 6
    ASSERT_EQ64 rax, 2, 6

    /* T07: bt r32, imm */
    mov eax, 0x80000000
    bt eax, 31
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 7
    ASSERT_EQ32 eax, 0x80000000, 7

    /* T08: bts r32, reg: index 32 maps to bit 0 */
    xor eax, eax
    mov ecx, 32
    bts eax, ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 8
    ASSERT_EQ32 eax, 1, 8

    /* T09: bt r16, imm */
    mov ax, 0x8000
    bt ax, 15
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 9
    ASSERT_EQ16 ax, 0x8000, 9

    /* T10: bts r16, reg: index 16 maps to bit 0 */
    xor ax, ax
    mov cx, 16
    bts ax, cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 10
    ASSERT_EQ16 ax, 1, 10

    /* T11: bt m64, imm: old bit set, memory unchanged */
    mov qword ptr [rip + mem64], 0x10
    bt qword ptr [rip + mem64], 4
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 11
    mov rax, qword ptr [rip + mem64]
    ASSERT_EQ64 rax, 0x10, 11

    /* T12: bts m64, imm: old bit clear, bit set */
    mov qword ptr [rip + mem64], 0x10
    bts qword ptr [rip + mem64], 7
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 12
    mov rax, qword ptr [rip + mem64]
    ASSERT_EQ64 rax, 0x90, 12

    /* T13: bts m64, imm: old bit set */
    bts qword ptr [rip + mem64], 7
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 13
    mov rax, qword ptr [rip + mem64]
    ASSERT_EQ64 rax, 0x90, 13

    /* T14: bt m64, reg: bit 65 reads qword[1] bit 1 */
    mov qword ptr [rip + mem_bits], 0
    mov qword ptr [rip + mem_bits + 8], 2
    mov rcx, 65
    bt qword ptr [rip + mem_bits], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 14

    /* T15: bts m64, reg: bit 70 sets qword[1] bit 6 */
    mov qword ptr [rip + mem_bits], 0
    mov qword ptr [rip + mem_bits + 8], 0
    mov rcx, 70
    bts qword ptr [rip + mem_bits], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 15
    mov rax, qword ptr [rip + mem_bits]
    ASSERT_EQ64 rax, 0, 15
    mov rax, qword ptr [rip + mem_bits + 8]
    ASSERT_EQ64 rax, 0x40, 15

    /* T16: bts m64, reg: same bit now old=1 */
    mov rcx, 70
    bts qword ptr [rip + mem_bits], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 16
    mov rax, qword ptr [rip + mem_bits + 8]
    ASSERT_EQ64 rax, 0x40, 16

    /* T17: bts m32, reg: bit 33 sets dword[1] bit 1 */
    mov dword ptr [rip + mem32], 0
    mov dword ptr [rip + mem32 + 4], 0
    mov ecx, 33
    bts dword ptr [rip + mem32], ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 17
    mov eax, dword ptr [rip + mem32]
    ASSERT_EQ32 eax, 0, 17
    mov eax, dword ptr [rip + mem32 + 4]
    ASSERT_EQ32 eax, 2, 17

    /* T18: bt m16, reg: bit 17 reads word[1] bit 1 */
    mov word ptr [rip + mem16], 0
    mov word ptr [rip + mem16 + 2], 2
    mov cx, 17
    bt word ptr [rip + mem16], cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 18

        /* T19: btr m64, reg: bit 70 clears qword[1] bit 6, CF=1 */
    mov qword ptr [rip + mem_bits], 0
    mov qword ptr [rip + mem_bits + 8], 0x40
    mov rcx, 70
    btr qword ptr [rip + mem_bits], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 19
    mov rax, qword ptr [rip + mem_bits + 8]
    ASSERT_EQ64 rax, 0, 19

    /* T20: btr m64, reg: old bit clear, memory unchanged, CF=0 */
    mov rcx, 70
    btr qword ptr [rip + mem_bits], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 20
    mov rax, qword ptr [rip + mem_bits + 8]
    ASSERT_EQ64 rax, 0, 20

    /* T21: btc m64, reg: bit 130 toggles qword[2] bit 2 from 0->1, CF=0 */
    mov qword ptr [rip + mem_bits], 0
    mov qword ptr [rip + mem_bits + 8], 0
    mov qword ptr [rip + mem_bits + 16], 0
    mov rcx, 130
    btc qword ptr [rip + mem_bits], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 21
    mov rax, qword ptr [rip + mem_bits + 16]
    ASSERT_EQ64 rax, 4, 21

    /* T22: btc m64, reg: same bit toggles 1->0, CF=1 */
    mov rcx, 130
    btc qword ptr [rip + mem_bits], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 22
    mov rax, qword ptr [rip + mem_bits + 16]
    ASSERT_EQ64 rax, 0, 22

    /* T23: bts m64 with negative offset: base=&qword[1], bit=-1 => qword[0] bit 63 */
    mov qword ptr [rip + mem_bits], 0
    mov qword ptr [rip + mem_bits + 8], 0
    lea rbx, [rip + mem_bits + 8]
    mov rcx, -1
    bts qword ptr [rbx], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 23
    mov rax, qword ptr [rip + mem_bits]
    ASSERT_EQ64 rax, 0x8000000000000000, 23

    /* T24: bt m64 with negative offset: base=&qword[1], bit=-1 reads qword[0] bit 63 */
    lea rbx, [rip + mem_bits + 8]
    mov rcx, -1
    bt qword ptr [rbx], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 24

    /* T25: btr m64 with negative offset clears qword[0] bit 63 */
    lea rbx, [rip + mem_bits + 8]
    mov rcx, -1
    btr qword ptr [rbx], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 25
    mov rax, qword ptr [rip + mem_bits]
    ASSERT_EQ64 rax, 0, 25

    /* T26: btc m32, reg: bit 65 toggles dword[2] bit 1 */
    mov dword ptr [rip + mem32], 0
    mov dword ptr [rip + mem32 + 4], 0
    mov dword ptr [rip + mem32 + 8], 0
    mov ecx, 65
    btc dword ptr [rip + mem32], ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 26
    mov eax, dword ptr [rip + mem32 + 8]
    ASSERT_EQ32 eax, 2, 26

    /* T27: btr m32, reg: bit 65 clears dword[2] bit 1 */
    mov ecx, 65
    btr dword ptr [rip + mem32], ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 27
    mov eax, dword ptr [rip + mem32 + 8]
    ASSERT_EQ32 eax, 0, 27

    /* T28: bts m32 with negative offset: base=&dword[1], bit=-1 => dword[0] bit 31 */
    mov dword ptr [rip + mem32], 0
    mov dword ptr [rip + mem32 + 4], 0
    lea rbx, [rip + mem32 + 4]
    mov ecx, -1
    bts dword ptr [rbx], ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 28
    mov eax, dword ptr [rip + mem32]
    ASSERT_EQ32 eax, 0x80000000, 28

    /* T29: btc m16, reg: bit 33 toggles word[2] bit 1 */
    mov word ptr [rip + mem16], 0
    mov word ptr [rip + mem16 + 2], 0
    mov word ptr [rip + mem16 + 4], 0
    mov cx, 33
    btc word ptr [rip + mem16], cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 29
    movzx eax, word ptr [rip + mem16 + 4]
    ASSERT_EQ32 eax, 2, 29

    /* T30: btr m16, reg: bit 33 clears word[2] bit 1 */
    mov cx, 33
    btr word ptr [rip + mem16], cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 30
    movzx eax, word ptr [rip + mem16 + 4]
    ASSERT_EQ32 eax, 0, 30

    /* T31: bts m16 with negative offset: base=&word[1], bit=-1 => word[0] bit 15 */
    mov word ptr [rip + mem16], 0
    mov word ptr [rip + mem16 + 2], 0
    lea rbx, [rip + mem16 + 2]
    mov cx, -1
    bts word ptr [rbx], cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 31
    movzx eax, word ptr [rip + mem16]
    ASSERT_EQ32 eax, 0x8000, 31

    /* T32: btr r64, reg: old bit set, clears it */
    mov rax, 0x20
    mov rcx, 5
    btr rax, rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 32
    ASSERT_EQ64 rax, 0, 32

    /* T33: btr r64, reg: old bit clear */
    mov rax, 0
    mov rcx, 5
    btr rax, rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 33
    ASSERT_EQ64 rax, 0, 33

    /* T34: btc r64, reg: clear -> set */
    mov rax, 0
    mov rcx, 5
    btc rax, rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 34
    ASSERT_EQ64 rax, 0x20, 34

    /* T35: btc r64, reg: set -> clear */
    mov rax, 0x20
    mov rcx, 5
    btc rax, rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 35
    ASSERT_EQ64 rax, 0, 35

    /* T36: btr r64, reg index modulo 64: bit 65 => bit 1 */
    mov rax, 2
    mov rcx, 65
    btr rax, rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 36
    ASSERT_EQ64 rax, 0, 36

    /* T37: btc r64, reg index modulo 64: bit 130 => bit 2 */
    mov rax, 0
    mov rcx, 130
    btc rax, rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 37
    ASSERT_EQ64 rax, 4, 37

    /* T38: btr r32, reg index modulo 32: bit 33 => bit 1 */
    mov eax, 2
    mov ecx, 33
    btr eax, ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 38
    ASSERT_EQ32 eax, 0, 38

    /* T39: btc r32, reg index modulo 32 */
    mov eax, 0
    mov ecx, 63                  /* 63 mod 32 = 31 */
    btc eax, ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 39
    ASSERT_EQ32 eax, 0x80000000, 39

    /* T40: btr r16, reg index modulo 16 */
    mov ax, 0x8000
    mov cx, 31                   /* 31 mod 16 = 15 */
    btr ax, cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 1, 40
    ASSERT_EQ16 ax, 0, 40

    /* T41: btc r16, reg index modulo 16 */
    mov ax, 0
    mov cx, 16                   /* 16 mod 16 = 0 */
    btc ax, cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 41
    ASSERT_EQ16 ax, 1, 41

    /* T42: memory m64 boundary: base qword[0], bit 64 => qword[1] bit 0 */
    mov qword ptr [rip + mem_bits], 0
    mov qword ptr [rip + mem_bits + 8], 0
    mov rcx, 64
    bts qword ptr [rip + mem_bits], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 42
    mov rax, qword ptr [rip + mem_bits]
    ASSERT_EQ64 rax, 0, 42
    mov rax, qword ptr [rip + mem_bits + 8]
    ASSERT_EQ64 rax, 1, 42

    /* T43: memory m32 boundary: bit 32 => dword[1] bit 0 */
    mov dword ptr [rip + mem32], 0
    mov dword ptr [rip + mem32 + 4], 0
    mov ecx, 32
    bts dword ptr [rip + mem32], ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 43
    mov eax, dword ptr [rip + mem32 + 4]
    ASSERT_EQ32 eax, 1, 43

    /* T44: memory m16 boundary: bit 16 => word[1] bit 0 */
    mov word ptr [rip + mem16], 0
    mov word ptr [rip + mem16 + 2], 0
    mov cx, 16
    bts word ptr [rip + mem16], cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 44
    movzx eax, word ptr [rip + mem16 + 2]
    ASSERT_EQ32 eax, 1, 44

    /* T45: memory m64 negative exact boundary: base qword[1], bit -64 => qword[0] bit 0 */
    mov qword ptr [rip + mem_bits], 0
    mov qword ptr [rip + mem_bits + 8], 0
    lea rbx, [rip + mem_bits + 8]
    mov rcx, -64
    bts qword ptr [rbx], rcx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 45
    mov rax, qword ptr [rip + mem_bits]
    ASSERT_EQ64 rax, 1, 45

    /* T46: memory m32 negative exact boundary: base dword[1], bit -32 => dword[0] bit 0 */
    mov dword ptr [rip + mem32], 0
    mov dword ptr [rip + mem32 + 4], 0
    lea rbx, [rip + mem32 + 4]
    mov ecx, -32
    bts dword ptr [rbx], ecx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 46
    mov eax, dword ptr [rip + mem32]
    ASSERT_EQ32 eax, 1, 46

    /* T47: memory m16 negative exact boundary: base word[1], bit -16 => word[0] bit 0 */
    mov word ptr [rip + mem16], 0
    mov word ptr [rip + mem16 + 2], 0
    lea rbx, [rip + mem16 + 2]
    mov cx, -16
    bts word ptr [rbx], cx
    SNAP_FLAGS r12
    ASSERT_CF r12, 0, 47
    movzx eax, word ptr [rip + mem16]
    ASSERT_EQ32 eax, 1, 47    

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
mem64:
    .quad 0

.align 8
mem_bits:
    .quad 0, 0, 0

.align 4
mem32:
    .long 0, 0, 0

.align 2
mem16:
    .word 0, 0, 0

.section .rodata
success_msg:
    .ascii "success\n"
    