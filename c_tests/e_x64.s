# compute the first 192 digits of e
# replicates this C code:
#    #define DIGITS_TO_FIND 200 /*9009*/
#    int main() {
#      int N = DIGITS_TO_FIND;
#      int x = 0;
#      int a[ DIGITS_TO_FIND ];
#      int n;
#
#      for (n = N - 1; n > 0; --n)
#          a[n] = 1;
#
#      a[1] = 2, a[0] = 0;
#      while (N > 9) {
#          n = N--;
#          while (--n) {
#              a[n] = x % n;
#              x = 10 * a[n-1] + x/n;
#          }
#          printf("%d", x);
#      }
#  
#      printf( "\ndone\n" );
#      return 0;
#    }
#
# array is in  r12
# N is in      r13
# x is in      rax
# n is in      r15
# 10 is in     r10
# preserve rax in r14

.equ array_size, 200

.data
.p2align 4
.comm _array, array_size
.p2align 4
.comm buffer 21
.p2align 4
done_string: .asciz "\ndone\n"

.global _start
.text
_start:
    push    %rbp
    mov     %rsp, %rbp

    mov     $10, %r10
    lea     [_array], %r12                    # for (n = N - 1; n > 0; --n); a[n] = 1;
    mov     %r12, %rdi                        # for (i = 0; i <= SIZE; i++)
    mov     $0x01010101, %rax                     # initialize 4 array entries at a time
    mov     $array_size / 4, %rcx
    cld
    rep     stosl

    movb     $0, (%r12)                       # a[0] = 0;
    movb     $2, 1(%r12)                      # a[1] = 2;

    mov     $array_size, %r13                 # N = DIGITS_TO_FIND;
    xor     %rax, %rax                        # x = 0;

  _outer:
    cmp     $9, %r13                          # while (N > 9)
    je      _loop_done
    dec     %r13                              # N--
    mov     %r13, %r15                        # n = N
    xor     %rdx, %rdx                        # prepare for division

  _inner:
    div     %r15                              # x / n. quotient is in rax, remainder in rdx
    mov     %rax, %r9                         # save quotient in r9 for later
    movb    %dl, (%r12,%r15)                  # a[n] = x % n;
    movzbq  -1(%r12,%r15,1), %rax             # load a[n-1]
    mul     %r10                              # x = 10 * a[n-1]. rdx will be 0 after this
    add     %r9, %rax                         # x += x/n
    dec     %r15                              # n--
    jne     _inner                            # while (--n)

  _print_digit:
    mov     %rax, %rcx
    mov     %rax, %r14
    call    print_uint64
    mov     %r14, %rax
    jmp     _outer

  _loop_done:
    movq    $1, %rax                          # write
    movq    $1, %rdi
    movq    $6, %rdx
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
