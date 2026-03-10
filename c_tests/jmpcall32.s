/* jmpcall32.s -- 32-bit Linux (i386) GAS tests: jmp and call forms
 *
 * Build:
 *   gcc -m32 jmpcall32.s -o jmpcall32.elf -static -nostdlib
 *
 * Run:
 *   ./jmpcall32.elf
 *   echo $?
 */

        .section .text
        .code32
        .globl _start

        .set SYS_exit,  1
        .set SYS_write, 4

/* ---------- helpers ---------- */
.macro FAIL code
        movl    $SYS_exit, %eax
        movl    $\code, %ebx
        int     $0x80
.endm

.macro ASSERT_REG_EQ r1, r2, code
        cmpl    \r2, \r1
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

.macro ASSERT_EQ_IMM reg, imm, code
        cmpl    $\imm, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        lea     stack_space_end, %esp

        /* =========================================================
         * T01: direct near call -- return address and ESP verified
         * =========================================================
         * After call: ESP decremented by 4, [ESP] == address of instruction
         * after the call, ret restores ESP to original value.
         */
        movl    %esp, %esi
        call    t01_target
t01_retaddr:
        jmp     t01_done
t01_target:
        /* ESP must equal saved ESP minus 4 */
        leal    -4(%esi), %edi
        ASSERT_REG_EQ %esp, %edi, 1
        /* [ESP] must be the address of t01_retaddr */
        movl    (%esp), %eax
        movl    $t01_retaddr, %edi
        ASSERT_REG_EQ %eax, %edi, 1
        ret
t01_done:
        /* ESP restored by ret */
        ASSERT_REG_EQ %esp, %esi, 1

        /* =========================================================
         * T02: indirect call via register  (call *%eax)
         * ========================================================= */
        movl    %esp, %esi
        movl    $t02_target, %eax
        call    *%eax
t02_retaddr:
        jmp     t02_done
t02_target:
        leal    -4(%esi), %edi
        ASSERT_REG_EQ %esp, %edi, 2
        movl    (%esp), %ecx
        movl    $t02_retaddr, %edi
        ASSERT_REG_EQ %ecx, %edi, 2
        ret
t02_done:
        ASSERT_REG_EQ %esp, %esi, 2

        /* =========================================================
         * T03: indirect call via memory  (call *funcptr)
         * ========================================================= */
        movl    %esp, %esi
        movl    $t03_target, %eax
        movl    %eax, funcptr
        call    *funcptr
t03_retaddr:
        jmp     t03_done
t03_target:
        leal    -4(%esi), %edi
        ASSERT_REG_EQ %esp, %edi, 3
        movl    (%esp), %ecx
        movl    $t03_retaddr, %edi
        ASSERT_REG_EQ %ecx, %edi, 3
        ret
t03_done:
        ASSERT_REG_EQ %esp, %esi, 3

        /* =========================================================
         * T04: short forward jmp (rel8) -- skips dead code
         * ========================================================= */
        movl    $0, %eax
        jmp     t04_fwd
        movl    $1, %eax        /* must NOT execute */
t04_fwd:
        ASSERT_EQ_IMM %eax, 0, 4

        /* =========================================================
         * T05: backward jmp -- loop 5 times
         * ========================================================= */
        movl    $0, %ecx
t05_loop:
        addl    $1, %ecx
        cmpl    $5, %ecx
        jl      t05_loop
        ASSERT_EQ_IMM %ecx, 5, 5

        /* =========================================================
         * T06: near forward jmp (rel32) -- 200-byte nop sled forces rel32 encoding
         * ========================================================= */
        movl    $0, %eax
        jmp     t06_fwd
        .space  200, 0x90       /* nop sled: distance > 127 forces rel32 */
t06_fwd:
        ASSERT_EQ_IMM %eax, 0, 6

        /* =========================================================
         * T07: indirect jmp via register  (jmp *%ebx)
         * ========================================================= */
        movl    $0, %eax
        movl    $t07_target, %ebx
        jmp     *%ebx
        movl    $1, %eax        /* must NOT execute */
t07_target:
        ASSERT_EQ_IMM %eax, 0, 7

        /* =========================================================
         * T08: indirect jmp via memory  (jmp *funcptr)
         * ========================================================= */
        movl    $0, %eax
        movl    $t08_target, %ebx
        movl    %ebx, funcptr
        jmp     *funcptr
        movl    $1, %eax        /* must NOT execute */
t08_target:
        ASSERT_EQ_IMM %eax, 0, 8

        /* =========================================================
         * T09: conditional jumps -- taken and not taken
         * ========================================================= */
        /* JE taken: 0 == 0 */
        xorl    %eax, %eax
        cmpl    $0, %eax
        jne     t09_fail
        jmp     t09_je_ok
t09_fail:
        FAIL    9
t09_je_ok:

        /* JE not taken: 1 != 0 */
        movl    $1, %eax
        cmpl    $0, %eax
        je      t09_fail

        /* JNE taken: 1 != 0 */
        movl    $1, %eax
        cmpl    $0, %eax
        jne     t09_jne_ok
        jmp     t09_fail
t09_jne_ok:

        /* JL taken (signed): -1 < 0 */
        movl    $-1, %eax
        cmpl    $0, %eax
        jl      t09_jl_ok
        jmp     t09_fail
