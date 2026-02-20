/* aaa_aas32.s  --  32-bit Linux (i386) GAS tests: AAA and AAS
 *
 * Build:
 *   as --32 -o aaa_aas32.o aaa_aas32.s
 *   ld -m elf_i386 -o aaa_aas32 aaa_aas32.o
 *
 * Run:
 *   ./aaa_aas32
 *   echo $?
 */

        .section .text
        .code32
        .globl _start

        .set SYS_exit,  1
        .set SYS_write, 4

        /* EFLAGS bits */
        .set CF_BIT, 0x00000001
        .set AF_BIT, 0x00000010
        .set AF_CF_MASK, (CF_BIT | AF_BIT)

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

/* Snapshot EFLAGS immediately after instruction into reg */
.macro SNAP_FLAGS reg
        pushfl
        popl    \reg
.endm

/* Assert AF/CF bits in saved flags equal expected mask value (0, CF, AF, or AF|CF) */
.macro ASSERT_AF_CF_SAVED flagsreg, expected_mask, code
        movl    \flagsreg, %ecx
        andl    $AF_CF_MASK, %ecx
        cmpl    $\expected_mask, %ecx
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        /* =========================
         * AAA tests
         *
         * Behavior summary:
         *   If ( (AL & 0x0F) > 9 ) or (AF=1):
         *       AL += 6; AH += 1; AF=1; CF=1
         *   else:
         *       AF=0; CF=0
         *   AL = AL & 0x0F
         *
         * We only test AL/AH + AF/CF (defined). Other flags undefined.
         * ========================= */

        /* T01: no adjust: AL=0x05, AF=0 -> AL=0x05, AH unchanged, AF=CF=0 */
        xorl    %eax, %eax
        movb    $0x12, %ah          /* sentinel */
        movb    $0x05, %al
        clc                         /* CF=0 */
        /* ensure AF=0: do a harmless OR that clears AF? OR sets AF undefined.
           Better: clear flags via SAHF using AH, but we don't want dependencies.
           We'll instead rely on condition (AL&0xF)<=9 and AF=0; set AF=0 using LAHF/SAHF trick. */
        /* Set AF=0,CF=0 deterministically: */
        pushfl
        popl    %edx
        andl    $~AF_CF_MASK, %edx  /* clear AF/CF in saved image */
        pushl   %edx
        popfl

        aaa
        SNAP_FLAGS %esi
        ASSERT_AH 0x12, 1
        ASSERT_AL 0x05, 1
        ASSERT_AF_CF_SAVED %esi, 0x00, 1

        /* T02: adjust because low nibble > 9: AL=0x0B -> AL becomes 0x01, AH++, AF=CF=1 */
        xorl    %eax, %eax
        movb    $0x10, %ah
        movb    $0x0b, %al
        /* AF=0,CF=0 */
        pushfl
        popl    %edx
        andl    $~AF_CF_MASK, %edx
        pushl   %edx
        popfl

        aaa
        SNAP_FLAGS %esi
        ASSERT_AH 0x11, 2          /* 0x10 + 1 */
        ASSERT_AL 0x01, 2          /* (0x0B+6)=0x11, then &0x0F -> 0x01 */
        ASSERT_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 2

        /* T03: adjust because AF=1 even though low nibble <=9: AL=0x08 with AF preset -> adjust */
        xorl    %eax, %eax
        movb    $0x20, %ah
        movb    $0x08, %al
        /* Set AF=1, CF=0 deterministically via flags image */
        pushfl
        popl    %edx
        andl    $~AF_CF_MASK, %edx
        orl     $AF_BIT, %edx
        pushl   %edx
        popfl

        aaa
        SNAP_FLAGS %esi
        ASSERT_AH 0x21, 3          /* incremented */
        ASSERT_AL 0x0e, 3          /* 0x08+6=0x0E, &0x0F -> 0x0E */
        ASSERT_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 3


        /* =========================
         * AAS tests
         *
         * Behavior summary:
         *   If ( (AL & 0x0F) > 9 ) or (AF=1):
         *       AL -= 6; AH -= 1; AF=1; CF=1
         *   else:
         *       AF=0; CF=0
         *   AL = AL & 0x0F
         * ========================= */

        /* T04: no adjust: AL=0x05, AF=0 -> AL=0x05, AH unchanged, AF=CF=0 */
        xorl    %eax, %eax
        movb    $0x30, %ah
        movb    $0x05, %al
        /* AF=0,CF=0 */
        pushfl
        popl    %edx
        andl    $~AF_CF_MASK, %edx
        pushl   %edx
        popfl

        aas
        SNAP_FLAGS %esi
        ASSERT_AH 0x30, 4
        ASSERT_AL 0x05, 4
        ASSERT_AF_CF_SAVED %esi, 0x00, 4

        /* T05: adjust because low nibble > 9: AL=0x0B -> AL becomes 0x05, AH--, AF=CF=1 */
        xorl    %eax, %eax
        movb    $0x40, %ah
        movb    $0x0b, %al
        /* AF=0,CF=0 */
        pushfl
        popl    %edx
        andl    $~AF_CF_MASK, %edx
        pushl   %edx
        popfl

        aas
        SNAP_FLAGS %esi
        ASSERT_AH 0x3f, 5          /* 0x40 - 1 */
        ASSERT_AL 0x05, 5          /* (0x0B-6)=0x05, &0x0F -> 0x05 */
        ASSERT_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 5

        /* T06: adjust because AF=1 even though low nibble <=9: AL=0x08 with AF preset -> adjust */
        xorl    %eax, %eax
        movb    $0x50, %ah
        movb    $0x08, %al
        /* Set AF=1, CF=0 */
        pushfl
        popl    %edx
        andl    $~AF_CF_MASK, %edx
        orl     $AF_BIT, %edx
        pushl   %edx
        popfl

        aas
        SNAP_FLAGS %esi
        ASSERT_AH 0x4f, 6          /* decremented */
        ASSERT_AL 0x02, 6          /* 0x08-6=0x02, &0x0F -> 0x02 */
        ASSERT_AF_CF_SAVED %esi, (AF_BIT|CF_BIT), 6


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
        