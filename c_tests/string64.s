/* string64.s -- x86-64 Linux GAS tests: CMPS/LODS/SCAS (various widths + REP + DF=1)
 *
 * Build:
 *   as -o string64.o string64.s
 *   ld -o string64 string64.o
 *
 * Run:
 *   ./string64
 *   echo $?
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

        /* Validate only these flags (PF/AF are messy) */
        .set CF_BIT, 0x0000000000000001
        .set ZF_BIT, 0x0000000000000040
        .set SF_BIT, 0x0000000000000080
        .set OF_BIT, 0x0000000000000800
        .set FLAG_MASK, (CF_BIT|ZF_BIT|SF_BIT|OF_BIT)

.macro FAIL code
        mov     $SYS_exit, %rax
        mov     $\code, %rdi
        syscall
.endm

.macro SNAP_RFLAGS reg
        pushfq
        popq    \reg
.endm

.macro ASSERT_EQ64 reg, imm64, code
        movabs  $\imm64, %r11
        cmpq    %r11, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ32 reg, imm32, code
        cmpl    $\imm32, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ16 reg16, imm16, code
        cmpw    $\imm16, \reg16
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ8 reg8, imm8, code
        cmpb    $\imm8, \reg8
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_PTR reg, label, code
        lea     \label(%rip), %r11
        cmpq    %r11, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* Assert (saved_flags & FLAG_MASK) == expected */
.macro ASSERT_FLAGS saved, expected, code
        movq    \saved, %r10
        andq    $FLAG_MASK, %r10
        movabs  $\expected, %r11
        cmpq    %r11, %r10
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* Assert LODS doesn't change flags in FLAG_MASK */
.macro ASSERT_FLAGS_UNCHANGED before, after, code
        movq    \before, %r10
        xorq    \after, %r10
        andq    $FLAG_MASK, %r10
        testq   %r10, %r10
        je      1f
        FAIL \code
1:
.endm


