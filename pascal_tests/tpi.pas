program tpi;

{$mode objfpc}

const
  HIGH_MARK = 500;
  iterations = 1;

var
  r: array[0..HIGH_MARK] of Integer;
  i, k: Integer;
  b, d: Integer;
  iteration: Integer;
  c, pv: Integer;

begin
  for iteration := 0 to iterations - 1 do
  begin
    c := 0;

    for i := 0 to HIGH_MARK - 1 do
      r[i] := 2000;

    k := HIGH_MARK;
    while k > 0 do
    begin
      d := 0;
      i := k;
      repeat
        d := d + r[i] * 10000;
        b := 2 * i - 1;
        r[i] := d mod b;
        d := d div b;
        Dec(i);
        if i = 0 then break;
        d := d * i;
      until False;
      if iteration = iterations - 1 then
      begin
        pv := c + d div 10000;
        Write(pv div 1000, (pv div 100) mod 10, (pv div 10) mod 10, pv mod 10);
      end;
      c := d mod 10000;
      Dec(k, 14);
    end;
  end;

  WriteLn;
end.
