# Prove that you can't win at tic-tac-toe.
#
# Board: 0 | 1 | 2
#        ---------
#        3 | 4 | 5
#        ---------
#        6 | 7 | 8
#
# Only first moves 0, 1, and 4 are solved since other first moves are reflections
#
# The app takes an optional argument:
#    - the number of iterations to run. Default is defaultIterations.
#
# This version unrolls the loop for better performance

.equ minimum_score, 2
.equ maximum_score, 9
.equ win_score, 6
.equ lose_score, 4
.equ tie_score, 5
.equ x_piece, 1
.equ o_piece, 2
.equ blank_piece, 0                           # not referenced in the code below, but it is assumed to be 0
.equ default_iterations, 1
                                         
# spill offsets -- [rbp + X] where X = 2..5  Spill referrs to saving parameters in registers to memory when needed
# these registers can be spilled: rcx, rdx, r8, r9
# Locations 0 (prior rbp) and 1 (return address) are reserved.
# These are for the functions minmax_min and minmax_max
.equ A_SPILL_OFFSET, 8 * 2                    # alpha
.equ B_SPILL_OFFSET, 8 * 3                    # beta
.equ V_SPILL_OFFSET, 8 * 4                    # value
.equ I_SPILL_OFFSET, 8 * 5                    # i in the for loop 0..8

.data
.p2align 8                                    # some intel CPUs have effective cache lines of 256 bytes (e.g. i5-2430M)
board0: .byte 1,0,0,0,0,0,0,0,0
.p2align 8
board1: .byte 0,1,0,0,0,0,0,0,0
.p2align 8
board4: .byte 0,0,0,0,1,0,0,0,0

.p2align 8
loopCount: .quad default_iterations
moveCount: .quad 0

.p2align 8
.section .rodata
WINPROCS: 
    .quad proc0
    .quad proc1
    .quad proc2
    .quad proc3
    .quad proc4
    .quad proc5
    .quad proc6
    .quad proc7
    .quad proc8
.p2align 4
NEXTMIN:  
    .quad minmax_min_try_1
    .quad minmax_min_try_2
    .quad minmax_min_try_3
    .quad minmax_min_try_4
    .quad minmax_min_try_5
    .quad minmax_min_try_6
    .quad minmax_min_try_7
    .quad minmax_min_try_8
    .quad minmax_min_loadv_done
.p2align 4
NEXTMAX:  
    .quad minmax_max_try_1
    .quad minmax_max_try_2
    .quad minmax_max_try_3
    .quad minmax_max_try_4
    .quad minmax_max_try_5
    .quad minmax_max_try_6
    .quad minmax_max_try_7
    .quad minmax_max_try_8
    .quad minmax_max_loadv_done

.p2align 4
moves_string: .asciz " moves\n"

.text
.p2align 4
.global _start
_start:
    push    %rbp
    mov     %rsp, %rbp
    sub     $32 + 8 * 4, %rsp

    movq    8(%rbp), %rax
    cmp     $1, %rax                          # is there a loop count?
    je      _runit
    movq    24(%rbp), %rdi
    call    atou64
    cmp     $0, %rax
    je      _runit                            # if malformed, use the default
    movq    %rax, (loopCount)

  _runit:    
    movq    $0, (moveCount)

    mov     $0, %rcx                          # solve for board 0
    call    TTTThreadProc

    mov     $1, %rcx                          # solve for board 1
    call    TTTThreadProc

    mov     $4, %rcx                          # solve for board 4
    call    TTTThreadProc

    mov     (moveCount), %rcx
    call    print_uint64

    movq    $1, %rax                          # write
    movq    $1, %rdi
    movq    $7, %rdx
    lea     moves_string, %rsi
    syscall

    mov     $60, %rax                         # exit
    xor     %rdi, %rdi
    syscall

.equ boardIndex, 32                           # local variable just above child spill locations
.p2align 4
TTTThreadProc:
    push    %rbp
    mov     %rsp, %rbp
    sub     $32 + 8 * 4, %rsp
    
    xor     %r13, %r13                        # and r13 to be the move count
    lea     WINPROCS, %rsi                    # rsi has the win proc function table
    mov     $0, %r12                          # r12 has 0
    
    mov     %rcx, boardIndex(%rsp)            # save the initial move board position

    # load r10 with the board to play -- BOARD0, BOARD1, or BOARD4

    cmp     $0, %rcx
    jne     TTTThreadProc_try1
    lea     board0, %r10
    jmp     TTTThreadProc_for

  TTTThreadProc_try1:
    cmp     $1, %rcx
    jne     TTTThreadProc_try4
    lea     board1, %r10
    jmp     TTTThreadProc_for

  TTTThreadProc_try4:                         # don't validate it's four -- just assume it
    lea     board4, %r10
    mov     $4, %rcx                          # ensure this is the case
    mov     %rcx, boardIndex(%rsp)            # again, make sure

  TTTThreadProc_for:
    movq    [loopCount], %r15