_start:
        /* -----------------------------
         * CMPS single-step, DF=0
         * ----------------------------- */

        /* T01: cmpsb equal -> ZF=1; RSI/RDI +1 */
        cld
        lea     cmpsb_eq_a(%rip), %rsi
        lea     cmpsb_eq_b(%rip), %rdi
        cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 1
        ASSERT_PTR %rsi, cmpsb_eq_a_end, 1
        ASSERT_PTR %rdi, cmpsb_eq_b_end, 1

        /* T02: cmpsb 0x01-0x02 -> CF=1,SF=1 */
        cld
        lea     cmpsb_lt_a(%rip), %rsi
        lea     cmpsb_lt_b(%rip), %rdi
        cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT), 2
        ASSERT_PTR %rsi, cmpsb_lt_a_end, 2
        ASSERT_PTR %rdi, cmpsb_lt_b_end, 2

        /* T03: cmpsb 0x80-0x01 -> OF=1 */
        cld
        lea     cmpsb_of_a(%rip), %rsi
        lea     cmpsb_of_b(%rip), %rdi
        cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, OF_BIT, 3
        ASSERT_PTR %rsi, cmpsb_of_a_end, 3
        ASSERT_PTR %rdi, cmpsb_of_b_end, 3

        /* T04: cmpsw equal -> ZF=1; +2 */
        cld
        lea     cmpsw_eq_a(%rip), %rsi
        lea     cmpsw_eq_b(%rip), %rdi
        cmpsw
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 4
        ASSERT_PTR %rsi, cmpsw_eq_a_end, 4
        ASSERT_PTR %rdi, cmpsw_eq_b_end, 4

        /* T05: cmpsl 1-2 -> CF=1,SF=1; +4 */
        cld
        lea     cmpsl_lt_a(%rip), %rsi
        lea     cmpsl_lt_b(%rip), %rdi
        cmpsl
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT), 5
        ASSERT_PTR %rsi, cmpsl_lt_a_end, 5
        ASSERT_PTR %rdi, cmpsl_lt_b_end, 5

        /* T06: cmpsq equal -> ZF=1; +8 */
        cld
        lea     cmpsq_eq_a(%rip), %rsi
        lea     cmpsq_eq_b(%rip), %rdi
        cmpsq
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 6
        ASSERT_PTR %rsi, cmpsq_eq_a_end, 6
        ASSERT_PTR %rdi, cmpsq_eq_b_end, 6

        /* OF=1 at 16/32/64 */
        /* T07: cmpsw 0x8000-1 -> OF=1 */
        cld
        lea     cmpsw_of_a(%rip), %rsi
        lea     cmpsw_of_b(%rip), %rdi
        cmpsw
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, OF_BIT, 7

        /* T08: cmpsl 0x80000000-1 -> OF=1 */
        cld
        lea     cmpsl_of_a(%rip), %rsi
        lea     cmpsl_of_b(%rip), %rdi
        cmpsl
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, OF_BIT, 8

        /* T09: cmpsq 0x8000..0000-1 -> OF=1 */
        cld
        lea     cmpsq_of_a(%rip), %rsi
        lea     cmpsq_of_b(%rip), %rdi
        cmpsq
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, OF_BIT, 9


        /* -----------------------------
         * CMPS single-step, DF=1
         * ----------------------------- */

        /* T10: cmpsw DF=1 equal: pointers -2; ZF=1 */
        std
        lea     cmpsw_df_a_last(%rip), %rsi
        lea     cmpsw_df_b_last(%rip), %rdi
        cmpsw
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 10
        ASSERT_PTR %rsi, cmpsw_df_a_first, 10
        ASSERT_PTR %rdi, cmpsw_df_b_first, 10
        cld


        /* -----------------------------
         * LODS (flags unchanged), DF=0 and DF=1
         * ----------------------------- */

        /* T11: lodsb DF=0 loads AL; RSI +1; flags unchanged */
        cld
        mov     $1, %eax
        sub     $2, %eax
        SNAP_RFLAGS %r8
        lea     lodsb_src(%rip), %rsi
        lodsb
        SNAP_RFLAGS %r9
        ASSERT_FLAGS_UNCHANGED %r8, %r9, 11
        ASSERT_EQ8  %al, 0x5A, 11
        ASSERT_PTR %rsi, lodsb_src_end, 11

        /* T12: lodsw DF=0 loads AX; RSI +2 */
        mov     $1, %eax
        sub     $2, %eax
        SNAP_RFLAGS %r8
        lea     lodsw_src(%rip), %rsi
        lodsw
        SNAP_RFLAGS %r9
        ASSERT_FLAGS_UNCHANGED %r8, %r9, 12
        ASSERT_EQ16 %ax, 0xB00B, 12
        ASSERT_PTR %rsi, lodsw_src_end, 12

        /* T13: lodsl DF=0 loads EAX; RSI +4 */
        mov     $1, %eax
        sub     $2, %eax
        SNAP_RFLAGS %r8
        lea     lodsl_src(%rip), %rsi
        lodsl
        SNAP_RFLAGS %r9
        ASSERT_FLAGS_UNCHANGED %r8, %r9, 13
        ASSERT_EQ32 %eax, 0x89ABCDEF, 13
        ASSERT_PTR %rsi, lodsl_src_end, 13

        /* T14: lodsq DF=1 loads RAX; RSI -8 */
        std
        mov     $1, %eax
        sub     $2, %eax
        SNAP_RFLAGS %r8
        lea     lodsq_val(%rip), %rsi
        lodsq
        SNAP_RFLAGS %r9
        ASSERT_FLAGS_UNCHANGED %r8, %r9, 14
        ASSERT_EQ64 %rax, 0x0123456789ABCDEF, 14
        ASSERT_PTR %rsi, lodsq_prev, 14
        cld


        /* -----------------------------
         * SCAS single-step, DF=0 and DF=1
         * ----------------------------- */

        /* T15: scasb equal -> ZF=1; RDI +1 */
        cld
        movb    $0x33, %al
        lea     scasb_eq(%rip), %rdi
        scasb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 15
        ASSERT_PTR %rdi, scasb_eq_end, 15

        /* T16: scasw 1-2 -> CF=1,SF=1; RDI +2 */
        cld
        movw    $0x0001, %ax
        lea     scasw_lt(%rip), %rdi
        scasw
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT), 16
        ASSERT_PTR %rdi, scasw_lt_end, 16

        /* T17: scasl 0x80000000 - 1 -> OF=1; RDI +4 */
        cld
        movl    $0x80000000, %eax
        lea     scasl_of(%rip), %rdi
        scasl
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, OF_BIT, 17
        ASSERT_PTR %rdi, scasl_of_end, 17

        /* T18: scasq equal DF=1: RDI -8; ZF=1 */
        std
        movabs  $0x1122334455667788, %rax
        lea     scasq_val(%rip), %rdi
        scasq
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 18
        ASSERT_PTR %rdi, scasq_prev, 18
        cld

        /* Extra OF=1: scasw/scasq */
        /* T19: scasw OF: 0x8000 - 1 -> OF=1 */
        cld
        movw    $0x8000, %ax
        lea     scasw_of(%rip), %rdi
        scasw
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, OF_BIT, 19

        /* T20: scasq OF: 0x8000..0000 - 1 -> OF=1 */
        cld
        movabs  $0x8000000000000000, %rax
        lea     scasq_of(%rip), %rdi
        scasq
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, OF_BIT, 20


        /* -----------------------------
         * REP tests (DF=0)
         * ----------------------------- */

        /* T21: REPE CMPSB all equal len=4 -> RCX=0, ZF=1, RSI/RDI +4 */
        cld
        lea     rep_eq_a(%rip), %rsi
        lea     rep_eq_b(%rip), %rdi
        mov     $4, %rcx
        repe cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 21
        ASSERT_EQ64 %rcx, 0, 21
        ASSERT_PTR %rsi, rep_eq_a_end, 21
        ASSERT_PTR %rdi, rep_eq_b_end, 21

        /* T22: REPE CMPSB mismatch at 1st "X..." vs "Y..." -> RCX=3, +1, flags CF|SF */
        cld
        lea     rep_m1_a(%rip), %rsi
        lea     rep_m1_b(%rip), %rdi
        mov     $4, %rcx
        repe cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT), 22
        ASSERT_EQ64 %rcx, 3, 22
        ASSERT_PTR %rsi, rep_m1_a_p1, 22
        ASSERT_PTR %rdi, rep_m1_b_p1, 22

        /* T23: REPE CMPSB mismatch at 2nd -> RCX=2, +2, flags CF|SF */
        cld
        lea     rep_m2_a(%rip), %rsi
        lea     rep_m2_b(%rip), %rdi
        mov     $4, %rcx
        repe cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT), 23
        ASSERT_EQ64 %rcx, 2, 23
        ASSERT_PTR %rsi, rep_m2_a_p2, 23
        ASSERT_PTR %rdi, rep_m2_b_p2, 23

        /* T24: REPE CMPSB mismatch at last -> RCX=0, +4, flags CF|SF */
        cld
        lea     rep_mlast_a(%rip), %rsi
        lea     rep_mlast_b(%rip), %rdi
        mov     $4, %rcx
        repe cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT), 24
        ASSERT_EQ64 %rcx, 0, 24
        ASSERT_PTR %rsi, rep_mlast_a_end, 24
        ASSERT_PTR %rdi, rep_mlast_b_end, 24

        /* T25: REPNE SCASB find 'd' in "abcde" -> RCX=1, RDI +4, ZF=1 */
        cld
        movb    $'d', %al
        lea     rep_scas_mid(%rip), %rdi
        mov     $5, %rcx
        repne scasb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 25
        ASSERT_EQ64 %rcx, 1, 25
        ASSERT_PTR %rdi, rep_scas_mid_p4, 25

        /* T26: REPNE SCASB find 'a' early -> RCX=4, RDI +1, ZF=1 */
        cld
        movb    $'a', %al
        lea     rep_scas_early(%rip), %rdi
        mov     $5, %rcx
        repne scasb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 26
        ASSERT_EQ64 %rcx, 4, 26
        ASSERT_PTR %rdi, rep_scas_early_p1, 26

        /* T27: REPNE SCASB no match ('z' in "aaaaa") -> RCX=0, RDI +5, ZF=0 */
        cld
        movb    $'z', %al
        lea     rep_scas_nomatch(%rip), %rdi
        mov     $5, %rcx
        repne scasb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, 0, 27
        ASSERT_EQ64 %rcx, 0, 27
        ASSERT_PTR %rdi, rep_scas_nomatch_end, 27


        /* -----------------------------
         * REP tests (DF=1) reverse
         * ----------------------------- */

        /* T28: REPE CMPSB backward mismatch on first compare (last chars differ)
         * "abcz" vs "abcy", start at last. z-y positive => mask=0, RCX=3, RSI/RDI -1
         */
        std
        lea     rep_bcmps_a_last(%rip), %rsi
        lea     rep_bcmps_b_last(%rip), %rdi
        mov     $4, %rcx
        repe cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, 0, 28
        ASSERT_EQ64 %rcx, 3, 28
        ASSERT_PTR %rsi, rep_bcmps_a_p2, 28
        ASSERT_PTR %rdi, rep_bcmps_b_p2, 28
        cld

        /* T29: REPNE SCASB backward find 'b' in "abcde"
         * start at 'e' (last), iterations=4 => RCX=1, and RDI becomes base ('a')
         */
        std
        movb    $'b', %al
        lea     rep_bscas_last(%rip), %rdi
        mov     $5, %rcx
        repne scasb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 29
        ASSERT_EQ64 %rcx, 1, 29
        ASSERT_PTR %rdi, rep_bscas_base, 29
        cld


        /* -----------------------------
         * Added: REPNE CMPSB (DF=0)
         * ----------------------------- */

        /* T30: REPNE CMPSB mismatch then match:
         * "xbcde" vs "ybcde": first mismatch continues, second equal stops -> ZF=1
         * iterations=2 => RCX=3, RSI/RDI +2
         */
        cld
        lea     repne_cmps_match_a(%rip), %rsi
        lea     repne_cmps_match_b(%rip), %rdi
        mov     $5, %rcx
        repne cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, ZF_BIT, 30
        ASSERT_EQ64 %rcx, 3, 30
        ASSERT_PTR %rsi, repne_cmps_match_a_p2, 30
        ASSERT_PTR %rdi, repne_cmps_match_b_p2, 30

        /* T31: REPNE CMPSB no match (all mismatches):
         * "xxxxx" vs "yyyyy": runs out -> RCX=0; last compare x-y => CF|SF
         */
        cld
        lea     repne_cmps_nomatch_a(%rip), %rsi
        lea     repne_cmps_nomatch_b(%rip), %rdi
        mov     $5, %rcx
        repne cmpsb
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT), 31
        ASSERT_EQ64 %rcx, 0, 31
        ASSERT_PTR %rsi, repne_cmps_nomatch_a_end, 31
        ASSERT_PTR %rdi, repne_cmps_nomatch_b_end, 31


        /* -----------------------------
         * Added: DF=1 mixed-width REPE CMPSW / CMPSL
         * ----------------------------- */

        /* T32: DF=1 REPE CMPSW, 3 words, mismatch in middle
         * Start at last (equal), then middle mismatch stops.
         * iterations=2 => RCX=1; RSI/RDI now at first word.
         */
        std
        lea     rep_bw_cmps_a_w2(%rip), %rsi
        lea     rep_bw_cmps_b_w2(%rip), %rdi
        mov     $3, %rcx
        repe cmpsw
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT|OF_BIT), 32
        ASSERT_EQ64 %rcx, 1, 32
        ASSERT_PTR %rsi, rep_bw_cmps_a_w0, 32
        ASSERT_PTR %rdi, rep_bw_cmps_b_w0, 32
        cld

        /* T33: DF=1 REPE CMPSL, 3 dwords, mismatch in middle
         * Start at last (equal), then middle mismatch stops.
         * iterations=2 => RCX=1; RSI/RDI now at first dword.
         */
        std
        lea     rep_bl_cmps_a_d2(%rip), %rsi
        lea     rep_bl_cmps_b_d2(%rip), %rdi
        mov     $3, %rcx
        repe cmpsl
        SNAP_RFLAGS %r12
        ASSERT_FLAGS %r12, (CF_BIT|SF_BIT), 33
        ASSERT_EQ64 %rcx, 1, 33
        ASSERT_PTR %rsi, rep_bl_cmps_a_d0, 33
        ASSERT_PTR %rdi, rep_bl_cmps_b_d0, 33
        cld


        /* success */
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     success_msg(%rip), %rsi
        mov     $8, %rdx
        syscall

        mov     $SYS_exit, %rax
        xor     %rdi, %rdi
        syscall


        .section .rodata
