program tshift;

{$mode objfpc}
{$H-}
{$R-}
{$Q-}

uses
  SysUtils;

function WrapU8(x: LongInt): UInt8;
begin
  Result := UInt8(x and $ff);
end;

function WrapI8(x: LongInt): Int8;
var
  u: UInt8;
begin
  u := UInt8(x and $ff);
  Move(u, Result, SizeOf(Result));
end;

function WrapU16(x: LongInt): UInt16;
begin
  Result := UInt16(x and $ffff);
end;

function WrapI16(x: LongInt): Int16;
var
  u: UInt16;
begin
  u := UInt16(x and $ffff);
  Move(u, Result, SizeOf(Result));
end;

function WrapU32(x: Int64): UInt32;
begin
  Result := UInt32(UInt64(x) and $ffffffff);
end;

function WrapI32(x: Int64): Int32;
var
  u: UInt32;
begin
  u := UInt32(UInt64(x) and $ffffffff);
  Move(u, Result, SizeOf(Result));
end;

function UIntToStrBase(n: UInt64; base, bytes: Integer): string;
const
  Digits = '0123456789abcdefghijklmnopqrstuvwxyz';
var
  rem: UInt64;
  mask: UInt64;
begin
  if bytes < 8 then
  begin
    mask := UInt64($ffffffffffffffff) shr (8 * (8 - bytes));
    n := n and mask;
  end;

  if n = 0 then
    Exit('0');

  Result := '';
  while n <> 0 do
  begin
    rem := n mod UInt64(base);
    Result := Digits[Integer(rem) + 1] + Result;
    n := n div UInt64(base);
  end;
end;

function IntToStrBase(n: Int64; base, bytes: Integer): string;
begin
  if (base = 10) and (n < 0) then
    Result := '-' + UIntToStrBase(UInt64(-n), base, bytes)
  else
    Result := UIntToStrBase(UInt64(n), base, bytes);
end;

procedure ShowU8(x: UInt8);
begin
  WriteLn('    sizeof T: ', SizeOf(x), ', result: ', UIntToStrBase(x, 16, SizeOf(x)));
end;

procedure ShowI8(x: Int8);
begin
  WriteLn('    sizeof T: ', SizeOf(x), ', result: ', IntToStrBase(x, 16, SizeOf(x)));
end;

procedure ShowU16(x: UInt16);
begin
  WriteLn('    sizeof T: ', SizeOf(x), ', result: ', UIntToStrBase(x, 16, SizeOf(x)));
end;

procedure ShowI16(x: Int16);
begin
  WriteLn('    sizeof T: ', SizeOf(x), ', result: ', IntToStrBase(x, 16, SizeOf(x)));
end;

procedure ShowU32(x: UInt32);
begin
  WriteLn('    sizeof T: ', SizeOf(x), ', result: ', UIntToStrBase(x, 16, SizeOf(x)));
end;

procedure ShowI32(x: Int32);
begin
  WriteLn('    sizeof T: ', SizeOf(x), ', result: ', IntToStrBase(x, 16, SizeOf(x)));
end;

procedure ShowU64(x: UInt64);
begin
  WriteLn('    sizeof T: ', SizeOf(x), ', result: ', UIntToStrBase(x, 16, SizeOf(x)));
end;

procedure ShowI64(x: Int64);
begin
  WriteLn('    sizeof T: ', SizeOf(x), ', result: ', IntToStrBase(x, 16, SizeOf(x)));
end;

function BC(x: Boolean): Char;
begin
  if x then
    Result := 't'
  else
    Result := 'f';
end;

procedure SRBool(a, b, c, d, e: Boolean);
begin
  WriteLn('    ', BC(a), ', ', BC(b), ', ', BC(c), ', ', BC(d), ', ', BC(e));
end;

var
  i8: Int8;
  ui8: UInt8;
  i16: Int16;
  ui16: UInt16;
  i32: Int32;
  ui32: UInt32;
  i64: Int64;
  ui64: UInt64;
  s: Integer;
  f0, f1, f2, f3, f4: Boolean;