.p2align 4
  TTTThreadProc_loop:
    mov     $minimum_score, %rcx              # alpha -- minimum score
    mov     $maximum_score, %rdx              # beta -- maximum score
    xor     %r8, %r8                          # depth is 0
    mov     boardIndex(%rsp), %r9             # position of last board update

    # r10 holds the board
    # r13 holds the minmax call count

    call    minmax_min                        # call min, because X just moved and now O moves should be minimized

    dec     %r15
    cmp     $0, %r15
    jne     TTTThreadProc_loop

    lock add %r13, (moveCount)                # do this locked update once here at the end instead of for each iteration
    xor     %rax, %rax

    leave
    ret

# Odd depth = maximize for X in subsequent moves, O just took a move in r9
.p2align 4
minmax_max:
    push    %rbp
    mov     %rsp, %rbp
    sub     $32, %rsp

    # rcx: alpha. Store in spill location reserved by parent stack
    # rdx: beta. Store in spill location reserved by parent stack
    # r8:  depth. keep in the register
    # r9:  position of last piece added 0..8. Keep in the register because it's used right away
    #      later, r9 is the i in the for loop 0..8. Spilled.
    # r10: the board
    # r11: value. Spilled.
    # r12: 0 constant
    # r13: global minmax call count
    # r14: temporary location of jump table
    # r15: reserved for global loop of N iterations
    # rsi: pointer to WINPROCS

    inc     %r13                              # r13 is a global variable with the # of calls to minmax_max and minmax_min

    # NOTE: rcx, r9, and rdx aren't saved in spill locations until actually needed. Don't trash them until after skip_winner

    cmp     $3, %r8                           # # of pieces on board is 1 + depth. So >= 4 means at least 5 moves played
    jle     minmax_max_skip_winner            # if too few moves, there can't be a winner yet

    # the win procs expect the board in r10
    mov     $o_piece, %rax                    # rax contains the player with the latest move on input
    call    *(%rsi,%r9,8)                     # call the proc that checks for wins starting with last piece added

    cmp     $o_piece, %rax                    # did O win?
    mov     $lose_score, %rax                 # wasted mov if not equal, but it often saves a jump. no cmov for loading register with constant
    je      minmax_max_done

.p2align 4
  minmax_max_skip_winner:
    mov     %rcx, A_SPILL_OFFSET(%rbp)        # alpha saved in the spill location
    mov     %r11, V_SPILL_OFFSET(%rbp)        # save value
    mov     %r9, I_SPILL_OFFSET(%rbp)         # save i -- the for loop variable
    mov     $minimum_score, %r11              # minimum possible score. maximizing, so find a score higher than this
    inc     %r8                               # next depth 1..8

    xor     %r9, %r9
    cmpb    %r12b, (%r10)
    je      minmax_max_move
  minmax_max_try_1:
    inc     %r9
    cmpb    %r12b, 1(%r10)
    je      minmax_max_move
  minmax_max_try_2:
    inc     %r9
    cmpb    %r12b, 2(%r10)
    je      minmax_max_move
  minmax_max_try_3:
    inc     %r9
    cmpb    %r12b, 3(%r10)
    je      minmax_max_move
  minmax_max_try_4:
    inc     %r9
    cmpb    %r12b, 4(%r10)
    je      minmax_max_move
  minmax_max_try_5:
    inc     %r9
    cmpb    %r12b, 5(%r10)
    je      minmax_max_move
  minmax_max_try_6:
    inc     %r9
    cmpb    %r12b, 6(%r10)
    je      minmax_max_move
  minmax_max_try_7:
    inc     %r9
    cmpb    %r12b, 7(%r10)
    je      minmax_max_move
  minmax_max_try_8:
    inc     %r9
    cmpb    %r12b, 8(%r10)
    jne     minmax_max_loadv_done

.p2align 4
  minmax_max_move:
    movb    $x_piece, (%r10,%r9)              # make the move
    call    minmax_min
    movb    %r12b, (%r10, %r9)
    cmp     $win_score, %rax                  # compare score with the winning score
    je      minmax_max_unspill                # can't do better than winning score when maximizing
    cmp     %r11, %rax                        # compare score with value
    jle     minmax_max_next
    cmp     %rdx, %rax                        # compare value with beta
    jge     minmax_max_unspill                # beta pruning
    mov     %rax, %r11                        # update value with score
    cmp     %rcx, %rax                        # compare value with alpha
    cmovg   %rax, %rcx                        # update alpha if new value is better
  minmax_max_next:
    lea     NEXTMAX, %r14
    jmp     *(%r14,%r9,8)

