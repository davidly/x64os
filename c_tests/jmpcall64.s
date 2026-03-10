/* jmpcall64.s -- x86-64 Linux GAS tests: jmp and call forms
 *
 * Build:
 *   gcc jmpcall64.s -o jmpcall64.elf -static -nostdlib
 *
 * Run:
 *   ./jmpcall64.elf
 *   echo $?
 */

        .section .text
        .code64
        .globl _start

        .set SYS_write, 1
        .set SYS_exit,  60

/* ---------- helpers ---------- */
.macro FAIL code
        mov     $SYS_exit, %rax
        mov     $\code, %rdi
        syscall
.endm

.macro ASSERT_REG_EQ r1, r2, code
        cmpq    \r2, \r1
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm

/* NOTE: cmpq $imm64,%reg is not generally encodable; use movabs+cmp */
.macro ASSERT_EQ64 reg, imm64, code
        movabs  $\imm64, %r11
        cmpq    %r11, \reg
        jne     1f
        jmp     2f
1:      FAIL \code
2:
.endm


_start:
        lea     stack_space_end(%rip), %rsp

        /* =========================================================
         * T01: direct near call -- return address and RSP verified
         * =========================================================
         * After call: RSP decremented by 8, [RSP] == address of instruction
         * after the call, ret restores RSP to original value.
         */
        movq    %rsp, %r12
        call    t01_target
t01_retaddr:
        jmp     t01_done
t01_target:
        /* RSP must equal saved RSP minus 8 */
        lea     -8(%r12), %r13
        ASSERT_REG_EQ %rsp, %r13, 1
        /* [RSP] must be the address of t01_retaddr */
        movq    (%rsp), %rax
        lea     t01_retaddr(%rip), %r13
        ASSERT_REG_EQ %rax, %r13, 1
        ret
t01_done:
        /* RSP restored by ret */
        ASSERT_REG_EQ %rsp, %r12, 1

        /* =========================================================
         * T02: indirect call via register  (call *%rax)
         * ========================================================= */
        movq    %rsp, %r12
        lea     t02_target(%rip), %rax
        call    *%rax
t02_retaddr:
        jmp     t02_done
t02_target:
        lea     -8(%r12), %r13
        ASSERT_REG_EQ %rsp, %r13, 2
        movq    (%rsp), %rbx
        lea     t02_retaddr(%rip), %r13
        ASSERT_REG_EQ %rbx, %r13, 2
        ret
t02_done:
        ASSERT_REG_EQ %rsp, %r12, 2

        /* =========================================================
         * T03: indirect call via memory  (call *funcptr(%rip))
         * ========================================================= */
        movq    %rsp, %r12
        lea     t03_target(%rip), %rax
        movq    %rax, funcptr(%rip)
        call    *funcptr(%rip)
t03_retaddr:
        jmp     t03_done
t03_target:
        lea     -8(%r12), %r13
        ASSERT_REG_EQ %rsp, %r13, 3
        movq    (%rsp), %rbx
        lea     t03_retaddr(%rip), %r13
        ASSERT_REG_EQ %rbx, %r13, 3
        ret
t03_done:
        ASSERT_REG_EQ %rsp, %r12, 3

        /* =========================================================
         * T04: short forward jmp (rel8) -- skips dead code
         * ========================================================= */
        movq    $0, %rax
        jmp     t04_fwd
        movq    $1, %rax        /* must NOT execute */
t04_fwd:
        ASSERT_EQ64 %rax, 0, 4

        /* =========================================================
         * T05: backward jmp -- loop 5 times
         * ========================================================= */
        movq    $0, %rcx
t05_loop:
        addq    $1, %rcx
        cmpq    $5, %rcx
        jl      t05_loop
        ASSERT_EQ64 %rcx, 5, 5

        /* =========================================================
         * T06: near forward jmp (rel32) -- 200-byte nop sled forces rel32 encoding
         * ========================================================= */
        movq    $0, %rax
        jmp     t06_fwd
        .space  200, 0x90       /* nop sled: distance > 127 forces rel32 */
t06_fwd:
        ASSERT_EQ64 %rax, 0, 6

        /* =========================================================
         * T07: indirect jmp via register  (jmp *%rbx)
         * ========================================================= */
        movq    $0, %rax
        lea     t07_target(%rip), %rbx
        jmp     *%rbx
        movq    $1, %rax        /* must NOT execute */
t07_target:
        ASSERT_EQ64 %rax, 0, 7

        /* =========================================================
         * T08: indirect jmp via memory  (jmp *funcptr(%rip))
         * ========================================================= */
        movq    $0, %rax
        lea     t08_target(%rip), %rbx
        movq    %rbx, funcptr(%rip)
        jmp     *funcptr(%rip)
        movq    $1, %rax        /* must NOT execute */
t08_target:
        ASSERT_EQ64 %rax, 0, 8

        /* =========================================================
         * T09: conditional jumps -- taken and not taken
         * ========================================================= */
        /* JE taken: 0 == 0 */
        xorq    %rax, %rax
        cmpq    $0, %rax
        jne     t09_fail
        jmp     t09_je_ok
t09_fail:
        FAIL    9
t09_je_ok:

        /* JE not taken: 1 != 0 */
        movq    $1, %rax
        cmpq    $0, %rax
        je      t09_fail

        /* JNE taken: 1 != 0 */
        movq    $1, %rax
        cmpq    $0, %rax
        jne     t09_jne_ok
        jmp     t09_fail