success_msg:
        .ascii "success\n"

        /* --- CMPS data --- */
cmpsb_eq_a:     .byte 0x12
cmpsb_eq_a_end:
cmpsb_eq_b:     .byte 0x12
cmpsb_eq_b_end:

cmpsb_lt_a:     .byte 0x01
cmpsb_lt_a_end:
cmpsb_lt_b:     .byte 0x02
cmpsb_lt_b_end:

cmpsb_of_a:     .byte 0x80
cmpsb_of_a_end:
cmpsb_of_b:     .byte 0x01
cmpsb_of_b_end:

cmpsw_eq_a:     .word 0xBEEF
cmpsw_eq_a_end:
cmpsw_eq_b:     .word 0xBEEF
cmpsw_eq_b_end:

cmpsl_lt_a:     .long 0x00000001
cmpsl_lt_a_end:
cmpsl_lt_b:     .long 0x00000002
cmpsl_lt_b_end:

cmpsq_eq_a:     .quad 0x0123456789ABCDEF
cmpsq_eq_a_end:
cmpsq_eq_b:     .quad 0x0123456789ABCDEF
cmpsq_eq_b_end:

cmpsw_of_a:     .word 0x8000
cmpsw_of_a_end:
cmpsw_of_b:     .word 0x0001
cmpsw_of_b_end:

cmpsl_of_a:     .long 0x80000000
cmpsl_of_a_end:
cmpsl_of_b:     .long 0x00000001
cmpsl_of_b_end:

cmpsq_of_a:     .quad 0x8000000000000000
cmpsq_of_a_end:
cmpsq_of_b:     .quad 0x0000000000000001
cmpsq_of_b_end:

        /* DF=1 CMPSW pair: first then last */
cmpsw_df_a_first:   .word 0xCAFE
cmpsw_df_a_last:    .word 0xBABE
cmpsw_df_b_first:   .word 0xCAFE
cmpsw_df_b_last:    .word 0xBABE

        /* --- LODS sources --- */
lodsb_src:      .byte 0x5A
lodsb_src_end:
lodsw_src:      .word 0xB00B
lodsw_src_end:
lodsl_src:      .long 0x89ABCDEF
lodsl_src_end:
lodsq_prev:     .quad 0x0
lodsq_val:      .quad 0x0123456789ABCDEF

        /* --- SCAS data --- */
scasb_eq:       .byte 0x33
scasb_eq_end:
scasw_lt:       .word 0x0002
scasw_lt_end:
scasl_of:       .long 0x00000001
scasl_of_end:
scasq_prev:     .quad 0x0
scasq_val:      .quad 0x1122334455667788
scasw_of:       .word 0x0001
scasw_of_end:
scasq_of:       .quad 0x0000000000000001
scasq_of_end:

        /* --- REPE CMPSB forward --- */