.p2align 4
  minmax_max_loadv_done:
    mov     %r11, %rax                        # load value then return

.p2align 4
  minmax_max_unspill:
    dec     %r8                               # restore depth to the current level
    mov     A_SPILL_OFFSET(%rbp), %rcx        # restore alpha
    mov     V_SPILL_OFFSET(%rbp), %r11        # restore value
    mov     I_SPILL_OFFSET(%rbp), %r9         # restore i
  minmax_max_done:
    leave
    ret

# Even depth = mininize for X in subsequent moves, X just took a move in r9
.p2align 4
minmax_min:
    push    %rbp
    mov     %rsp, %rbp
    sub     $32, %rsp

    # rcx: alpha. Store in spill location reserved by parent stack
    # rdx: beta. Store in spill location reserved by parent stack
    # r8:  depth. keep in the register
    # r9:  position of last piece added 0..8. Keep in the register because it's used right away
    #      later, r9 is the i in the for loop 0..8
    # r10: the board
    # r11: value
    # r12: 0 constant
    # r13: global minmax call count
    # r14: temporary location of jump table
    # r15: reserved for global loop of N iterations
    # rsi: pointer to WINPROCS

    inc     %r13                              # r13 is a global variable with the # of calls to minmax_max and minmax_min

    # NOTE: rcx, r9, and rdx aren't saved in spill locations until actually needed. Don't trash them until after skip_winner

    cmp     $3, %r8                           # # of pieces on board is 1 + depth. So >= 4 means at least 5 moves played
    jle     minmax_min_skip_winner            # if too few moves, there can't be a winner yet

    # the win procs expect the board in r10
    mov     $x_piece, %rax                    # rax contains the player with the latest move on input
    call    *(%rsi, %r9, 8)                   # call the proc that checks for wins starting with last piece added

    cmp     $x_piece, %rax                    # did X win? 
    mov     $win_score, %rax                  # wasted mov, but it often saves a jump. no cmov for loading constant to register
    je      minmax_min_done

    cmp     $8, %r8                           # recursion can only go 8 deep before the board is full
    mov     $tie_score, %rax                  # wasted mov, but it often saves a jump
    je      minmax_min_done

.p2align 4
  minmax_min_skip_winner:
    mov     %rdx, B_SPILL_OFFSET(%rbp)        # beta saved in the spill location
    mov     %r11, V_SPILL_OFFSET(%rbp)        # save value
    mov     %r9, I_SPILL_OFFSET(%rbp)         # save i -- the for loop variable
    mov     $maximum_score, %r11              # maximum possible score# minimizing, so find a score lower than this 
    inc     %r8                               # next depth 1..8

    mov     $0, %r9
    cmpb    %r12b, (%r10)
    je      minmax_min_move
  minmax_min_try_1:
    inc     %r9
    cmpb    %r12b, 1(%r10)
    je      minmax_min_move
  minmax_min_try_2:
    inc     %r9
    cmpb    %r12b, 2(%r10)
    je      minmax_min_move
  minmax_min_try_3:
    inc     %r9
    cmpb    %r12b, 3(%r10)
    je      minmax_min_move
  minmax_min_try_4:
    inc     %r9
    cmpb    %r12b, 4(%r10)
    je      minmax_min_move
  minmax_min_try_5:
    inc     %r9
    cmpb    %r12b, 5(%r10)
    je      minmax_min_move
  minmax_min_try_6:
    inc     %r9
    cmpb    %r12b, 6(%r10)
    je      minmax_min_move
  minmax_min_try_7:
    inc     %r9
    cmpb    %r12b, 7(%r10)
    je      minmax_min_move
  minmax_min_try_8:
    inc     %r9
    cmpb    %r12b, 8(%r10)
    jne     minmax_min_loadv_done

.p2align 4
  minmax_min_move:
    movb    $o_piece, (%r10,%r9)
    call    minmax_max
    movb    %r12b, (%r10,%r9)
    cmp     $lose_score, %rax
    je      minmax_min_unspill                # can't do better than losing score when minimizing
    cmp     %r11, %rax                        # compare score with value
    jge     minmax_min_next
    cmp     %rcx, %rax                        # compare value with alpha
    jle     minmax_min_unspill                # alpha pruning
    mov     %rax, %r11                        # update value with score
    cmp     %rdx, %rax                        # compare value with beta
    cmovl   %rax, %rdx                        # update beta if new value is worse
  minmax_min_next:
    lea     NEXTMIN, %r14
    jmp     *(%r14,%r9,8)

