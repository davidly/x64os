program sleeptm;

{$mode objfpc}

uses
  BaseUnix, Linux, SysUtils;

{ TRUsage and RUSAGE_SELF not in FPC Linux RTL; define manually.
  Uses TTimeVal and clong which are platform-sized (8/16 bytes on x86_64, 4/8 on i386)
  so the layout matches the C struct rusage on both architectures. }
type
  TRUsage = packed record
    ru_utime: TTimeVal;
    ru_stime: TTimeVal;
    ru_maxrss: clong;
    ru_ixrss: clong;
    ru_idrss: clong;
    ru_isrss: clong;
    ru_minflt: clong;
    ru_majflt: clong;
    ru_nswap: clong;
    ru_inblock: clong;
    ru_oublock: clong;
    ru_msgsnd: clong;
    ru_msgrcv: clong;
    ru_nsignals: clong;
    ru_nvcsw: clong;
    ru_nivcsw: clong;
  end;

const
  RUSAGE_SELF = 0;

{ getrusage via direct syscall (98 on x86_64, 77 on i386) to avoid libc dependency.
  On x86_64 System V ABI, args 1+2 are already in rdi/rsi which are also the
  syscall arg registers, so only the syscall number needs loading.
  On i386 register convention, args arrive in eax/edx and must move to ebx/ecx.
  Returns 0 on success; getrusage(RUSAGE_SELF, valid_ptr) never fails in practice. }
function my_getrusage(who: cint; var ru: TRUsage): cint; assembler;
asm
{$ifdef CPUX86_64}
  { who in rdi, &ru in rsi already match syscall arg registers }
  movl $98, %eax
  syscall
{$else}
  pushl %ebx
  movl %eax, %ebx
  movl %edx, %ecx
  movl $77, %eax
  int $0x80
  popl %ebx
{$endif}
end;

function rdtsc: QWord; assembler;
asm
{$ifdef CPUX86_64}
  rdtsc
  shlq $32, %rdx
  orq %rdx, %rax
{$else}
  { i386: rdtsc puts high 32 bits in edx, low in eax; FPC returns QWord in edx:eax }
  rdtsc
{$endif}
end;

function timespec_diff_ms(const a, b: TTimeSpec): Int64;
var
  sec_diff : Int64;
  nsec_diff: Int64;
begin
  sec_diff  := a.tv_sec - b.tv_sec;
  nsec_diff := a.tv_nsec - b.tv_nsec;

  if nsec_diff < 0 then
  begin
    Dec(sec_diff);
    Inc(nsec_diff, 1000000000);
  end;

  Result := sec_diff * 1000 + (nsec_diff div 1000000);
end;

var
  start_cycles, end_cycles: QWord;
  t_start, t_after_sleep, t_end: TTimeSpec;
  sts_start, sts_sleep_end, sts_end: TTimeSpec;
  tms_start, tms_sleep_end, tms_end, tms_dummy: TTms;
  cend_sleep, cbusy_loop: TClock;
  request: TTimeSpec;
  usage: TRUsage;
  clk_tck: Int64;
  busy_work: QWord;
  busy_time: Int64;
  user_ms, system_ms: QWord;
  sleep_ms, total_ms: Int64;
  cgt_sleep_duration, cgt_duration: LongWord;
  res: cInt;

begin
  start_cycles := rdtsc;
  clock_gettime(CLOCK_MONOTONIC, @t_start);

  res := clock_gettime(CLOCK_MONOTONIC, @sts_start);
  if res = -1 then
  begin
    WriteLn(Format('clock_gettime failed with error %d', [fpGetErrno]));
    Halt(1);
  end;

  { sysconf(_SC_CLK_TCK) always returns 100 on Linux (USER_HZ) }
  clk_tck := 100;
  WriteLn(Format('clk_tck / number of linux ticks per second: %d', [clk_tck]));

  fpTimes(tms_start);
  request.tv_sec := 1;
  request.tv_nsec := 500000000;
  res := fpNanosleep(@request, nil);
  if res = -1 then
  begin
    WriteLn(Format('nanosleep failed with error %d', [fpGetErrno]));
    Halt(1);
  end;

  clock_gettime(CLOCK_MONOTONIC, @t_after_sleep);

  cend_sleep := fpTimes(tms_sleep_end);

  res := clock_gettime(CLOCK_MONOTONIC, @sts_sleep_end);
  if res = -1 then
  begin
    WriteLn(Format('clock_gettime failed with error %d', [fpGetErrno]));
    Halt(1);
  end;

  busy_work := 0;
  repeat
    cbusy_loop := fpTimes(tms_dummy);
    busy_time := (Int64(cbusy_loop - cend_sleep) * 1000) div clk_tck;
    if busy_time >= 1000 then
      break;
    busy_work := busy_work * QWord(busy_time);
    busy_work := busy_work - 33;
    busy_work := busy_work * 14;
  until False;

  fpTimes(tms_end);

  user_ms := QWord(tms_end.tms_utime) * 1000 div QWord(clk_tck);
  system_ms := QWord(tms_end.tms_stime) * 1000 div QWord(clk_tck);

  res := my_getrusage(RUSAGE_SELF, usage);
  if res = -1 then
  begin
    WriteLn(Format('getrusage failed with error %d', [fpGetErrno]));
    Halt(1);
  end;

  user_ms := QWord(usage.ru_utime.tv_sec) * 1000 + QWord(usage.ru_utime.tv_usec) div 1000;
  system_ms := QWord(usage.ru_stime.tv_sec) * 1000 + QWord(usage.ru_stime.tv_usec) div 1000;

  if (user_ms = 0) or (system_ms = 0) then
    WriteLn(Format('getrusage user time in ms: %d, system time %d', [Int64(user_ms), Int64(system_ms)]));

  clock_gettime(CLOCK_MONOTONIC, @t_end);
  sleep_ms := timespec_diff_ms(t_after_sleep, t_start);
  total_ms := timespec_diff_ms(t_end, t_start);

  res := clock_gettime(CLOCK_MONOTONIC, @sts_end);
  if res = -1 then
  begin
    WriteLn(Format('clock_gettime failed with error %d', [fpGetErrno]));
    Halt(1);
  end;

  if (sleep_ms < 1480) or (sleep_ms > 1520) or (total_ms < 2480) or (total_ms > 2520) then
    WriteLn(Format('milliseconds sleeping using chrono (should be ~1500) %d, milliseconds total (should be ~2500): %d',
      [sleep_ms, total_ms]));

  cgt_sleep_duration := LongWord(timespec_diff_ms(sts_sleep_end, sts_start));
  cgt_duration := LongWord(timespec_diff_ms(sts_end, sts_start));

  if (cgt_sleep_duration < 1480) or (cgt_sleep_duration > 1530) or
     (cgt_duration < 2480) or (cgt_duration > 2540) then
  begin
    WriteLn(Format('millisecond sleep duration using clock_gettime (should be about 1500 ms): %u', [cgt_sleep_duration]));
    WriteLn(Format('millisecond duration using clock_gettime (should be about 2500 ms): %u', [cgt_duration]));
  end;

  end_cycles := rdtsc;
  if end_cycles <= start_cycles then
    WriteLn('rdtsc end cycles 0x' + LowerCase(IntToHex(end_cycles, 1)) +
            ' not greater than start cycles 0x' + LowerCase(IntToHex(start_cycles, 1)));

  WriteLn('sleepy time ended with great success');
end.
