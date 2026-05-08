program tmuldiv;

{$mode objfpc}
{$R-}
{$Q-}

procedure test_i8(i8A, i8B: ShortInt);
var
  i8C: ShortInt;
begin
  i8C := i8A * i8B;
  WriteLn('i8 ', Integer(i8A), ' * ', Integer(i8B), ': ', Integer(i8C));
  i8C := i8A mod i8B;
  WriteLn('i8 ', Integer(i8A), ' % ', Integer(i8B), ': ', Integer(i8C));
  i8C := i8A div i8B;
  WriteLn('i8 ', Integer(i8A), ' / ', Integer(i8B), ': ', Integer(i8C));
end;

procedure test_ui8(ui8A, ui8B: Byte);
var
  ui8C: Byte;
begin
  ui8C := ui8A * ui8B;
  WriteLn('ui8 ', LongWord(ui8A), ' * ', LongWord(ui8B), ': ', LongWord(ui8C));
  ui8C := ui8A mod ui8B;
  WriteLn('ui8 ', LongWord(ui8A), ' % ', LongWord(ui8B), ': ', LongWord(ui8C));
  ui8C := ui8A div ui8B;
  WriteLn('ui8 ', LongWord(ui8A), ' / ', LongWord(ui8B), ': ', LongWord(ui8C));
end;

procedure test_i16(i16A, i16B: SmallInt);
var
  i16C: SmallInt;
begin
  i16C := i16A * i16B;
  WriteLn('i16 ', Integer(i16A), ' * ', Integer(i16B), ': ', Integer(i16C));
  i16C := i16A mod i16B;
  WriteLn('i16 ', Integer(i16A), ' % ', Integer(i16B), ': ', Integer(i16C));
  i16C := i16A div i16B;
  WriteLn('i16 ', Integer(i16A), ' / ', Integer(i16B), ': ', Integer(i16C));
end;

procedure test_ui16(ui16A, ui16B: Word);
var
  ui16C: Word;
begin
  ui16C := ui16A * ui16B;
  WriteLn('ui16 ', LongWord(ui16A), ' * ', LongWord(ui16B), ': ', LongWord(ui16C));
  ui16C := ui16A mod ui16B;
  WriteLn('ui16 ', LongWord(ui16A), ' % ', LongWord(ui16B), ': ', LongWord(ui16C));
  ui16C := ui16A div ui16B;
  WriteLn('ui16 ', LongWord(ui16A), ' / ', LongWord(ui16B), ': ', LongWord(ui16C));
end;

procedure test_i32(i32A, i32B: LongInt);
var
  i32C: LongInt;
begin
  i32C := i32A * i32B;
  WriteLn('i32 ', i32A, ' * ', i32B, ': ', i32C);
  i32C := i32A mod i32B;
  WriteLn('i32 ', i32A, ' % ', i32B, ': ', i32C);
  i32C := i32A div i32B;
  WriteLn('i32 ', i32A, ' / ', i32B, ': ', i32C);
end;

procedure test_ui32(ui32A, ui32B: LongWord);
var
  ui32C: LongWord;
begin
  ui32C := ui32A * ui32B;
  WriteLn('ui32 ', ui32A, ' * ', ui32B, ': ', ui32C);
  ui32C := ui32A mod ui32B;
  WriteLn('ui32 ', ui32A, ' % ', ui32B, ': ', ui32C);
  ui32C := ui32A div ui32B;
  WriteLn('ui32 ', ui32A, ' / ', ui32B, ': ', ui32C);
end;

procedure test_i64(i64A, i64B: Int64);
var
  i64C: Int64;
begin
  i64C := i64A * i64B;
  WriteLn('i64 ', i64A, ' * ', i64B, ': ', i64C);
  i64C := i64A mod i64B;
  WriteLn('i64 ', i64A, ' % ', i64B, ': ', i64C);
  i64C := i64A div i64B;
  WriteLn('i64 ', i64A, ' / ', i64B, ': ', i64C);
