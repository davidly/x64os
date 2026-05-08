program tf;

{$mode objfpc}

uses
  Math, SysUtils;

const
  TRIG_FLT_EPSILON  = 0.00002;
  TRIG_DBL_EPSILON  = 0.00000002;
  TRIG_LDBL_EPSILON = 0.0000000000002;
  FLT_EPSILON = 1.1920928955078125e-7;
  DBL_EPSILON = 2.2204460492503131e-16;
  LDBL_EPSILON = 1.0842021724855044e-19;
  max_N_Iterations = 10;

function floattoa(d: Double; precision: Integer): string;
var
  whole: LongWord;
  fraction: Double;
begin
  Result := '';
  if d < 0 then
  begin
    Result := '-';
    d := -d;
  end;
  whole := LongWord(Trunc(Single(d)));
  Result := Result + IntToStr(whole);
  if precision > 0 then
  begin
    Result := Result + '.';
    fraction := d - whole;
    while precision > 0 do
    begin
      fraction := fraction * 10.0;
      whole := LongWord(Trunc(fraction));
      Result := Result + Chr(Ord('0') + whole);
      fraction := fraction - whole;
      Dec(precision);
    end;
  end;
end;

procedure check_same_f(const operation: string; a, b, dbgval: Single);
begin
  if Abs(a - b) > TRIG_FLT_EPSILON then
  begin
    WriteLn(Format('operation %s: float %.20f is not the same as float %.20f', [operation, Double(a), Double(b)]));
    WriteLn(Format('  original value: %.20f', [Double(dbgval)]));
    Halt(0);
  end;
end;

procedure check_same_d(const operation: string; a, b, dbgval: Double);
begin
  if Abs(a - b) > TRIG_DBL_EPSILON then
  begin
    WriteLn(Format('operation %s: double %.20f is not the same as double %.20f', [operation, a, b]));
    WriteLn(Format('  original value: %.20f', [dbgval]));
    Halt(0);
  end;
end;

procedure check_same_ld(const operation: string; a, b, dbgval: Extended);
begin
  if Abs(a - b) > TRIG_LDBL_EPSILON then
  begin
    WriteLn(Format('operation %s: long double %.20f is not the same as long double %.20f', [operation, Double(a), Double(b)]));
    WriteLn(Format('  original value: %.20f', [Double(dbgval)]));
    Halt(0);
  end;
end;

function factorial(n: Int64): Int64;
begin
  if n = 0 then
    Result := 1
  else
    Result := n * factorial(n - 1);
end;

function my_sin_ld(x: Extended; n: Integer = max_N_Iterations): Extended;
var
  res: Extended;
  sign, i: Integer;
begin
  res := 0.0;
  sign := 1;
  for i := 1 to n do
  begin
    res := res + Extended(sign) * IntPower(x, Integer(2*i - 1)) / factorial(2*i - 1);
    sign := -sign;
  end;
  Result := res;
end;

function my_sin_d(x: Double; n: Integer = max_N_Iterations): Double;
var
  res: Extended;
  sign, i: Integer;
begin
  res := 0.0;
  sign := 1;
  for i := 1 to n do
  begin
    res := res + Extended(sign) * IntPower(Extended(x), Integer(2*i - 1)) / factorial(2*i - 1);
    sign := -sign;
  end;
  Result := Double(res);
end;

function my_sin_f(x: Single; n: Integer = max_N_Iterations): Single;
var
  res: Extended;
  sign, i: Integer;
begin
  res := 0.0;
  sign := 1;
  for i := 1 to n do
  begin
    res := res + Extended(sign) * IntPower(Extended(x), Integer(2*i - 1)) / factorial(2*i - 1);
    sign := -sign;
  end;
  Result := Single(res);
end;

procedure many_trigonometrics;
var
  f, limit, f_cos: Single;
  fresult, fback: Single;
  dresult, dback: Double;
  ldresult, ldback: Extended;
