program pis;

{$mode objfpc}
{$R-}
{$Q-}
{$inline on}

uses
  SysUtils;

function fpart_nomod(x: Double): Double;
begin
  if x >= 1.0 then
    Result := x - 1.0
  else
    Result := x;
end;

function fpart(x: Double): Double;
begin
  Result := Frac(x);
  if Result < 0.0 then
    Result := 1.0 + Result;
end;

function memeq(p1, p2: Pointer; n: SizeUInt): Boolean;
var
  b1: PByte absolute p1;
  b2: PByte absolute p2;
  i: SizeUInt;
begin
  i := 0;
  while i < n do
  begin
    if b1[i] <> b2[i] then begin Result := False; Exit; end;
    Inc(i);
  end;
  Result := True;
end;

{ d is always in [0,1); incrementing the bit pattern gives the next larger double }
function eps(d: Double): Double;
var
  bits: QWord;
begin
  Move(d, bits, SizeOf(Double));
  Inc(bits);
  Move(bits, Result, SizeOf(Double));
  Result := Result - d;
end;

function powermod16_faster(e, m: SizeUInt): SizeUInt;
var
  b: SizeUInt;
begin
  if m = 1 then begin Result := 0; Exit; end;
  if e = 0 then begin Result := 1; Exit; end;
  Result := 1;
  b := 16 mod m;
  repeat
    if (e and 1) <> 0 then
      Result := (Result * b) mod m;
    e := e shr 1;
    if e = 0 then Exit;
    b := (b * b) mod m;
  until False;
end;

function fun(n, j: SizeUInt): Double;
var
  s: Double;
  denom, k, p: SizeUInt;
  num, fdenom, frac: Double;
begin
  s := 0.0;
  denom := j;
  k := 0;
  while k <= n do
  begin
    p := powermod16_faster(n - k, denom);
    s := fpart_nomod(s + Double(p) / Double(denom));
    Inc(denom, 8);
    Inc(k);
  end;
  num := 1.0 / 16.0;
  fdenom := Double(denom);
  frac := num / fdenom;
  while frac > eps(s) do
  begin
    s := s + frac;
    num := num / 16.0;
    fdenom := fdenom + 8.0;
    frac := num / fdenom;
  end;
  Result := fpart_nomod(s);
end;

function pi_digit(n: SizeUInt): Integer;
var
  sum, f, r: Double;
begin
  sum := (4.0 * fun(n, 1)) - (2.0 * fun(n, 4)) - fun(n, 5) - fun(n, 6);
  f := fpart(sum);
  r := 16.0 * f;
  Result := Integer(Trunc(r));
  if (Result < 0) or (Result > 15) then
    WriteLn(Format('resulting sum %f, f %f, r %f, x: %d', [sum, f, r, Result]));
end;

procedure Usage;
begin
  WriteLn('Usage: pis [offset] [count]');
  WriteLn('  PI source. Generates hexadecimal digits of PI.');
  WriteLn('  arguments:  [offset]    Offset in 128 where generation starts. Default is 0.');
  WriteLn('              [count]     Count in 128 of digits to generate. Default is 1.');
  Halt(1);
end;