end;

procedure test_ui64(ui64A, ui64B: QWord);
var
  ui64C: QWord;
begin
  ui64C := ui64A * ui64B;
  WriteLn('ui64 ', ui64A, ' * ', ui64B, ': ', ui64C);
  ui64C := ui64A mod ui64B;
  WriteLn('ui64 ', ui64A, ' % ', ui64B, ': ', ui64C);
  ui64C := ui64A div ui64B;
  WriteLn('ui64 ', ui64A, ' / ', ui64B, ': ', ui64C);
end;

begin
  WriteLn('app start');

  test_i8(3, 14);
  test_i8(17, 14);
  test_i8(-3, 14);
  test_i8(-17, 14);
  test_i8(-3, -14);
  test_i8(-17, -14);
  test_i8(28, 4);
  test_i8(28, -4);
  test_i8(-28, 4);
  test_i8(-28, -4);

  test_ui8(3, 14);
  test_ui8(17, 14);
  test_ui8(Byte(-3), 14);
  test_ui8(Byte(-17), 14);
  test_ui8(Byte(-3), Byte(-14));
  test_ui8(Byte(-17), Byte(-14));
  test_ui8(28, 4);
  test_ui8(28, Byte(-4));
  test_ui8(Byte(-28), 4);
  test_ui8(Byte(-28), Byte(-4));

  test_i16(3, 14);
  test_i16(3700, 14);
  test_i16(-3, 14);
  test_i16(-3700, 14);
  test_i16(-3, -14);
  test_i16(-3700, -14);
  test_i16(2800, 4);
  test_i16(2800, -4);
  test_i16(-2800, 4);
  test_i16(-2800, -4);

  test_ui16(3, 14);
  test_ui16(3700, 14);
  test_ui16(Word(-3), 14);
  test_ui16(Word(-3700), 14);
  test_ui16(Word(-3), Word(-14));
  test_ui16(Word(-3700), Word(-14));
  test_ui16(2800, 4);
  test_ui16(2800, Word(-4));
  test_ui16(Word(-2800), 4);
  test_ui16(Word(-2800), Word(-4));

  test_i32(3, 14);
  test_i32(37000, 14);
  test_i32(-3, 14);
  test_i32(-37000, 14);
  test_i32(-3, -14);
  test_i32(-37000, -14);
  test_i32(280000, 4);
  test_i32(280000, -4);
  test_i32(-280000, 4);
  test_i32(-280000, -4);

  test_ui32(3, 14);
  test_ui32(37000, 14);
  test_ui32(LongWord(-3), 14);
  test_ui32(LongWord(-37000), 14);
  test_ui32(LongWord(-3), LongWord(-14));
  test_ui32(LongWord(-37000), LongWord(-14));
  test_ui32(280000, 4);
  test_ui32(280000, LongWord(-4));
  test_ui32(LongWord(-280000), 4);
  test_ui32(LongWord(-280000), LongWord(-4));

  test_i64(3, 14);
  test_i64(370000000, 14);
  test_i64(-3, 14);
  test_i64(-370000000, 14);
  test_i64(-3, -14);
  test_i64(-370000000, -14);
  test_i64(28000000000000, 4);
  test_i64(28000000000000, -4);
  test_i64(-28000000000000, 4);
  test_i64(-28000000000000, -4);

  test_ui64(3, 14);
  test_ui64(370000000, 14);
  test_ui64(QWord(-3), 14);
  test_ui64(QWord(-370000000), 14);
  test_ui64(QWord(-3), QWord(-14));
  test_ui64(QWord(-370000000), QWord(-14));
  test_ui64(28000000000000, 4);
  test_ui64(28000000000000, QWord(-4));
  test_ui64(QWord(-28000000000000), 4);
  test_ui64(QWord(-28000000000000), QWord(-4));

  WriteLn('tmuldiv ended with great success');
end.
