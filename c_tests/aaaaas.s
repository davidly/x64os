/* bcd_adjust32.s  --  32-bit Linux (i386) GAS tests:
 *   AAA, AAS, DAA, DAS
 *
 * Build:
 *   as --32 -o bcd_adjust32.o bcd_adjust32.s
 *   ld -m elf_i386 -o bcd_adjust32 bcd_adjust32.o
 *
 * Run:
 *   ./bcd_adjust32
 *   echo $?
 *
 * Exit code:
 *   0  = pass (also prints "success")
 *   !=0 = failed test id
 */

        .section .text
        .code32
        .globl _start

        .set SYS_exit,  1
        .set SYS_write, 4

        /* EFLAGS bits */
        .set CF_BIT, 0x00000001
        .set PF_BIT, 0x00000004
        .set AF_BIT, 0x00000010
        .set ZF_BIT, 0x00000040
        .set SF_BIT, 0x00000080

        /* Masks */
        .set AF_CF_MASK,      (AF_BIT | CF_BIT)                             /* 0x11 */
        .set SZP_AF_CF_MASK,  (SF_BIT | ZF_BIT | PF_BIT | AF_BIT | CF_BIT)  /* 0xD5 */

/* -------- helpers -------- */
.macro FAIL code
        movl    $SYS_exit, %eax
        movl    $\code, %ebx
        int     $0x80
.endm

.macro ASSERT_AL imm, code
        cmpb    $\imm, %al
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_AH imm, code
        cmpb    $\imm, %ah
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* Snapshot EFLAGS immediately after instruction into REG */
.macro SNAP_FLAGS reg
        pushfl
        popl    \reg
.endm

/* Set AF/CF deterministically (others don't matter) */
.macro SET_AF_CF af_on, cf_on
        pushfl
        popl    %edx
        andl    $~AF_CF_MASK, %edx
        .if \af_on
          orl   $AF_BIT, %edx
        .endif
        .if \cf_on
          orl   $CF_BIT, %edx
        .endif
        pushl   %edx
        popfl
.endm

