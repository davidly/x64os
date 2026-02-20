/* xlat64.s  --  x86-64 Linux GAS test: XLAT / XLATB
 *
 * Build:
 *   as -o xlat64.o xlat64.s
 *   ld -o xlat64 xlat64.o
 *
 * Run:
 *   ./xlat64
 *   echo $?
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

        /* Status flags mask: CF(0) PF(2) AF(4) ZF(6) SF(7) OF(11) */
        .set STATUS_MASK, 0x00000000000008D5

/* -------- helpers -------- */
.macro FAIL code
        mov     $SYS_exit, %rax
        mov     $\code, %rdi
        syscall
.endm

.macro ASSERT_AL imm, code
        cmp     $\imm, %al
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* Snapshot EFLAGS into REG (64-bit) */
.macro SNAP_RFLAGS reg
        pushfq
        popq    \reg
.endm

/* Store masked status flags in REG without changing live flags:
 *   - snapshots RFLAGS into REG
 *   - masks into REG
 *   - restores original live RFLAGS
 * Clobbers: r11
 */
.macro SNAP_STATUS_MASKED_RESTORE reg
        pushfq
        popq    \reg              /* \reg = original RFLAGS */
        pushq   \reg              /* save original for restore */
        movq    \reg, %r11
        andq    $STATUS_MASK, %r11
        movq    %r11, \reg         /* \reg = masked */
        popfq                        /* restore original live RFLAGS */
.endm

.macro ASSERT_REG_EQ r1, r2, code
        cmpq    \r2, \r1
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        /* RBX = base of translation table (XLAT uses (R)BX + AL) */
        lea     xlat_table(%rip), %rbx

        /* ----------------------------
         * T01: xlatb AL=0x00 -> table[0]=0xA5, flags unchanged
         * ---------------------------- */
        mov     $1, %eax
        sub     $2, %eax           /* establish nontrivial flags */
        SNAP_STATUS_MASKED_RESTORE %r12   /* before masked (and restore flags) */

        mov     $0x00, %al
        xlatb
        SNAP_STATUS_MASKED_RESTORE %r13   /* after masked (and restore flags) */

        ASSERT_REG_EQ %r12, %r13, 1
        ASSERT_AL 0xA5, 1

        /* ----------------------------
         * T02: xlat AL=0x7F -> 0x7F^0xA5=0xDA, flags unchanged
         * ---------------------------- */
        mov     $1, %eax
        sub     $2, %eax
        SNAP_STATUS_MASKED_RESTORE %r12

        mov     $0x7f, %al
        xlat
        SNAP_STATUS_MASKED_RESTORE %r13

        ASSERT_REG_EQ %r12, %r13, 2
        ASSERT_AL 0xDA, 2

        /* ----------------------------
         * T03: xlatb AL=0xFF -> 0xFF^0xA5=0x5A, flags unchanged
         * ---------------------------- */
        mov     $1, %eax
        sub     $2, %eax
        SNAP_STATUS_MASKED_RESTORE %r12

        mov     $0xff, %al
        xlatb
        SNAP_STATUS_MASKED_RESTORE %r13

        ASSERT_REG_EQ %r12, %r13, 3
        ASSERT_AL 0x5A, 3

        /* ----------------------------
         * T04: offset base: RBX += 16, AL=3 -> original index 0x13 -> 0x13^0xA5=0xB6
         * ---------------------------- */
        lea     xlat_table(%rip), %rbx
        add     $16, %rbx

        mov     $1, %eax
        sub     $2, %eax
        SNAP_STATUS_MASKED_RESTORE %r12

        mov     $0x03, %al
        xlat
        SNAP_STATUS_MASKED_RESTORE %r13

        ASSERT_REG_EQ %r12, %r13, 4
        ASSERT_AL 0xB6, 4

        /* success: write(1,"success\n",8) */
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     success_msg(%rip), %rsi
        mov     $8, %rdx
        syscall

        /* exit(0) */
        mov     $SYS_exit, %rax
        xor     %rdi, %rdi
        syscall


        .section .rodata
        .balign 16
success_msg:
        .ascii  "success\n"

/* table[i] = i ^ 0xA5 */
xlat_table:
        .set i, 0
        .rept 256
        .byte (i ^ 0xA5)
        .set i, i+1
        .endr
        