program sieve_of_eratosthenes
  implicit none

  integer, parameter :: limit = 100
  logical, dimension(limit) :: is_prime
  integer :: i, p, count

  ! Initialize all numbers as potentially prime
  is_prime = .true.

  ! 0 and 1 are not prime
  is_prime(1) = .false.

  ! Sieve algorithm
  do p = 2, int(sqrt(real(limit)))
    if (is_prime(p)) then
      ! Mark multiples of p as not prime
      do i = p*p, limit, p
        is_prime(i) = .false.
      end do
    end if
  end do

  ! Print prime numbers
  write(*,*) "Prime numbers up to", limit, ":"
  count = 0
  do i = 1, limit
    if (is_prime(i)) then
      write(*,*) i
      count = count + 1
    end if
  end do
  write(*,*) "Total primes found:", count

end program sieve_of_eratosthenes