const
  Julia1k: AnsiString =
    '243f6a8885a308d313198a2e03707344a4093822299f31d0082efa98ec4e6c89452821e638d01377be54' +
    '66cf34e90c6cc0ac29b7c97c50dd3f84d5b5b54709179216d5d98979fb1bd1310ba698dfb5ac2ffd72db' +
    'd01adfb7b8e1afed6a267e96ba7c9045f12c7f9924a19947b3916cf70801f2e2858efc16636920d87157' +
    '4e69a458fea3f4933d7e0d95748f728eb658718bcd5882154aee7b54a41dc25a59b59c30d5392af26013' +
    'c5d1b023286085f0ca417918b8db38ef8e79dcb0603a180e6c9e0e8bb01e8a3ed71577c1bd314b2778af' +
    '2fda55605c60e65525f3aa55ab945748986263e8144055ca396a2aab10b6b4cc5c341141e8cea15486af' +
    '7c72e993b3ee1411636fbc2a2ba9c55d741831f6ce5c3e169b87931eafd6ba336c24cf5c7a3253812895' +
    '86773b8f48986b4bb9afc4bfe81b6628219361d809ccfb21a991487cac605dec8032ef845d5de98575b1' +
    'dc262302eb651b8823893e81d396acc50f6d6ff383f442392e0b4482a484200469c8f04a9e1f9b5e21c6' +
    '6842f6e96c9a670c9c61abd388f06a51a0d2d8542f68960fa728ab5133a36eef0b6c137a3be4ba3bf050' +
    '7efb2a98a1f1651d39af017666ca593e82430e888cee8619456f9fb47d84a5c33b8b5ebee06f75d885c1' +
    '2073401a449f56c16aa64ed3aa62363f77061bfedf72429b023d37d0d724d00a1248db0fead349f1c09b' +
    '075372c980991b7b';

var
  startingOffset, startingOffset128, countGenerated128, countGenerated: SizeUInt;
  chunkSize, startInChunks, limitInChunks: SizeUInt;
  complete, generatedChunks: SizeUInt;
  ci, d, chunk_start: SizeUInt;
  x: Integer;
  hc: AnsiChar;
  ac: array of AnsiChar;
  bufsize: SizeUInt;

begin
  startingOffset   := 0;
  startingOffset128 := 0;
  countGenerated128 := 1;
  countGenerated   := 128;

  if ParamCount > 2 then
    Usage;

  if ParamCount >= 1 then
  begin
    startingOffset128 := SizeUInt(StrToInt64(ParamStr(1)));
    startingOffset    := startingOffset128 * 128;
  end;

  if ParamCount = 2 then
  begin
    countGenerated128 := SizeUInt(StrToInt64(ParamStr(2)));
    countGenerated    := 128 * countGenerated128;
  end;

  WriteLn(Format('startingOffset128: %d, startingOffset: %d, countGenerated128 %d, countGenerated %d',
    [Int64(startingOffset128), Int64(startingOffset),
     Int64(countGenerated128), Int64(countGenerated)]));

  bufsize := 1 + countGenerated;
  SetLength(ac, bufsize);
  FillChar(ac[0], bufsize, 0);

  chunkSize      := 32;
  startInChunks  := (startingOffset128 * 128) div chunkSize;
  limitInChunks  := (startInChunks + (countGenerated128 * 128)) div chunkSize;

  WriteLn(Format('startInChunks: %d, limitInChunks %d',
    [Int64(startInChunks), Int64(limitInChunks)]));

  complete        := 0;
  generatedChunks := countGenerated128 * 129 div chunkSize;

  ci := startInChunks;
  while ci < limitInChunks do
  begin
    chunk_start := ci * chunkSize;
    d := chunk_start;
    while d < chunk_start + chunkSize do
    begin
      x := pi_digit(d);
      if x <= 9 then
        hc := AnsiChar(Ord('0') + x)
      else
        hc := AnsiChar(Ord('a') + x - 10);
      ac[d - startingOffset] := hc;
      Inc(d);
    end;
    Inc(complete);
    WriteLn(Format('percent complete: %.6f',
      [100.0 * Double(complete) / Double(generatedChunks)]));
    Inc(ci);
  end;

  if (startingOffset = 0) and (countGenerated128 >= 1) then
  begin
    if (countGenerated128 >= 8) and
       not memeq(@ac[0], PAnsiChar(Julia1k), 1024) then
      WriteLn('results 1k don''t match Julia!')
    else if not memeq(@ac[0], PAnsiChar(Julia1k), 128) then
      WriteLn('results 128 don''t match Julia!')
    else
      WriteLn('results are valid');
  end;

  WriteLn('final: ', PAnsiChar(@ac[0]));
end.
