program tcmp;

{$mode objfpc}
{$R-}
{$Q-}

uses
  SysUtils;

const
  FLT_EPSILON  = 1.1920928955078125e-7;
  DBL_EPSILON  = 2.2204460492503131e-16;
  LDBL_EPSILON = 1.0842021724855044e-19;

procedure cmp_u8(a, b: Byte);
begin
  WriteLn(Format('  lt %d le %d eq %d ge %d gt %d',
    [Ord(a < b), Ord(a <= b), Ord(a = b), Ord(a >= b), Ord(a > b)]));
end;

procedure cmp_s8(a, b: ShortInt);
begin
  WriteLn(Format('  lt %d le %d eq %d ge %d gt %d',
    [Ord(a < b), Ord(a <= b), Ord(a = b), Ord(a >= b), Ord(a > b)]));
end;

procedure cmp_u16(a, b: Word);
begin
  WriteLn(Format('  lt %d le %d eq %d ge %d gt %d',
    [Ord(a < b), Ord(a <= b), Ord(a = b), Ord(a >= b), Ord(a > b)]));
end;

procedure cmp_s16(a, b: SmallInt);
begin
  WriteLn(Format('  lt %d le %d eq %d ge %d gt %d',
    [Ord(a < b), Ord(a <= b), Ord(a = b), Ord(a >= b), Ord(a > b)]));
end;

procedure cmp_u32(a, b: LongWord);
begin
  WriteLn(Format('  lt %d le %d eq %d ge %d gt %d',
    [Ord(a < b), Ord(a <= b), Ord(a = b), Ord(a >= b), Ord(a > b)]));
end;

procedure cmp_s32(a, b: LongInt);
begin
  WriteLn(Format('  lt %d le %d eq %d ge %d gt %d',
    [Ord(a < b), Ord(a <= b), Ord(a = b), Ord(a >= b), Ord(a > b)]));
end;

procedure cmp_u64(a, b: QWord);
begin
  WriteLn(Format('  lt %d le %d eq %d ge %d gt %d',
    [Ord(a < b), Ord(a <= b), Ord(a = b), Ord(a >= b), Ord(a > b)]));
end;

procedure cmp_s64(a, b: Int64);
begin
  WriteLn(Format('  lt %d le %d eq %d ge %d gt %d',
    [Ord(a < b), Ord(a <= b), Ord(a = b), Ord(a >= b), Ord(a > b)]));
end;

procedure cmp_double(a, b: Double);
var
  diff, abs_diff: Double;
  gt, lt, eq, le, ge: Boolean;
begin
  diff := a - b;
  abs_diff := Abs(diff);
  gt := (diff > 0.0) and (abs_diff > DBL_EPSILON);
  lt := (diff < 0.0) and (abs_diff > DBL_EPSILON);
  eq := abs_diff < DBL_EPSILON;
  le := (diff <= 0.0) or (abs_diff < DBL_EPSILON);
  ge := (diff >= 0.0) or (abs_diff < DBL_EPSILON);
  WriteLn(Format('  dbl lt %d le %d eq %d ge %d gt %d',
    [Ord(lt), Ord(le), Ord(eq), Ord(ge), Ord(gt)]));
end;

procedure cmp_long_double(a, b: Extended);
var
  diff, abs_diff: Extended;
  gt, lt, eq, le, ge: Boolean;
begin
  diff := a - b;
  abs_diff := Abs(diff);
  gt := (diff > 0.0) and (abs_diff > LDBL_EPSILON);
  lt := (diff < 0.0) and (abs_diff > LDBL_EPSILON);
  eq := abs_diff < DBL_EPSILON;  { matches C source: uses DBL_EPSILON, not LDBL_EPSILON }
  le := (diff <= 0.0) or (abs_diff < LDBL_EPSILON);
  ge := (diff >= 0.0) or (abs_diff < LDBL_EPSILON);
  WriteLn(Format('  ldb lt %d le %d eq %d ge %d gt %d',
    [Ord(lt), Ord(le), Ord(eq), Ord(ge), Ord(gt)]));
end;