/* Assert AF/CF bits in saved flags == expected mask */
.macro ASSERT_AF_CF_SAVED flagsreg, expected_mask, code
        movl    \flagsreg, %ecx
        andl    $AF_CF_MASK, %ecx
        cmpl    $\expected_mask, %ecx
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* Assert (SF|ZF|PF|AF|CF) bits in saved flags == expected mask */
.macro ASSERT_SZP_AF_CF_SAVED flagsreg, expected_mask, code
        movl    \flagsreg, %ecx
        andl    $SZP_AF_CF_MASK, %ecx
        cmpl    $\expected_mask, %ecx
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        /* =========================================================
         * AAA tests (defined: AL/AH + AF/CF)
         * ========================================================= */

        /* T01: AAA no adjust: AL=0x05, AF=0 -> AL=0x05, AH unchanged, AF=CF=0 */
        xorl    %eax, %eax
        movb    $0x12, %ah
        movb    $0x05, %al
        SET_AF_CF 0, 0
        aaa
        SNAP_FLAGS %esi
        ASSERT_AH 0x12, 1
        ASSERT_AL 0x05, 1
        ASSERT_AF_CF_SAVED %esi, 0x00, 1

        /* T02: AAA adjust via low nibble>9: AL=0x0B -> AL=0x01, AH++, AF=CF=1 */
        xorl    %eax, %eax
        movb    $0x10, %ah
        movb    $0x0b, %al
        SET_AF_CF 0, 0
        aaa
        SNAP_FLAGS %esi
        ASSERT_AH 0x11, 2
        ASSERT_AL 0x01, 2
        ASSERT_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 2

        /* T03: AAA adjust via AF=1: AL=0x08 -> AL=0x0E, AH++, AF=CF=1 */
        xorl    %eax, %eax
        movb    $0x20, %ah
        movb    $0x08, %al
        SET_AF_CF 1, 0
        aaa
        SNAP_FLAGS %esi
        ASSERT_AH 0x21, 3
        ASSERT_AL 0x0e, 3
        ASSERT_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 3


        /* =========================================================
         * AAS tests (defined: AL/AH + AF/CF)
         * ========================================================= */

        /* T04: AAS no adjust: AL=0x05, AF=0 -> AL=0x05, AH unchanged, AF=CF=0 */
        xorl    %eax, %eax
        movb    $0x30, %ah
        movb    $0x05, %al
        SET_AF_CF 0, 0
        aas
        SNAP_FLAGS %esi
        ASSERT_AH 0x30, 4
        ASSERT_AL 0x05, 4
        ASSERT_AF_CF_SAVED %esi, 0x00, 4

        /* T05: AAS adjust via low nibble>9: AL=0x0B -> AL=0x05, AH--, AF=CF=1 */
        xorl    %eax, %eax
        movb    $0x40, %ah
        movb    $0x0b, %al
        SET_AF_CF 0, 0
        aas
        SNAP_FLAGS %esi
        ASSERT_AH 0x3f, 5
        ASSERT_AL 0x05, 5
        ASSERT_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 5

        /* T06: AAS adjust via AF=1: AL=0x08 -> AL=0x02, AH--, AF=CF=1 */
        xorl    %eax, %eax
        movb    $0x50, %ah
        movb    $0x08, %al
        SET_AF_CF 1, 0
        aas
        SNAP_FLAGS %esi
        ASSERT_AH 0x4f, 6
        ASSERT_AL 0x02, 6
        ASSERT_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 6


        /* =========================================================
         * DAA tests (defined: SF/ZF/PF + AF/CF; OF undefined)
         * ========================================================= */

        /* T07: DAA no adjust: AL=0x09, AF=0, CF=0 -> AL=0x09; PF=1 */
        xorl    %eax, %eax
        movb    $0x09, %al
        SET_AF_CF 0, 0
        daa
        SNAP_FLAGS %esi
        ASSERT_AL 0x09, 7
        ASSERT_SZP_AF_CF_SAVED %esi, (PF_BIT), 7

        /* T08: DAA low adjust: AL=0x0B, AF=0, CF=0 -> AL=0x11; AF=1; PF=1 */
        xorl    %eax, %eax
        movb    $0x0b, %al
        SET_AF_CF 0, 0
        daa
        SNAP_FLAGS %esi
        ASSERT_AL 0x11, 8
        ASSERT_SZP_AF_CF_SAVED %esi, (PF_BIT|AF_BIT), 8

        /* T09: DAA both adjusts: AL=0x9A, AF=0, CF=0 -> AL=0x00; AF=1; CF=1; ZF=1; PF=1 */
        xorl    %eax, %eax
        movb    $0x9a, %al
        SET_AF_CF 0, 0
        daa
        SNAP_FLAGS %esi
        ASSERT_AL 0x00, 9
        ASSERT_SZP_AF_CF_SAVED %esi, (ZF_BIT|PF_BIT|AF_BIT|CF_BIT), 9  /* 0x55 */

        /* T10: DAA with incoming CF=1 forces 0x60 adjust:
         *   AL=0x15, AF=0, CF=1 -> low no; high yes via CF -> AL=0x75; CF=1; AF=0; PF(0x75)=0
         *   mask = CF = 0x01
         */
        xorl    %eax, %eax
        movb    $0x15, %al
        SET_AF_CF 0, 1
        daa
        SNAP_FLAGS %esi
        ASSERT_AL 0x75, 10
        ASSERT_SZP_AF_CF_SAVED %esi, (CF_BIT), 10

        /* T11: DAA with incoming AF=1 forces +6 even if low nibble <=9:
         *   AL=0x15, AF=1, CF=0 -> AL=0x1B; AF=1; CF=0; PF(0x1B)=1
         *   mask = PF + AF = 0x14
         */
        xorl    %eax, %eax
        movb    $0x15, %al
        SET_AF_CF 1, 0
        daa
        SNAP_FLAGS %esi
        ASSERT_AL 0x1b, 11
        ASSERT_SZP_AF_CF_SAVED %esi, (PF_BIT|AF_BIT), 11

        /* T12: DAA with incoming AF=1 and CF=1 forces both:
         *   AL=0x15 -> +6 => 0x1B (AF=1), then +0x60 via CF => 0x7B (CF=1)
         *   PF(0x7B)=1; mask = PF+AF+CF = 0x15
         */
        xorl    %eax, %eax
        movb    $0x15, %al
        SET_AF_CF 1, 1
        daa
        SNAP_FLAGS %esi
        ASSERT_AL 0x7b, 12
        ASSERT_SZP_AF_CF_SAVED %esi, (PF_BIT|AF_BIT|CF_BIT), 12


        /* =========================================================
         * DAS tests (defined: SF/ZF/PF + AF/CF; OF undefined)
         * ========================================================= */

        /* T13: DAS no adjust: AL=0x09, AF=0, CF=0 -> AL=0x09; PF=1 */
        xorl    %eax, %eax
        movb    $0x09, %al
        SET_AF_CF 0, 0
        das
        SNAP_FLAGS %esi
        ASSERT_AL 0x09, 13
        ASSERT_SZP_AF_CF_SAVED %esi, (PF_BIT), 13

        /* T14: DAS low adjust: AL=0x0B, AF=0, CF=0 -> AL=0x05; AF=1; PF(0x05)=1 => 0x14 */
        xorl    %eax, %eax
        movb    $0x0b, %al
        SET_AF_CF 0, 0
        das
        SNAP_FLAGS %esi
        ASSERT_AL 0x05, 14
        ASSERT_SZP_AF_CF_SAVED %esi, (PF_BIT|AF_BIT), 14

        /* T15: DAS both adjusts: AL=0x9A, AF=0, CF=0 -> AL=0x34; AF=1; CF=1; PF(0x34)=0 => 0x11 */
        xorl    %eax, %eax
        movb    $0x9a, %al
        SET_AF_CF 0, 0
        das
        SNAP_FLAGS %esi
        ASSERT_AL 0x34, 15
        ASSERT_SZP_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 15

        /* T16: DAS with incoming CF=1 forces 0x60 subtract:
         *   AL=0x75, AF=0, CF=1 -> low no; high yes via CF -> AL=0x15; CF=1; AF=0; PF(0x15)=0
         *   mask = CF = 0x01
         */
        xorl    %eax, %eax
        movb    $0x75, %al
        SET_AF_CF 0, 1
        das
        SNAP_FLAGS %esi
        ASSERT_AL 0x15, 16
        ASSERT_SZP_AF_CF_SAVED %esi, (CF_BIT), 16

        /* T17: DAS with incoming AF=1 forces -6 even if low nibble <=9:
         *   AL=0x1B, AF=1, CF=0 -> AL=0x15; AF=1; CF=0; PF(0x15)=0
         *   mask = AF = 0x10
         */
        xorl    %eax, %eax
        movb    $0x1b, %al
        SET_AF_CF 1, 0
        das
        SNAP_FLAGS %esi
        ASSERT_AL 0x15, 17
        ASSERT_SZP_AF_CF_SAVED %esi, (AF_BIT), 17

        /* T18: DAS with incoming AF=1 and CF=1 forces both:
         *   AL=0x7B, AF=1, CF=1 -> -6 => 0x75 (AF=1), then -0x60 via CF => 0x15 (CF=1)
         *   PF(0x15)=0; mask = AF+CF = 0x11
         */
        xorl    %eax, %eax
        movb    $0x7b, %al
        SET_AF_CF 1, 1
        das
        SNAP_FLAGS %esi
        ASSERT_AL 0x15, 18
        ASSERT_SZP_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 18


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
success_msg:
        .ascii "success\n"
        