begin
  f := 0.01 - (Pi / 2);
  limit := (Pi / 2) - 0.01;

  while f < limit do
  begin
    fresult := Tan(f);
    fback := ArcTan(fresult);
    check_same_f('tan', f, fback, fresult);

    dresult := Tan(Double(f));
    dback := ArcTan(dresult);
    check_same_d('tan', Double(f), dback, fresult);

    ldresult := Tan(Extended(f));
    ldback := ArcTan(ldresult);
    check_same_ld('tan', Extended(f), ldback, ldresult);

    fresult := Sin(f);
    fback := my_sin_f(f);
    check_same_f('sin vs my_sin', fresult, fback, f);

    fresult := Sin(f);
    fback := ArcSin(fresult);
    check_same_f('sin', f, fback, fresult);

    fresult := my_sin_f(f);
    fback := ArcSin(fresult);
    check_same_f('my sin', f, fback, fresult);

    dresult := Sin(Double(f));
    dback := ArcSin(dresult);
    check_same_d('sin', Double(f), dback, dresult);

    dresult := my_sin_d(Double(f));
    dback := ArcSin(dresult);
    check_same_d('my sin', Double(f), dback, dresult);

    ldresult := Sin(Extended(f));
    ldback := my_sin_ld(Extended(f));
    check_same_ld('sinl vs my_sinl', ldresult, ldback, f);

    ldresult := Sin(Extended(f));
    ldback := ArcSin(ldresult);
    check_same_ld('sin', Extended(f), ldback, ldresult);

    ldresult := my_sin_ld(Extended(f));
    ldback := ArcSin(ldresult);
    check_same_ld('my sin', Extended(f), ldback, ldresult);

    f_cos := f + (Pi / 2);

    fresult := Cos(f_cos);
    fback := ArcCos(fresult);
    check_same_f('cos', f_cos, fback, fresult);

    dresult := Cos(Double(f_cos));
    dback := ArcCos(dresult);
    check_same_d('cos', Double(f_cos), dback, dresult);

    ldresult := Cos(Extended(f_cos));
    ldback := ArcCos(ldresult);
    check_same_ld('cos', Extended(f_cos), ldback, ldresult);

    f := f + 0.032;
  end;
end;

function square_root_f(num: Single): Single;
var
  x, y, e: Single;
begin
  x := num;
  y := 1.0;
  e := 10.0 * FLT_EPSILON;
  while (x - y) > e do
  begin
    x := (x + y) / 2;
    y := num / x;
  end;
  Result := x;
end;

function square_root_d(num: Double): Double;
var
  x, y, e: Double;
begin
  x := num;
  y := 1.0;
  e := 10.0 * DBL_EPSILON;
  while (x - y) > e do
  begin
    x := (x + y) / 2;
    y := num / x;
  end;
  Result := x;
end;

function square_root_ld(num: Extended): Extended;
var
  x, y, e: Extended;
begin
  x := num;
  y := 1.0;
  e := 10.0 * LDBL_EPSILON;
  while (x - y) > e do
  begin
    x := (x + y) / 2;
    y := num / x;
  end;
  Result := x;
end;

procedure fl_cl_test;
var
  f1_1, f1_8: Single;
  x_int: LongInt;
begin
  f1_1 := 1.1;
  f1_8 := 1.8;

  x_int := Floor(f1_1);
  WriteLn(Format('floor of 1.1: %f == %d', [Double(x_int), x_int]));

  x_int := Ceil(f1_1);
  WriteLn(Format('ceil of 1.1: %f == %d', [Double(x_int), x_int]));

  x_int := Floor(-f1_8);
  WriteLn(Format('floor of -1.8: %f == %d', [Double(x_int), x_int]));

  x_int := Ceil(-f1_8);
  WriteLn(Format('ceil of -1.8: %f == %d', [Double(x_int), x_int]));
end;

var
  f2, fm1, fr, fd, fs: Single;
  pi_f, radians: Single;
  s_f, sh_f, c_f, ch_f, t_f: Single;
  f_val, at_f: Single;
  b_val, a_val: Single;
  f_sqrt: Single;
  d_val: Double;
  d_sqrt: Double;
  ld_sqrt: Extended;
  mantissa_e: Extended;
  exponent_i: Integer;

