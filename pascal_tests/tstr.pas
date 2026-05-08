program tstr;

{$mode objfpc}

uses
  SysUtils, Strings;

var
  ac: array[0..4095] of AnsiChar;
  other_arr: array[0..4095] of AnsiChar;
  zeroes: array[0..4095] of AnsiChar;
  wc_arr: array[0..4095] of WideChar;

function my_wcslen(p: PWideChar): SizeInt;
begin
  Result := 0;
  while p^ <> #0 do
  begin
    Inc(Result);
    Inc(p);
  end;
end;

function my_wcschr(str: PWideChar; c: WideChar): PWideChar;
begin
  while str^ <> #0 do
  begin
    if str^ = c then
    begin
      Result := str;
      Exit;
    end;
    Inc(str);
  end;
  Result := nil;
end;

function my_wcsrchr(str: PWideChar; c: WideChar): PWideChar;
begin
  Result := nil;
  while str^ <> #0 do
  begin
    if str^ = c then
      Result := str;
    Inc(str);
  end;
end;

function my_wcsstr(haystack, needle: PWideChar): PWideChar;
var
  h, n: PWideChar;
begin
  if needle^ = #0 then
  begin
    Result := haystack;
    Exit;
  end;
  while haystack^ <> #0 do
  begin
    h := haystack;
    n := needle;
    while (h^ <> #0) and (n^ <> #0) and (h^ = n^) do
    begin
      Inc(h);
      Inc(n);
    end;
    if n^ = #0 then
    begin
      Result := haystack;
      Exit;
    end;
    Inc(haystack);
  end;
  Result := nil;
end;

procedure test_wide;
var
  i, k, start, e_pos, len, offset: Integer;
  orig: WideChar;
  pbang: PWideChar;
  slen: SizeInt;
  walpha: array[0..26] of WideChar;
  pattern: PWideChar;
  p: PWideChar;
begin
  for i := 0 to 4095 do
    wc_arr[i] := WideChar(Ord('a') + (i mod 26));

  WriteLn('testing wcslen');
  for i := 0 to 999 do
  begin
    start := Random(300);
    e_pos := 1 + start + Random(3000);
    len := e_pos - start;
    orig := wc_arr[e_pos];
    wc_arr[e_pos] := #0;
    slen := my_wcslen(@wc_arr[start]);
    if len <> slen then
    begin
      WriteLn(Format('wcslen failed: iteration %d, len %d, wcslen %d, start %d, end %d', [i, len, slen, start, e_pos]));
      Halt(1);
    end;
    wc_arr[e_pos] := orig;
  end;

  WriteLn('testing wcschr and wcsrchr');
  for i := 0 to 999 do
  begin
    start := Random(300);
    e_pos := 1 + start + Random(70);
    len := e_pos - start;
    orig := wc_arr[e_pos];
    wc_arr[e_pos] := '!';
    pbang := my_wcschr(@wc_arr[start], '!');
    if pbang = nil then
    begin
      WriteLn(Format('wcschr failed to find char: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
      Halt(1);
    end;
    if pbang <> @wc_arr[e_pos] then
    begin
      WriteLn(Format('wcschr offset incorrect: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
      Halt(1);
    end;
    pbang := my_wcsrchr(@wc_arr[start], '!');
    if pbang = nil then
    begin
      WriteLn(Format('wcsrchr failed to find char: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
      Halt(1);
    end;
    if pbang <> @wc_arr[e_pos] then
    begin
      WriteLn(Format('wcsrchr offset incorrect: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
      Halt(1);
    end;
    wc_arr[e_pos] := orig;
  end;

  WriteLn('testing wcsstr');
  for k := 0 to 25 do
    walpha[k] := WideChar(Ord('a') + k);
  walpha[26] := #0;

  for i := 0 to 999 do
  begin
    start := Random(300);
    offset := Random(26);
    len := 1 + Random(26 - offset);
    if (offset + len) > 26 then
    begin
      WriteLn(Format('test bug offset %d, len %d', [offset, len]));
      Halt(1);
    end;
    pattern := @walpha[offset];
    p := my_wcsstr(@wc_arr[start], pattern);
    if p = nil then
    begin
      WriteLn(Format('wcsstr pattern not found iteration %d, start %d, offset %d, len %d', [i, start, offset, len]));
      Halt(1);
    end;
    if not CompareMem(p, pattern, len * SizeOf(WideChar)) then
    begin
      WriteLn(Format('wcsstr found the wrong pattern iteration %d, start %d, offset %d, len %d', [i, start, offset, len]));
      Halt(1);
    end;
  end;
end;

const
  gfe_needle: PAnsiChar = 'gfe';

var
  loop_count, loops, i: Integer;
  start, e_pos, len, offset: Integer;
  orig_c: AnsiChar;
  slen: SizeInt;
  pbang_c: PAnsiChar;
  alpha: array[0..26] of AnsiChar;
  pattern_c, p_c: PAnsiChar;
  l: SizeInt;

begin
  if ParamCount > 0 then
    loop_count := StrToIntDef(ParamStr(1), 1)
  else
    loop_count := 1;

  for loops := 0 to loop_count - 1 do
  begin
    for i := 0 to 4095 do
      ac[i] := AnsiChar(Ord('a') + (i mod 26));

    WriteLn('testing strlen');
    for i := 0 to 999 do
    begin
      start := Random(300);
      e_pos := 1 + start + Random(3000);
      len := e_pos - start;
      orig_c := ac[e_pos];
      ac[e_pos] := #0;
      slen := StrLen(@ac[start]);
      if len <> slen then
      begin
        WriteLn(Format('strlen failed: iteration %d, len %d, strlen %d, start %d, end %d', [i, len, slen, start, e_pos]));
        Halt(1);
      end;
      ac[e_pos] := orig_c;
    end;

    WriteLn('testing strchr and strrchr');
    for i := 0 to 999 do
    begin
      start := Random(300);
      e_pos := 1 + start + Random(70);
      len := e_pos - start;
      orig_c := ac[e_pos];
      ac[e_pos] := '!';
      pbang_c := StrScan(@ac[start], '!');
      if pbang_c = nil then
      begin
        WriteLn(Format('strchr failed to find char: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
        Halt(1);
      end;
      if pbang_c <> @ac[e_pos] then
      begin
        WriteLn(Format('strchr offset incorrect: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
        Halt(1);
      end;
      pbang_c := StrRScan(@ac[start], '!');
      if pbang_c = nil then
      begin
        WriteLn(Format('strrchr failed to find char: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
        Halt(1);
      end;
      if pbang_c <> @ac[e_pos] then
      begin
        WriteLn(Format('strrchr offset incorrect: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
        Halt(1);
      end;
      if StrScan(@ac[start], '$') <> nil then
      begin
        WriteLn(Format('strrchr somehow found $: iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
        Halt(1);
      end;
      ac[e_pos] := orig_c;
    end;

    WriteLn('testing strstr');
    StrPCopy(@alpha[0], 'abcdefghijklmnopqrstuvwxyz');
    for i := 0 to 999 do
    begin
      start := Random(300);
      offset := Random(26);
      len := 1 + Random(26 - offset);
      if (offset + len) > 26 then
      begin
        WriteLn(Format('test bug offset %d, len %d', [offset, len]));
        Halt(1);
      end;
      pattern_c := @alpha[offset];
      p_c := StrPos(@ac[start], pattern_c);
      if p_c = nil then
      begin
        WriteLn(Format('strstr pattern not found iteration %d, start %d, offset %d, len %d, pattern %s',
          [i, start, offset, len, AnsiString(pattern_c)]));
        Halt(1);
      end;
      if not CompareMem(p_c, pattern_c, len) then
      begin
        WriteLn(Format('strstr found the wrong pattern iteration %d, start %d, offset %d, len %d, pattern %s',
          [i, start, offset, len, AnsiString(pattern_c)]));
        Halt(1);
      end;
      if StrPos(@ac[start], gfe_needle) <> nil then
      begin
        WriteLn(Format('strstr somehow found gfe. iteration %d, start %d, offset %d', [i, start, offset]));
        Halt(1);
      end;
    end;

    WriteLn('testing memcpy and memcmp');
    for i := 0 to 999 do
    begin
      start := Random(300);
      e_pos := 1 + start + Random(3000);
      len := e_pos - start;
      Move(ac[start], other_arr[start], len);
      if not CompareMem(@other_arr[start], @ac[start], len) then
      begin
        WriteLn(Format('memcmp of memcpy''ed memory failed to find match, iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
        Halt(1);
      end;
      FillByte(other_arr[start], len, 0);
      if not CompareMem(@other_arr[start], @zeroes[start], len) then
      begin
        WriteLn(Format('zeroes not found in zero-filled memory, iteration %d, len %d, start %d, end %d', [i, len, start, e_pos]));
        Halt(1);
      end;
    end;

    WriteLn('testing printf');
    for i := 0 to 19 do
    begin
      start := Random(300);
      e_pos := 1 + start + Random(70);
      len := e_pos - start;
      orig_c := ac[e_pos];
      ac[e_pos] := #0;
      l := StrLen(@ac[start]);
      WriteLn(Format('%2d (%2d): %s', [len, l, AnsiString(PAnsiChar(@ac[start]))]));
      ac[e_pos] := orig_c;
    end;

    test_wide;
  end;

  WriteLn('tstr completed with great success');
end.