begin
  WriteLn('test multiple shifts');

  for s := 0 to 8 * SizeOf(i8) - 1 do
  begin
    i8 := -1;
    i8 := WrapI8(LongInt(i8) shl s);
    ShowU8(UInt8(i8));

    ui8 := $ff;
    ui8 := WrapU8(LongInt(ui8) shl s);
    ShowU8(ui8);

    i8 := -1;
    i8 := WrapI8(LongInt(i8) shr s);
    ShowU8(UInt8(i8));

    ui8 := $ff;
    ui8 := WrapU8(LongInt(ui8) shr s);
    ShowU8(ui8);
  end;

  for s := 0 to 8 * SizeOf(i16) - 1 do
  begin
    i16 := -1;
    i16 := WrapI16(LongInt(i16) shl s);
    ShowU16(UInt16(i16));

    ui16 := $ffff;
    ui16 := WrapU16(LongInt(ui16) shl s);
    ShowU16(ui16);

    i16 := -1;
    i16 := WrapI16(LongInt(i16) shr s);
    ShowU16(UInt16(i16));

    ui16 := $ffff;
    ui16 := WrapU16(LongInt(ui16) shr s);
    ShowU16(ui16);
  end;

  for s := 0 to 8 * SizeOf(i32) - 1 do
  begin
    i32 := -1;
    i32 := WrapI32(Int64(i32) shl s);
    ShowU32(UInt32(i32));

    ui32 := UInt32($ffffffff);
    ui32 := WrapU32(Int64(ui32) shl s);
    ShowU32(ui32);

    i32 := -1;
    i32 := WrapI32(Int64(i32) shr s);
    ShowU32(UInt32(i32));

    ui32 := UInt32($ffffffff);
    ui32 := WrapU32(Int64(ui32) shr s);
    ShowU32(ui32);
  end;

  for s := 0 to 8 * SizeOf(i64) - 1 do
  begin
    i64 := -1;
    i64 := Int64(i64 shl s);
    ShowI64(i64);

    ui64 := UInt64($ffffffffffffffff);
    ui64 := UInt64(ui64 shl s);
    ShowU64(ui64);

    i64 := -1;
    i64 := Int64(i64 shr s);
    ShowI64(i64);

    ui64 := UInt64($ffffffffffffffff);
    ui64 := UInt64(ui64 shr s);
    ShowU64(ui64);
  end;

  WriteLn('test right shifts');

  i8 := -1;
  i8 := WrapI8(LongInt(i8) shr 1);
  ShowI8(i8);

  ui8 := $ff;
  ui8 := WrapU8(LongInt(ui8) shr 1);
  ShowU8(ui8);

  i16 := -1;
  i16 := WrapI16(LongInt(i16) shr 1);
  ShowI16(i16);

  ui16 := $ffff;
  ui16 := WrapU16(LongInt(ui16) shr 1);
  ShowU16(ui16);

  i32 := -1;
  i32 := WrapI32(Int64(i32) shr 1);
  ShowI32(i32);

  ui32 := UInt32($ffffffff);
  ui32 := WrapU32(Int64(ui32) shr 1);
  ShowU32(ui32);

  i64 := -1;
  i64 := Int64(i64 shr 1);
  ShowI64(i64);

  ui64 := UInt64($ffffffffffffffff);
  ui64 := UInt64(ui64 shr 1);
  ShowU64(ui64);

  WriteLn('now test left shifts');

  i8 := -1;
  i8 := WrapI8(LongInt(i8) shl 1);
  ShowU8(UInt8(i8));

  ui8 := $ff;
  ui8 := WrapU8(LongInt(ui8) shl 1);
  ShowU8(ui8);

  i16 := -1;
  i16 := WrapI16(LongInt(i16) shl 1);
  ShowU16(UInt16(i16));

  ui16 := $ffff;
  ui16 := WrapU16(LongInt(ui16) shl 1);
  ShowU16(ui16);

  i32 := -1;
  i32 := WrapI32(Int64(i32) shl 1);
  ShowU32(UInt32(i32));

  ui32 := UInt32($ffffffff);
  ui32 := WrapU32(Int64(ui32) shl 1);
  ShowU32(ui32);

  i64 := -1;
  i64 := Int64(i64 shl 1);
  ShowI64(i64);

  ui64 := UInt64($ffffffffffffffff);
  ui64 := UInt64(ui64 shl 1);
  ShowU64(ui64);

  WriteLn('now test comparisons. f, f, f, t, t expected');

  f0 := i8 = WrapI8(ui8);
  f1 := i8 > WrapI8(ui8);
  f2 := i8 >= WrapI8(ui8);
  f3 := i8 < WrapI8(ui8);
  f4 := i8 <= WrapI8(ui8);
  SRBool(f0, f1, f2, f3, f4);

  f0 := i16 = WrapI16(ui16);
  f1 := i16 > WrapI16(ui16);
  f2 := i16 >= WrapI16(ui16);
  f3 := i16 < WrapI16(ui16);
  f4 := i16 <= WrapI16(ui16);
  SRBool(f0, f1, f2, f3, f4);

  WriteLn('more test comparisons. t, f, t, f, t expected');

  f0 := UInt32(i32) = ui32;
  f1 := UInt32(i32) > ui32;
  f2 := UInt32(i32) >= ui32;
  f3 := UInt32(i32) < ui32;
  f4 := UInt32(i32) <= ui32;
  SRBool(f0, f1, f2, f3, f4);

  f0 := UInt64(i64) = ui64;
  f1 := UInt64(i64) > ui64;
  f2 := UInt64(i64) >= ui64;
  f3 := UInt64(i64) < ui64;
  f4 := UInt64(i64) <= ui64;
  SRBool(f0, f1, f2, f3, f4);

  f0 := i8 = i16;
  f1 := i8 > i16;
  f2 := i8 >= i16;
  f3 := i8 < i16;
  f4 := i8 <= i16;
  SRBool(f0, f1, f2, f3, f4);

  f0 := i16 = i32;
  f1 := i16 > i32;
  f2 := i16 >= i32;
  f3 := i16 < i32;
  f4 := i16 <= i32;
  SRBool(f0, f1, f2, f3, f4);

  f0 := i32 = i64;
  f1 := i32 > i64;
  f2 := i32 >= i64;
  f3 := i32 < i64;
  f4 := i32 <= i64;
  SRBool(f0, f1, f2, f3, f4);

  WriteLn('more test comparisons. f, f, f, t, t expected');

  f0 := i64 = ui8;
  f1 := i64 > ui8;
  f2 := i64 >= ui8;
  f3 := i64 < ui8;
  f4 := i64 <= ui8;
  SRBool(f0, f1, f2, f3, f4);

  WriteLn('more comparisons. f, f, f, t, t expected');

  f0 := i8 = 16;
  f1 := i8 > 16;
  f2 := i8 >= 16;
  f3 := i8 < 16;
  f4 := i8 <= 16;
  SRBool(f0, f1, f2, f3, f4);

  f0 := i16 = 32;
  f1 := i16 > 32;
  f2 := i16 >= 32;
  f3 := i16 < 32;
  f4 := i16 <= 32;
  SRBool(f0, f1, f2, f3, f4);

  f0 := i32 = 64;
  f1 := i32 > 64;
  f2 := i32 >= 64;
  f3 := i32 < 64;
  f4 := i32 <= 64;
  SRBool(f0, f1, f2, f3, f4);

  f0 := i64 = 8;
  f1 := i64 > 8;
  f2 := i64 >= 8;
  f3 := i64 < 8;
  f4 := i64 <= 8;
  SRBool(f0, f1, f2, f3, f4);

  WriteLn('testing printf');

  WriteLn('  string: ''hello''');
  WriteLn('  char: ''h''');
  WriteLn('  int: 27, ', UIntToStrBase(27, 16, 4));
  WriteLn('  negative int: -27, ', UIntToStrBase(UInt32(-27), 16, 4));
  WriteLn('  int64_t: 27, ', UIntToStrBase(27, 16, 8));
  WriteLn('  negative int64_t: -27, ', UIntToStrBase(UInt64(Int64(-27)), 16, 8));
  WriteLn('  float: ', 3.1415729:0:6);
  WriteLn('  negative float: ', -3.1415729:0:6);

  WriteLn('testing inttoa');

  WriteLn('  ui64toa: ', UIntToStrBase(UInt64($ffffffffffffffff), 10, 8));
  WriteLn('  i64toa: ', IntToStrBase(-1, 10, 8));
  WriteLn('  ui32toa: ', UIntToStrBase(UInt32($ffffffff), 10, 4));
  WriteLn('  i32toa: ', IntToStrBase(Int32(-1), 10, 4));
  WriteLn('  ui16toa: ', UIntToStrBase(UInt16($ffff), 10, 2));
  WriteLn('  i16toa: ', IntToStrBase(Int16(-1), 10, 2));
  WriteLn('  ui8toa: ', UIntToStrBase(UInt8($ff), 10, 1));
  WriteLn('  i8toa: ', IntToStrBase(Int8(-1), 10, 1));

  WriteLn('test shifts and comparisons completed with great success');
end.