t09_jl_ok:

        /* JL not taken (signed): 1 < 0 is false */
        movl    $1, %eax
        cmpl    $0, %eax
        jl      t09_fail

        /* JG taken (signed): 1 > 0 */
        movl    $1, %eax
        cmpl    $0, %eax
        jg      t09_jg_ok
        jmp     t09_fail
t09_jg_ok:

        /* JG not taken (signed): -1 > 0 is false */
        movl    $-1, %eax
        cmpl    $0, %eax
        jg      t09_fail

        /* JB taken (unsigned): 1 < 2 */
        movl    $1, %eax
        cmpl    $2, %eax
        jb      t09_jb_ok
        jmp     t09_fail
t09_jb_ok:

        /* JB not taken (unsigned): 3 < 2 is false */
        movl    $3, %eax
        cmpl    $2, %eax
        jb      t09_fail

        /* JA taken (unsigned): 3 > 2 */
        movl    $3, %eax
        cmpl    $2, %eax
        ja      t09_ja_ok
        jmp     t09_fail
t09_ja_ok:

        /* JA not taken (unsigned): 1 > 2 is false */
        movl    $1, %eax
        cmpl    $2, %eax
        ja      t09_fail

        /* =========================================================
         * T10: nested calls -- stack balanced after return from outer
         * ========================================================= */
        lea     stack_space_end, %esp
        movl    %esp, %esi
        call    t10_outer
        ASSERT_REG_EQ %esp, %esi, 10
        jmp     t10_done
t10_outer:
        call    t10_inner
        ret
t10_inner:
        ret
t10_done:

        /* =========================================================
         * T11: call/ret with shared subroutine -- called 4 times
         *      counter increments each call; verify final count
         * ========================================================= */
        movl    $0, counter
        call    t11_sub
        call    t11_sub
        call    t11_sub
        call    t11_sub
        movl    counter, %eax
        ASSERT_EQ_IMM %eax, 4, 11
        jmp     t11_done
t11_sub:
        addl    $1, counter
        ret
t11_done:

        /* =========================================================
         * T12: call *(%esp) -- effective address uses ESP as base
         * =========================================================
         * In x86 ModRM, ESP as base always requires a SIB byte even
         * with no index/scale.  The call reads [ESP] for the target
         * before decrementing ESP and pushing the return address.
         */
        lea     stack_space_end, %esp
        movl    %esp, %esi
        movl    $t12_target, %eax
        pushl   %eax                    /* [ESP] = t12_target; ESP = S-4 */
        call    *(%esp)                 /* target = [ESP]; ESP = S-8; [ESP] = retaddr */
t12_retaddr:
        jmp     t12_done
t12_target:
        leal    -8(%esi), %edi
        ASSERT_REG_EQ %esp, %edi, 12
        movl    (%esp), %eax
        movl    $t12_retaddr, %edi
        ASSERT_REG_EQ %eax, %edi, 12
        ret
t12_done:
        addl    $4, %esp                /* discard the pushl %eax from before the call */
        ASSERT_REG_EQ %esp, %esi, 12

        /* =========================================================
         * T13: jmp *(%esp) -- effective address uses ESP as base
         * =========================================================
         * jmp does not push a return address; ESP is unchanged.
         */
        lea     stack_space_end, %esp
        movl    %esp, %esi
        movl    $t13_target, %eax
        pushl   %eax                    /* [ESP] = t13_target; ESP = S-4 */
        jmp     *(%esp)                 /* target = [ESP]; ESP unchanged */
        movl    $1, %ecx               /* must NOT execute */
t13_target:
        leal    -4(%esi), %edi
        ASSERT_REG_EQ %esp, %edi, 13
        popl    %eax                    /* discard the target pointer */
        ASSERT_REG_EQ %esp, %esi, 13

        /* =========================================================
         * T14: call *4(%esp) -- ESP + disp8, still requires SIB
         * =========================================================
         * Stack layout going into call:
         *   [ESP+4] = t14_target   (pushed first, now deeper)
         *   [ESP]   = dummy 0      (pushed second, on top)
         */
        lea     stack_space_end, %esp
        movl    %esp, %esi
        movl    $t14_target, %eax
        pushl   %eax                    /* ESP = S-4;  [ESP] = t14_target */
        pushl   $0                      /* ESP = S-8;  [ESP] = 0, [ESP+4] = t14_target */
        call    *4(%esp)                /* target=[ESP+4]; ESP=S-12; [ESP]=retaddr */
t14_retaddr:
        jmp     t14_done
t14_target:
        leal    -12(%esi), %edi
        ASSERT_REG_EQ %esp, %edi, 14
        movl    (%esp), %eax
        movl    $t14_retaddr, %edi
        ASSERT_REG_EQ %eax, %edi, 14
        ret
t14_done:
        addl    $8, %esp                /* discard both pushes */
        ASSERT_REG_EQ %esp, %esi, 14

        /* success */
        movl    $SYS_write, %eax
        movl    $1, %ebx
        movl    $success_msg, %ecx
        movl    $8, %edx
        int     $0x80

        movl    $SYS_exit, %eax
        xorl    %ebx, %ebx
        int     $0x80


        .section .bss
        .balign 16
stack_space:
        .skip   4096
stack_space_end:

        .balign 4
counter:
        .skip   4

        .section .data
        .balign 4
funcptr:
        .long   0

        .section .rodata
success_msg:
        .ascii  "success\n"
