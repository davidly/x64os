/* aaxlat32.s  --  32-bit Linux (i386) GAS tests: AAM, AAD, XLAT, XLATB
 *
 * Build:
 *   as --32 -o aaxlat32.o aaxlat32.s
 *   ld -m elf_i386 -o aaxlat32 aaxlat32.o
 *
 * Run:
 *   ./aaxlat32
 *   echo $?
 */

        .section .text
        .code32
        .globl _start

        .set SYS_exit,  1
        .set SYS_write, 4

        /* CF PF AF ZF SF OF */
        .set STATUS_MASK, 0x000008D5
        /* SF ZF PF */
        .set SZP_MASK,    0x000000C4

.macro FAIL code
        movl    $SYS_exit, %eax
        movl    $\code, %ebx
        int     $0x80
.endm

.macro ASSERT_AL imm, code
        cmpb    $\imm, %al
        jne     1f
        jmp     2f
1:      FAIL    \code
2:
.endm

.macro ASSERT_AH imm, code
        cmpb    $\imm, %ah
        jne     1f
        jmp     2f
1:      FAIL    \code
2:
.endm

/* Assert saved flags reg has expected SZP bits */
.macro ASSERT_SZP_SAVED flagsreg, expected, code
        movl    \flagsreg, %ecx
        andl    $SZP_MASK, %ecx
        cmpl    $\expected, %ecx
        jne     1f
        jmp     2f
1:      FAIL    \code
2:
.endm

/* Take full EFLAGS snapshot into REG */
.macro SNAP_FLAGS reg
        pushfl
        popl    \reg
.endm

/* Compare masked status bits of two saved-flag regs */
.macro ASSERT_STATUS_SAVED_EQ before, after, code
        movl    \before, %ecx
        andl    $STATUS_MASK, %ecx
        movl    \after, %edx
        andl    $STATUS_MASK, %edx
        cmpl    %edx, %ecx
        jne     1f
        jmp     2f
1:      FAIL    \code
2:
.endm


_start:
        /* =========================
         * AAM tests (result + SZP from AL)
         * ========================= */

        /* T01: AL=35 -> AAM(10): AH=3, AL=5; AL=0x05 => PF=1 => SZP=0x04 */
        xorl    %eax, %eax
        movb    $0x23, %al
        aam
        SNAP_FLAGS %esi
        ASSERT_AH 0x03, 1
        ASSERT_AL 0x05, 1
        ASSERT_SZP_SAVED %esi, 0x04, 1

        /* T02: AL=0 -> AH=0, AL=0; SZP => ZF+PF = 0x44 */
        xorl    %eax, %eax
        movb    $0x00, %al
        aam
        SNAP_FLAGS %esi
        ASSERT_AH 0x00, 2
        ASSERT_AL 0x00, 2
        ASSERT_SZP_SAVED %esi, 0x44, 2

        /* T03: AL=27 -> AH=2, AL=7; 0x07 odd parity => PF=0 => SZP=0 */
        xorl    %eax, %eax
        movb    $27, %al
        aam     $0x0a
        SNAP_FLAGS %esi
        ASSERT_AH 0x02, 3
        ASSERT_AL 0x07, 3
        ASSERT_SZP_SAVED %esi, 0x00, 3


        /* =========================
         * AAD tests (result + SZP from AL; AH becomes 0)
         * ========================= */

        /* T04: AH=3,AL=5 -> AAD(10): AL=0x23, AH=0; 0x23 odd parity => PF=0 => SZP=0 */
        xorl    %eax, %eax
        movb    $0x03, %ah
        movb    $0x05, %al
        aad
        SNAP_FLAGS %esi
        ASSERT_AH 0x00, 4
        ASSERT_AL 0x23, 4
        ASSERT_SZP_SAVED %esi, 0x00, 4

        /* T05: AH=0,AL=0 -> AL=0,AH=0; SZP=0x44 */
        xorl    %eax, %eax
        movb    $0x00, %ah
        movb    $0x00, %al
        aad     $0x0a
        SNAP_FLAGS %esi
        ASSERT_AH 0x00, 5
        ASSERT_AL 0x00, 5
        ASSERT_SZP_SAVED %esi, 0x44, 5

        /* T06: base 0x80: AH=1,AL=0 -> AL=0x80,AH=0; SZP = SF=1 => 0x80 */
        xorl    %eax, %eax
        movb    $0x01, %ah
        movb    $0x00, %al
        aad     $0x80
        SNAP_FLAGS %esi
        ASSERT_AH 0x00, 6
        ASSERT_AL 0x80, 6
        ASSERT_SZP_SAVED %esi, 0x80, 6


        /* =========================
         * XLAT / XLATB tests
         * Each test:
         *   - creates known flags
         *   - snapshots flags BEFORE
         *   - runs xlat/xlatb
         *   - snapshots flags AFTER
         *   - compares (masked) flags equality
         *   - then checks AL result
         * ========================= */

        movl    $xlat_table, %ebx

        /* T07: xlatb AL=0 -> 0xA5; flags unchanged */
        movl    $1, %eax
        subl    $2, %eax             /* establish a nontrivial flags state */
        SNAP_FLAGS %esi              /* before */
        movb    $0x00, %al
        xlatb
        SNAP_FLAGS %edi              /* after */
        ASSERT_STATUS_SAVED_EQ %esi, %edi, 7
        ASSERT_AL 0xA5, 7

        /* T08: xlat AL=0x7F -> 0xDA; flags unchanged */
        movl    $1, %eax
        subl    $2, %eax
        SNAP_FLAGS %esi
        movb    $0x7f, %al
        xlat
        SNAP_FLAGS %edi
        ASSERT_STATUS_SAVED_EQ %esi, %edi, 8
        ASSERT_AL 0xDA, 8

        /* T09: xlatb AL=0xFF -> 0x5A; flags unchanged */
        movl    $1, %eax
        subl    $2, %eax
        SNAP_FLAGS %esi
        movb    $0xff, %al
        xlatb
        SNAP_FLAGS %edi
        ASSERT_STATUS_SAVED_EQ %esi, %edi, 9
        ASSERT_AL 0x5A, 9


        /* success: write(1,"success\n",8) */
        movl    $SYS_write, %eax
        movl    $1, %ebx
        movl    $success_msg, %ecx
        movl    $8, %edx
        int     $0x80

        /* exit(0) */
        movl    $SYS_exit, %eax
        xorl    %ebx, %ebx
        int     $0x80


        .section .rodata
        .balign 16
success_msg:
        .ascii "success\n"

/* table[i] = i ^ 0xA5 */
xlat_table:
        .set i, 0
        .rept 256
        .byte (i ^ 0xA5)
        .set i, i+1
        .endr
        