# build on linux: gcc $1.s -o $1.elf -static -nostdlib
#
# The BYTE magazine classic sieve from 1981:
#   #define SIZE 8190
#
#   char flags[SIZE+1];
#
#   int main()
#           {
#           int i,k;
#           int prime,count,iter;
#
#           for (iter = 1; iter <= 10; iter++) {    /* do program 10 times */
#                   count = 0;                      /* initialize prime counter */
#                   for (i = 0; i <= SIZE; i++)     /* set all flags TRUE */
#                           flags[i] = TRUE;
#                   for (i = 0; i <= SIZE; i++) {
#                           if (flags[i]) {         /* found a prime */
#                                   prime = i + i + 3;      /* twice index + 3 */
#                                   for (k = i + prime; k <= SIZE; k += prime)
#                                           flags[k] = FALSE;       /* kill all multiples */
#                                   count++;                /* primes found */
#                                   }
#                           }
#                   }
#           printf("%d primes.\n",count);           /*primes found in 10th pass */
#           return 0;
#           }
#
# k         r10
# flags     r11
# size      r12
# prime     r13
# count     rdx
# i         r15
# iter      r9

.equ flags_size, 8190
.equ size_full, 8192

.data
.globl _flags
.p2align 4
.comm _flags,flags_size+2         # multiple of 4 and includes 2 bonus bytes per algorithm and to stop inner loop
.p2align 4
.comm buffer 21
.p2align 4
done_string: .asciz " primes\n"

.global _start
.text
_start:
    push    %rbp
    mov     %rsp, %rbp

    mov     $10, %r9                          # iter <= 10

  _next_iteration:
    lea     [_flags], %r11                    # for (i = 0; i <= SIZE; i++)
    mov     $0x0101010101010101, %rcx         # initialize 8 array entries at a time
    mov     $size_full - 8, %rbx

  _initialize_next:
    movq    %rcx, (%r11, %rbx)     # flags[i] = TRUE
    sub     $8, %rbx
    jge     _initialize_next

    xor     %rcx, %rcx
    xor     %rbx, %rbx
    movq    $-1, %r15                         # i = -1
    xor     %rdx, %rdx                        # count = 0

  _outer:
    inc     %r15
    cmpb    $0, (%r11, %r15)
    je      _outer                            # if flags[i]

    cmp     $flags_size, %r15
    jg      _all_done

    mov     %r15, %r13                        # prime = i
    add     $3, %r13                          # prime += 3
    add     %r15, %r13                        # prime += i
    mov     %r13, %r10                        # k = prime
    add     %r15, %r10                        # k += i
    cmp     $flags_size, %r10
    jg      _inc_count

  _inner:
    movb    $0, (%r11, %r10)                  # flags[ k ] = false
    add     %r13, %r10                        # k += prime
    cmp     $flags_size, %r10                 # k <= SIZE
    jle     _inner

  _inc_count:
    inc     %rdx
    jmp     _outer

  _all_done:
    dec     %r9
    jnz     _next_iteration

    mov     %rdx, %rcx
    call    print_uint64

    movq    $1, %rax
    movq    $1, %rdi
    movq    $8, %rdx
    lea     done_string, %rsi
    syscall

    mov     $60, %rax                         # exit
    xor     %rdi, %rdi
    syscall

/*
 * Function: print_uint64
 * Description: Prints a 64-bit unsigned integer from RCX to stdout.
 * Input: RCX contains the unsigned integer to print.
 * Clobbers: RAX, RCX, RDX, RDI, RSI, RBP, RSP, R11
 * The syscall instruction results in rax, rcx, and r11 getting trashed.
 */

.text
.globl print_uint64

print_uint64:
    /* Prologue to set up a new stack frame and save RBP */
    pushq   %rbp
    movq    %rsp, %rbp

    /* Allocate 21 bytes on the stack for the string buffer.
     * (20 digits max for 2^64-1 + null terminator, or 1 for '0' case)
     */
    subq    $21, %rsp

    /* Special case: if the input is 0, print a single '0' character. */
    cmpq    $0, %rcx
    jne     .nonzero_case

    movq    $1, %rax        /* sys_write syscall number */
    movq    $1, %rdi        /* file descriptor 1 (stdout) */
    movq    %rsp, %rsi      /* buffer address */
    movb    $'0', (%rsi)    /* store '0' in the buffer */
    movq    $1, %rdx        /* length is 1 */
    syscall
    jmp     .done

.nonzero_case:
    movq    %rcx, %rax      /* rax = number */
    leaq    20(%rsp), %rdi  /* rdi points to the end of the buffer */
    movb    $0, (%rdi)      /* null-terminate the string */
    decq    %rdi            /* move pointer to the last character position */

.convert_loop:
    xorq    %rdx, %rdx      /* clear rdx for division */
    movq    $10, %rbx       /* divisor is 10 */
    divq    %rbx            /* rax = rax / 10, rdx = rax % 10 */
    addb    $'0', %dl       /* convert digit to ASCII character */
    movb    %dl, (%rdi)     /* store the ASCII character in the buffer */
    decq    %rdi            /* move to the next position */
    cmpq    $0, %rax        /* check if rax is 0 */
    jne     .convert_loop   /* continue if not zero */

    incq    %rdi            /* rdi now points to the start of the string */

    movq    %rbp, %rdx      /* rdx = initial rsp (from rbp) */
    subq    %rdi, %rdx      /* rdx = length of the string */
    dec     %rdx
    movq    %rdi, %rsi      /* rsi = address of the string to print */
    movq    $1, %rax        /* sys_write syscall number */
    movq    $1, %rdi        /* file descriptor 1 (stdout) */
    syscall                 /* make the system call; trashes rax, rcx, and r11 on Linux */

.done:
    movq    %rbp, %rsp      /* Restore the stack pointer */
    popq    %rbp            /* Restore RBP */
    ret