.p2align 4
  minmax_min_loadv_done:
    mov     %r11, %rax                        # load value then return

  minmax_min_unspill:
    dec     %r8                               # restore depth to the current level
    mov     B_SPILL_OFFSET(%rbp), %rdx        # restore beta
    mov     V_SPILL_OFFSET(%rbp), %r11        # restore value
    mov     I_SPILL_OFFSET(%rbp), %r9         # restore i
  minmax_min_done:
    leave
    ret

.p2align 4
proc0:
    movb    %al, %bl
    andb    1(%r10), %al
    andb    2(%r10), %al
    jnz     proc0_yes

    movb    %bl, %al
    andb    3(%r10), %al
    andb    6(%r10), %al
    jnz     proc0_yes

    movb    %bl, %al
    andb    4(%r10), %al
    andb    8(%r10), %al

  proc0_yes:
    ret

.p2align 4
proc1:
    movb    %al, %bl
    andb    0(%r10), %al
    andb    2(%r10), %al
    jnz     proc1_yes

    movb    %bl, %al
    andb    4(%r10), %al
    andb    7(%r10), %al

  proc1_yes:
    ret

.p2align 4
proc2:
    movb    %al, %bl
    andb    0(%r10), %al
    andb    1(%r10), %al
    jnz     proc2_yes

    movb    %bl, %al
    andb    5(%r10), %al
    andb    8(%r10), %al
    jnz     proc2_yes

    movb    %bl, %al
    andb    4(%r10), %al
    andb    6(%r10), %al

  proc2_yes:
    ret

.p2align 4
proc3:
    movb    %al, %bl
    andb    0(%r10), %al
    andb    6(%r10), %al
    jnz     proc3_yes

    movb    %bl, %al
    andb    4(%r10), %al
    andb    5(%r10), %al

  proc3_yes:
    ret

.p2align 4
proc4:
    movb    %al, %bl
    andb    0(%r10), %al
    andb    8(%r10), %al
    jnz     proc4_yes

    movb    %bl, %al
    andb    2(%r10), %al
    andb    6(%r10), %al
    jnz     proc4_yes

    movb    %bl, %al
    andb    1(%r10), %al
    andb    7(%r10), %al
    jnz     proc4_yes

    movb    %bl, %al
    andb    3(%r10), %al
    andb    5(%r10), %al

  proc4_yes:
    ret

.p2align 4
proc5:
    movb    %al, %bl
    andb    3(%r10), %al
    andb    4(%r10), %al
    jnz     proc5_yes

    movb    %bl, %al
    andb    2(%r10), %al
    andb    8(%r10), %al

  proc5_yes:
    ret

.p2align 4
proc6:
    movb    %al, %bl
    andb    4(%r10), %al
    andb    2(%r10), %al
    jnz     proc6_yes

    movb    %bl, %al
    andb    0(%r10), %al
    andb    3(%r10), %al
    jnz     proc6_yes

    movb    %bl, %al
    andb    7(%r10), %al
    andb    8(%r10), %al

  proc6_yes:
    ret

.p2align 4
proc7:
    movb    %al, %bl
    andb    1(%r10), %al
    andb    4(%r10), %al
    jnz     proc7_yes

    movb    %bl, %al
    andb    6(%r10), %al
    andb    8(%r10), %al

  proc7_yes:
    ret

.p2align 4
proc8:
    movb    %al, %bl
    andb    0(%r10), %al
    andb    4(%r10), %al
    jnz     proc8_yes

    movb    %bl, %al
    andb    2(%r10), %al
    andb    5(%r10), %al
    jnz     proc8_yes

    movb    %bl, %al
    andb    6(%r10), %al
    andb    7(%r10), %al

  proc8_yes:
    ret
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

.text
.globl atou64
atou64:                                       # string in rdi into a number in rax
    xor     %rax, %rax
    xor     %rbx, %rbx
    movq    $10, %rcx
    xor     %rdx, %rdx
    movq    $'0', %rsi
  _atou64_next:    
    movb    (%rdi), %bl
    cmp     $'0', %bl
    jl      _atou64_done
    cmp     $'9', %bl
    jg      _atou64_done
    mul     %rcx
    sub     %sil, %bl
    add     %rbx, %rax
    inc     %rdi
    jmp     _atou64_next
  _atou64_done:
    ret
