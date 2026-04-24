.intel_syntax noprefix
.global _start

.section .text

_start:
    fninit

    call test_roundtrip_qwords
    call test_path_copy
    call test_compiler_copy
    call test_usingexec_copy

    mov eax, 4
    mov ebx, 1
    lea ecx, [success_msg]
    mov edx, 8
    int 0x80

    mov eax, 1
    xor ebx, ebx
    int 0x80

fail:
    mov eax, 1
    int 0x80

copy_reverse_x87:
    push ebx
    push esi
    push edi
    push ebp

    mov edi, eax        /* dst */
    mov esi, edx        /* src */
    mov ebx, ecx        /* len */

    /* qword_count = len & ~7 */
    mov ebp, ebx
    and ebp, -8
    jz .tail

    lea eax, [edi + ebp]
    lea edx, [esi + ebp]
    mov ecx, ebp

.qloop:
    sub eax, 8
    sub edx, 8
    fild qword ptr [edx]
    fistp qword ptr [eax]
    sub ecx, 8
    jnz .qloop

.tail:
    /* tail_count = len - qword_count */
    mov ecx, ebx
    sub ecx, ebp
    jz .done

    /* advance to first uncopied byte */
    lea esi, [esi + ebp]
    lea edi, [edi + ebp]

.tloop:
    mov al, byte ptr [esi]
    mov byte ptr [edi], al
    inc esi
    inc edi
    dec ecx
    jnz .tloop

.done:
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
    
memset_byte:
    push edi
    mov edi, eax
.mloop:
    test ecx, ecx
    jz .mdone
    mov byte ptr [edi], dl
    inc edi
    dec ecx
    jmp .mloop
.mdone:
    pop edi
    ret

verify_equal:
    push esi
    push edi
    mov esi, eax
    mov edi, edx
.vloop:
    test ecx, ecx
    jz .ok
    mov al, byte ptr [esi]
    cmp al, byte ptr [edi]
    jne .bad
    inc esi
    inc edi
    dec ecx
    jmp .vloop
.bad:
    pop edi
    pop esi
    jmp fail
.ok:
    pop edi
    pop esi
    ret

test_roundtrip_qwords:
    mov eax, dword ptr [pattern1]
    mov dword ptr [src_q], eax
    mov eax, dword ptr [pattern1 + 4]
    mov dword ptr [src_q + 4], eax
    fild qword ptr [src_q]
    fistp qword ptr [dst_q]
    mov eax, dword ptr [src_q]
    cmp eax, dword ptr [dst_q]
    jne .f1
    mov eax, dword ptr [src_q + 4]
    cmp eax, dword ptr [dst_q + 4]
    jne .f1

    mov eax, dword ptr [pattern2]
    mov dword ptr [src_q], eax
    mov eax, dword ptr [pattern2 + 4]
    mov dword ptr [src_q + 4], eax
    fild qword ptr [src_q]
    fistp qword ptr [dst_q]
    mov eax, dword ptr [src_q]
    cmp eax, dword ptr [dst_q]
    jne .f1
    mov eax, dword ptr [src_q + 4]
    cmp eax, dword ptr [dst_q + 4]
    jne .f1
    ret
.f1:
    mov ebx, 1
    jmp fail

prep_buffers:
    lea eax, [src_work]
    mov dl, 0xa5
    mov ecx, 256
    call memset_byte

    lea eax, [dst_work]
    mov dl, 0x5a
    mov ecx, 256
    call memset_byte
    ret

test_path_copy:
    call prep_buffers

    lea esi, [path_msg]
    lea edi, [src_work + 3]
    mov ecx, 0x39
.pcopy:
    mov al, byte ptr [esi]
    mov byte ptr [edi], al
    inc esi
    inc edi
    dec ecx
    jnz .pcopy

    lea eax, [dst_work + 29]
    lea edx, [src_work + 3]
    mov ecx, 0x39
    call copy_reverse_x87

    lea eax, [dst_work + 29]
    lea edx, [path_msg]
    mov ecx, 0x39
    mov ebx, 10
    call verify_equal
    ret

test_compiler_copy:
    call prep_buffers

    lea esi, [compiler_msg]
    lea edi, [src_work + 5]
    mov ecx, 0x23
.ccopy:
    mov al, byte ptr [esi]
    mov byte ptr [edi], al
    inc esi
    inc edi
    dec ecx
    jnz .ccopy

    lea eax, [dst_work + 21]
    lea edx, [src_work + 5]
    mov ecx, 0x23
    call copy_reverse_x87

    lea eax, [dst_work + 21]
    lea edx, [compiler_msg]
    mov ecx, 0x23
    mov ebx, 11
    call verify_equal
    ret

test_usingexec_copy:
    call prep_buffers

    lea esi, [usingexec_msg]
    lea edi, [src_work + 1]
    mov ecx, 0x16
.ucopy:
    mov al, byte ptr [esi]
    mov byte ptr [edi], al
    inc esi
    inc edi
    dec ecx
    jnz .ucopy

    lea eax, [dst_work + 17]
    lea edx, [src_work + 1]
    mov ecx, 0x16
    call copy_reverse_x87

    lea eax, [dst_work + 17]
    lea edx, [usingexec_msg]
    mov ecx, 0x16
    mov ebx, 12
    call verify_equal
    ret

.section .rodata
success_msg:
    .ascii "success\n"

pattern1:
    .quad 0x0123456789abcdef
pattern2:
    .quad 0xfedcba9876543210

path_msg:
    .ascii "Path \"/usr/lib/fpc/3.2.2/units/i386-linux/rtl/\" not found"

compiler_msg:
    .ascii "Compiler: /usr/lib/fpc/3.2.2/ppc386"

usingexec_msg:
    .ascii "Using executable path:"

.section .bss
.align 8
src_q:    .skip 8
dst_q:    .skip 8

.align 16
src_work: .skip 256
dst_work: .skip 256