procedure cmp_float(a, b: Single);
var
  diff, abs_diff: Single;
  gt, lt, eq, le, ge: Boolean;
begin
  diff := a - b;
  abs_diff := Abs(diff);
  gt := (diff > 0.0) and (abs_diff > FLT_EPSILON);
  lt := (diff < 0.0) and (abs_diff > FLT_EPSILON);
  eq := abs_diff < FLT_EPSILON;
  le := (diff <= 0.0) or (abs_diff < FLT_EPSILON);
  ge := (diff >= 0.0) or (abs_diff < FLT_EPSILON);
  WriteLn(Format('  flt lt %d le %d eq %d ge %d gt %d',
    [Ord(lt), Ord(le), Ord(eq), Ord(ge), Ord(gt)]));
end;

var
  f: Single;
  d: Double;
  ld: Extended;
  i: Integer;

begin
  WriteLn('uint8_t:');
  cmp_u8(1, 3);
  cmp_u8(1, Byte(-3));
  cmp_u8(Byte(-1), 3);
  cmp_u8(Byte(-1), Byte(-3));
  cmp_u8(Byte(-1), Byte(-1));
  cmp_u8(1, Byte(-1));
  cmp_u8(247, 3);
  cmp_u8(247, Byte(-3));
  cmp_u8(9, 3);
  cmp_u8(9, Byte(-3));
  cmp_u8($f1, $f2);
  cmp_u8($f3, $f2);
  cmp_u8($e1, $e2);
  cmp_u8($e3, $e2);
  cmp_u8($81, $78);
  cmp_u8(0, $80);
  cmp_u8($7f, $80);

  WriteLn('int8_t:');
  cmp_s8(1, 3);
  cmp_s8(1, -3);
  cmp_s8(-1, 3);
  cmp_s8(-1, -3);
  cmp_s8(-1, -1);
  cmp_s8(1, -1);
  cmp_s8(ShortInt(247), 3);
  cmp_s8(ShortInt(247), -3);
  cmp_s8(9, 3);
  cmp_s8(9, -3);
  cmp_s8(ShortInt($f1), ShortInt($f2));
  cmp_s8(ShortInt($f3), ShortInt($f2));
  cmp_s8(ShortInt($e1), ShortInt($e2));
  cmp_s8(ShortInt($e3), ShortInt($e2));
  cmp_s8(ShortInt($81), $78);
  cmp_s8(0, ShortInt($80));
  cmp_s8($7f, ShortInt($80));

  WriteLn('uint16_t:');
  cmp_u16(1, 3);
  cmp_u16(1, Word(-3));
  cmp_u16(Word(-1), 3);
  cmp_u16(Word(-1), Word(-3));
  cmp_u16(Word(-1), Word(-1));
  cmp_u16(1, Word(-1));
  cmp_u16(247, 3);
  cmp_u16(247, Word(-3));
  cmp_u16(Word(-247), 3);
  cmp_u16(Word(-247), Word(-3));
  cmp_u16($ff11, $ff22);
  cmp_u16($ff33, $ff22);
  cmp_u16($ef11, $ef22);
  cmp_u16($ef33, $ef22);
  cmp_u16($8001, $7ff8);
  cmp_u16(0, $8000);
  cmp_u16($7fff, $8000);

  WriteLn('int16_t:');
  cmp_s16(1, 3);
  cmp_s16(1, -3);
  cmp_s16(-1, 3);
  cmp_s16(-1, -3);
  cmp_s16(-1, -1);
  cmp_s16(1, -1);
  cmp_s16(247, 3);
  cmp_s16(247, -3);
  cmp_s16(-247, 3);
  cmp_s16(-247, -3);
  cmp_s16(SmallInt($ff11), SmallInt($ff22));
  cmp_s16(SmallInt($ff33), SmallInt($ff22));
  cmp_s16(SmallInt($ef11), SmallInt($ef22));
  cmp_s16(SmallInt($ef33), SmallInt($ef22));
  cmp_s16(SmallInt($8001), $7ff8);
  cmp_s16(0, SmallInt($8000));
  cmp_s16($7fff, SmallInt($8000));

  WriteLn('uint32_t:');
  cmp_u32(1, 3);
  cmp_u32(1, LongWord(-3));
  cmp_u32(LongWord(-1), 3);
  cmp_u32(LongWord(-1), LongWord(-3));
  cmp_u32(LongWord(-1), LongWord(-1));
  cmp_u32(1, LongWord(-1));
  cmp_u32(247, 3);
  cmp_u32(247, LongWord(-3));
  cmp_u32(LongWord(-247), 3);
  cmp_u32(LongWord(-247), LongWord(-3));
  cmp_u32($ffff1111, $ffff2222);
  cmp_u32($ffff3333, $ffff2222);
  cmp_u32($efff1111, $efff2222);
  cmp_u32($efff3333, $efff2222);
  cmp_u32($80000001, $7ffffff8);
  cmp_u32(0, $80000000);
  cmp_u32($7fffffff, $80000000);

  WriteLn('int32_t:');
  cmp_s32(1, 3);
  cmp_s32(1, -3);
  cmp_s32(-1, 3);
  cmp_s32(-1, -3);
  cmp_s32(-1, -1);
  cmp_s32(1, -1);
  cmp_s32(247, 3);
  cmp_s32(247, -3);
  cmp_s32(-247, 3);
  cmp_s32(-247, -3);
  cmp_s32(LongInt($ffff1111), LongInt($ffff2222));
  cmp_s32(LongInt($ffff3333), LongInt($ffff2222));
  cmp_s32(LongInt($efff1111), LongInt($efff2222));
  cmp_s32(LongInt($efff3333), LongInt($efff2222));
  cmp_s32(LongInt($80000001), $7ffffff8);
  cmp_s32(0, LongInt($80000000));
  cmp_s32($7fffffff, LongInt($80000000));

  WriteLn('uint64_t:');
  cmp_u64(1, 3);
  cmp_u64(1, QWord(-3));
  cmp_u64(QWord(-1), 3);
  cmp_u64(QWord(-1), QWord(-3));
  cmp_u64(QWord(-1), QWord(-1));
  cmp_u64(1, QWord(-1));
  cmp_u64(247, 3);
  cmp_u64(247, QWord(-3));
  cmp_u64(QWord(-247), 3);
  cmp_u64(QWord(-247), QWord(-3));
  cmp_u64(QWord($ffff111111111111), QWord($ffff222222222222));
  cmp_u64(QWord($ffff333333333333), QWord($ffff222222222222));
  cmp_u64(QWord($efff111111111111), QWord($efff222222222222));
  cmp_u64(QWord($efff333333333333), QWord($efff222222222222));
  cmp_u64(QWord($8000000000000001), $7ffffffffffffff8);
  cmp_u64(0, QWord($8000000000000000));
  cmp_u64($7fffffffffffffff, QWord($8000000000000000));

  WriteLn('int64_t:');
  cmp_s64(1, 3);
  cmp_s64(1, -3);
  cmp_s64(-1, 3);
  cmp_s64(-1, -3);
  cmp_s64(-1, -1);
  cmp_s64(1, -1);
  cmp_s64(247, 3);
  cmp_s64(247, -3);
  cmp_s64(-247, 3);
  cmp_s64(-247, -3);
  cmp_s64(Int64($ffff111111111111), Int64($ffff222222222222));
  cmp_s64(Int64($ffff333333333333), Int64($ffff222222222222));
  cmp_s64(Int64($efff111111111111), Int64($efff222222222222));
  cmp_s64(Int64($efff333333333333), Int64($efff222222222222));
  cmp_s64(Int64($8000000000000001), $7ffffffffffffff8);
  cmp_s64(0, Int64($8000000000000000));
  cmp_s64($7fffffffffffffff, Int64($8000000000000000));

  WriteLn('floating point:');
  f := -0.5;
  d := -0.5;
  ld := -0.5;
  for i := 0 to 9 do
  begin
    cmp_float(f, 0.2);
    cmp_double(d, 0.2);
    cmp_long_double(ld, 0.2);
    f := f + 0.1;
    d := d + 0.1;
    ld := ld + 0.1;
  end;
end.
