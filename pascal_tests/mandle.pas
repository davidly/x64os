program mandle;

procedure mandle_single;
var
  cx, cy, c2, c4: Single;
  ca, cb, a, b, t: Single;
  x, y, i: Integer;
begin
  cx := 0.0458;
  cy := 0.08333;
  c2 := 2.0;
  c4 := 4.0;

  for y := -12 to 12 do
  begin
    for x := -39 to 39 do
    begin
      ca := x * cx;
      cb := y * cy;
      a := ca;
      b := cb;
      i := 0;
      while i <= 15 do
      begin
        t := a * a - b * b + ca;
        b := c2 * a * b + cb;
        a := t;
        if (a * a + b * b) > c4 then
          break;
        Inc(i);
      end;
      if i > 15 then
        Write(' ')
      else
      begin
        if i > 9 then
          i := i + 7;
        Write(Chr(48 + i));
      end;
    end;
    WriteLn;
  end;
end;

procedure mandle_double;
var
  cx, cy, c2, c4: Double;
  ca, cb, a, b, t: Double;
  x, y, i: Integer;
begin
  cx := 0.0458;
  cy := 0.08333;
  c2 := 2.0;
  c4 := 4.0;

  for y := -12 to 12 do
  begin
    for x := -39 to 39 do
    begin
      ca := x * cx;
      cb := y * cy;
      a := ca;
      b := cb;
      i := 0;
      while i <= 15 do
      begin
        t := a * a - b * b + ca;
        b := c2 * a * b + cb;
        a := t;
        if (a * a + b * b) > c4 then
          break;
        Inc(i);
      end;
      if i > 15 then
        Write(' ')
      else
      begin
        if i > 9 then
          i := i + 7;
        Write(Chr(48 + i));
      end;
    end;
    WriteLn;
  end;
end;

procedure mandle_extended;
var
  cx, cy, c2, c4: Extended;
  ca, cb, a, b, t: Extended;
  x, y, i: Integer;
begin
  cx := 0.0458;
  cy := 0.08333;
  c2 := 2.0;
  c4 := 4.0;

  for y := -12 to 12 do
  begin
    for x := -39 to 39 do
    begin
      ca := x * cx;
      cb := y * cy;
      a := ca;
      b := cb;
      i := 0;
      while i <= 15 do
      begin
        t := a * a - b * b + ca;
        b := c2 * a * b + cb;
        a := t;
        if (a * a + b * b) > c4 then
          break;
        Inc(i);
      end;
      if i > 15 then
        Write(' ')
      else
      begin
        if i > 9 then
          i := i + 7;
        Write(Chr(48 + i));
      end;
    end;
    WriteLn;
  end;
end;

begin
  mandle_single;
  mandle_double;
  mandle_extended;

  WriteLn('mandle completed with great success');
end.