t09_jne_ok:

        /* JL taken (signed): -1 < 0 */
        movq    $-1, %rax
        cmpq    $0, %rax
        jl      t09_jl_ok
        jmp     t09_fail
t09_jl_ok:

        /* JL not taken (signed): 1 < 0 is false */
        movq    $1, %rax
        cmpq    $0, %rax
        jl      t09_fail

        /* JG taken (signed): 1 > 0 */
        movq    $1, %rax
        cmpq    $0, %rax
        jg      t09_jg_ok
        jmp     t09_fail
t09_jg_ok:

        /* JG not taken (signed): -1 > 0 is false */
        movq    $-1, %rax
        cmpq    $0, %rax
        jg      t09_fail

        /* JB taken (unsigned): 1 < 2 */
        movq    $1, %rax
        cmpq    $2, %rax
        jb      t09_jb_ok
        jmp     t09_fail
t09_jb_ok:

        /* JB not taken (unsigned): 3 < 2 is false */
        movq    $3, %rax
        cmpq    $2, %rax
        jb      t09_fail

        /* JA taken (unsigned): 3 > 2 */
        movq    $3, %rax
        cmpq    $2, %rax
        ja      t09_ja_ok
        jmp     t09_fail
t09_ja_ok:

        /* JA not taken (unsigned): 1 > 2 is false */
        movq    $1, %rax
        cmpq    $2, %rax
        ja      t09_fail

        /* =========================================================
         * T10: nested calls -- stack balanced after return from outer
         * ========================================================= */
        lea     stack_space_end(%rip), %rsp
        movq    %rsp, %r12
        call    t10_outer
        ASSERT_REG_EQ %rsp, %r12, 10
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
        movq    $0, counter(%rip)
        call    t11_sub
        call    t11_sub
        call    t11_sub
        call    t11_sub
        movq    counter(%rip), %rax
        ASSERT_EQ64 %rax, 4, 11
        jmp     t11_done
t11_sub:
        addq    $1, counter(%rip)
        ret
t11_done:

        /* =========================================================
         * T12: call *(%rsp) -- effective address uses RSP as base
         * =========================================================
         * In x86 ModRM, RSP as base always requires a SIB byte even
         * with no index/scale.  The call reads [RSP] for the target
         * before decrementing RSP and pushing the return address.
         */
        lea     stack_space_end(%rip), %rsp
        movq    %rsp, %r12
        lea     t12_target(%rip), %rax
        pushq   %rax                    /* [RSP] = t12_target; RSP = R12-8 */
        call    *(%rsp)                 /* target = [RSP]; RSP = R12-16; [RSP] = retaddr */
t12_retaddr:
        jmp     t12_done
t12_target:
        lea     -16(%r12), %r13
        ASSERT_REG_EQ %rsp, %r13, 12
        movq    (%rsp), %rax
        lea     t12_retaddr(%rip), %r13
        ASSERT_REG_EQ %rax, %r13, 12
        ret
t12_done:
        addq    $8, %rsp                /* discard the pushq %rax from before the call */
        ASSERT_REG_EQ %rsp, %r12, 12

        /* =========================================================
         * T13: jmp *(%rsp) -- effective address uses RSP as base
         * =========================================================
         * jmp does not push a return address; RSP is unchanged.
         */
        lea     stack_space_end(%rip), %rsp
        movq    %rsp, %r12
        lea     t13_target(%rip), %rax
        pushq   %rax                    /* [RSP] = t13_target; RSP = R12-8 */
        jmp     *(%rsp)                 /* target = [RSP]; RSP unchanged */
        movq    $1, %rcx               /* must NOT execute */
t13_target:
        lea     -8(%r12), %r13
        ASSERT_REG_EQ %rsp, %r13, 13
        popq    %rax                    /* discard the target pointer */
        ASSERT_REG_EQ %rsp, %r12, 13

        /* =========================================================
         * T14: call *8(%rsp) -- RSP + disp8, still requires SIB
         * =========================================================
         * Stack layout going into call:
         *   [RSP+8] = t14_target   (pushed first, now deeper)
         *   [RSP]   = dummy 0      (pushed second, on top)
         */
        lea     stack_space_end(%rip), %rsp
        movq    %rsp, %r12
        lea     t14_target(%rip), %rax
        pushq   %rax                    /* RSP = R12-8;  [RSP] = t14_target */
        pushq   $0                      /* RSP = R12-16; [RSP] = 0, [RSP+8] = t14_target */
        call    *8(%rsp)                /* target=[RSP+8]; RSP=R12-24; [RSP]=retaddr */
t14_retaddr:
        jmp     t14_done
t14_target:
        lea     -24(%r12), %r13
        ASSERT_REG_EQ %rsp, %r13, 14
        movq    (%rsp), %rax
        lea     t14_retaddr(%rip), %r13
        ASSERT_REG_EQ %rax, %r13, 14
        ret
t14_done:
        addq    $16, %rsp               /* discard both pushes */
        ASSERT_REG_EQ %rsp, %r12, 14

        /* success */
        mov     $SYS_write, %rax
        mov     $1, %rdi
        lea     success_msg(%rip), %rsi
        mov     $8, %rdx
        syscall

        mov     $SYS_exit, %rax
        xor     %rdi, %rdi
        syscall


        .section .bss
        .balign 16
stack_space:
        .skip   8192
stack_space_end:

        .balign 8
counter:
        .skip   8

        .section .data
        .balign 8
funcptr:
        .quad   0

        .section .rodata
success_msg:
        .ascii  "success\n"