begin
  WriteLn('float converted by floattoa: ', floattoa(-1.234567, 8));
  WriteLn('float converted by floattoa: ', floattoa(1.234567, 8));
  WriteLn('float converted by floattoa: ', floattoa(34.567, 8));

  WriteLn(Format('float from printf: %f', [45.678]));

  f2 := 20.2;
  fm1 := -1.342;
  fr := f2 * fm1;
  fd := 1000.0 / 3.0;
  fs := Sqrt(fd);

  WriteLn(Format('division result: %f, square root %f', [Double(fd), Double(fs)]));
  WriteLn('float converted with floattoa: ', floattoa(fr, 6));
  WriteLn(Format('result of 20.2 * -1.342: %f', [Double(fr)]));

  d_val := Double(fr);
  WriteLn(Format('result of 20.2 * -1.342 as a double: %f', [d_val]));

  pi_f := Pi;
  radians := pi_f / 180.0 * 30.0;
  WriteLn(Format('pi in radians: %f', [Double(radians)]));

  s_f := Sin(radians);
  WriteLn(Format('sinf of 30 degress is %f', [Double(s_f)]));

  sh_f := SinH(0.5);
  WriteLn(Format('sinhf of 0.5 is %f', [Double(sh_f)]));

  c_f := Cos(radians);
  WriteLn(Format('cosf of 30 degrees is %f', [Double(c_f)]));

  ch_f := CosH(0.5);
  WriteLn(Format('cosh of 0.5 (in radians) is %f', [Double(ch_f)]));

  t_f := Tan(radians);
  WriteLn(Format('tanf of 30 degrees is %f', [Double(t_f)]));

  f_val := 1.0;
  at_f := ArcTan(f_val);
  WriteLn(Format('atanf of %f is %f', [Double(f_val), Double(at_f)]));

  at_f := ArcTan2(0.3, 0.2);
  WriteLn(Format('atan2f of 0.3, 0.2 is %f', [Double(at_f)]));

  c_f := ArcCos(0.3);
  WriteLn(Format('acosf of 0.3 is %f', [Double(c_f)]));

  s_f := ArcSin(0.3);
  WriteLn(Format('asinf of 0.3 is %f', [Double(s_f)]));

  f_val := TanH(2.2);
  WriteLn(Format('tanhf of 2.2 is %f', [Double(f_val)]));

  f_val := Ln(0.3);
  WriteLn(Format('logf of 0.3: %f', [Double(f_val)]));

  f_val := Log10(300.0);
  WriteLn(Format('log10f of 300: %f', [Double(f_val)]));

  Frexp(Extended(pi_f), mantissa_e, exponent_i);
  WriteLn(Format('pi has mantissa: %f, exponent %d', [Double(mantissa_e), exponent_i]));

  fl_cl_test;

  b_val := 2.7;
  a_val := 2.0;
  while a_val < 3.0 do
  begin
    if a_val > b_val  then Write('g,');
    if a_val >= b_val then Write('ge,');
    if a_val = b_val  then Write('eq,');
    if a_val < b_val  then Write('l,');
    if a_val <= b_val then Write('le,');
    a_val := a_val + 0.1;
  end;
  WriteLn;

  many_trigonometrics;

  f_sqrt := 1.0;
  while f_sqrt < 100.0 do
  begin
    check_same_f('square root float', square_root_f(f_sqrt), Sqrt(f_sqrt), f_sqrt);
    f_sqrt := f_sqrt + 1.38;
  end;

  d_sqrt := 1.0;
  while d_sqrt < 100.0 do
  begin
    check_same_d('square root double', square_root_d(d_sqrt), Sqrt(d_sqrt), d_sqrt);
    d_sqrt := d_sqrt + 1.38;
  end;

  ld_sqrt := 1.0;
  while ld_sqrt < 100.0 do
  begin
    check_same_ld('square root long double', square_root_ld(ld_sqrt), Sqrt(ld_sqrt), ld_sqrt);
    ld_sqrt := ld_sqrt + 1.38;
  end;

  WriteLn('test tf completed with great success');
  Halt(0);
end.