rep_eq_a:       .byte 'A','B','C','D'
rep_eq_a_end:
rep_eq_b:       .byte 'A','B','C','D'
rep_eq_b_end:

/* T22: mismatch at 1st, need label at +1 */
rep_m1_a:       .byte 'X'
rep_m1_a_p1:    .byte 'a','b','c'
rep_m1_a_end:
rep_m1_b:       .byte 'Y'
rep_m1_b_p1:    .byte 'a','b','c'
rep_m1_b_end:

/* T23: mismatch at 2nd, need label at +2 */
rep_m2_a:       .byte 'a','X'
rep_m2_a_p2:    .byte 'b','c'
rep_m2_a_end:
rep_m2_b:       .byte 'a','Y'
rep_m2_b_p2:    .byte 'b','c'
rep_m2_b_end:

rep_mlast_a:    .byte 'a','b','c','X'
rep_mlast_a_end:
rep_mlast_b:    .byte 'a','b','c','Y'
rep_mlast_b_end:


        /* --- REPNE SCASB forward --- */
/* T25: label at +4 (past 'd') */
rep_scas_mid:       .byte 'a','b','c','d'
rep_scas_mid_p4:    .byte 'e'
rep_scas_mid_end:

/* T26: label at +1 */
rep_scas_early:     .byte 'a'
rep_scas_early_p1:  .byte 'b','c','d','e'
rep_scas_early_end:

rep_scas_nomatch:   .byte 'a','a','a','a','a'
rep_scas_nomatch_end:


        /* --- REPE CMPSB backward --- */
/* After one DF=1 cmpsb starting at last, RSI/RDI become base+2 (the 'c') */
rep_bcmps_a:        .byte 'a','b'
rep_bcmps_a_p2:     .byte 'c'
rep_bcmps_a_last:   .byte 'z'
rep_bcmps_a_end:

rep_bcmps_b:        .byte 'a','b'
rep_bcmps_b_p2:     .byte 'c'
rep_bcmps_b_last:   .byte 'y'
rep_bcmps_b_end:


        /* --- REPNE SCASB backward --- */
rep_bscas_base:     .byte 'a','b','c','d'
rep_bscas_last:     .byte 'e'
rep_bscas_end:


        /* --- REPNE CMPSB forward --- */
/* T30: stop after 2 compares, need label at +2 */
repne_cmps_match_a:     .byte 'x','b'
repne_cmps_match_a_p2:  .byte 'c','d','e'
repne_cmps_match_a_end:
repne_cmps_match_b:     .byte 'y','b'
repne_cmps_match_b_p2:  .byte 'c','d','e'
repne_cmps_match_b_end:

repne_cmps_nomatch_a:   .byte 'x','x','x','x','x'
repne_cmps_nomatch_a_end:
repne_cmps_nomatch_b:   .byte 'y','y','y','y','y'
repne_cmps_nomatch_b_end:
        /* --- DF=1 REPE CMPSW --- */
rep_bw_cmps_a_w0:   .word 0x1111
rep_bw_cmps_a_w1:   .word 0x2222
rep_bw_cmps_a_w2:   .word 0x3333

rep_bw_cmps_b_w0:   .word 0x1111
rep_bw_cmps_b_w1:   .word 0x9999
rep_bw_cmps_b_w2:   .word 0x3333

        /* --- DF=1 REPE CMPSL --- */
rep_bl_cmps_a_d0:   .long 0xA5A5A5A5
rep_bl_cmps_a_d1:   .long 0x00000001
rep_bl_cmps_a_d2:   .long 0x5A5A5A5A

rep_bl_cmps_b_d0:   .long 0xA5A5A5A5
rep_bl_cmps_b_d1:   .long 0x00000002
rep_bl_cmps_b_d2:   .long 0x5A5A5A5A
