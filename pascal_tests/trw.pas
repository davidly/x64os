program trw;

{$mode objfpc}

uses
  BaseUnix, Unix, SysUtils;

const
  BUF_ELEMENTS = 32;
  BUF_SIZE = BUF_ELEMENTS * SizeOf(SmallInt);

var
  buf: array[0..BUF_ELEMENTS - 1] of SmallInt;

procedure show_error(const str: string);
begin
  WriteLn('error: ', str, ', errno: ', fpGetErrno);
  Halt(1);
end;

var
  i, j: Integer;
  res: TSsize;
  seek_offset, file_offset: TOff;
  fd: cInt;
  buf_bytes: cInt;

begin
  WriteLn('O_CREATE ', IntToHex(O_CREAT, 1));
  WriteLn('O_RDWR ', IntToHex(O_RDWR, 1));
  WriteLn('O_TRUNC ', IntToHex(O_TRUNC, 1));
  Flush(Output);

  fd := fpOpen('trw.dat', O_CREAT or O_RDWR or O_TRUNC, $1ff);
  if fd = -1 then
    show_error('unable to create data file');

  buf_bytes := SizeOf(buf);

  for i := 0 to 4095 do
  begin
    for j := 0 to BUF_ELEMENTS - 1 do
      buf[j] := SmallInt(i);

    res := fpWrite(fd, @buf, buf_bytes);
    if res <> buf_bytes then
      show_error('unable to write to file');
  end;

  fpFSync(fd);
  fpClose(fd);

  fd := fpOpen('trw.dat', O_RDONLY);
  if fd = -1 then
    show_error('unable to open data file read only');

  for i := 0 to 4095 do
  begin
    FillByte(buf, buf_bytes, $69);
    res := fpRead(fd, @buf, buf_bytes);
    if res <> buf_bytes then
    begin
      WriteLn('result: ', res, ', i ', i);
      show_error('unable to read from file at point A');
    end;

    for j := 0 to BUF_ELEMENTS - 1 do
    begin
      if buf[j] <> i then
      begin
        WriteLn('i ', IntToHex(i, 1), ', j ', IntToHex(j, 1), ', buf[j] ', IntToHex(buf[j] and $ffff, 4));
        show_error('data read from file isn''t what was expected (should be i) at point A');
      end;
    end;
  end;

  fpClose(fd);

  fd := fpOpen('trw.dat', O_RDWR);
  if fd = -1 then
    show_error('unable to open data file read/write');

  for i := 0 to 4095 do
  begin
    if (i mod 8) = 0 then
    begin
      seek_offset := TOff(i) * BUF_SIZE;
      file_offset := fpLSeek(fd, seek_offset, SEEK_SET);
      if file_offset <> seek_offset then
      begin
        WriteLn('file_offset ', file_offset, ', seek_offset ', seek_offset);
        show_error('lseek location not as expected');
      end;

      for j := 0 to BUF_ELEMENTS - 1 do
        buf[j] := SmallInt(i + $4000);

      res := fpWrite(fd, @buf, buf_bytes);
      if res <> buf_bytes then
        show_error('unable to write to file after lseek');
    end;
  end;

  fpClose(fd);

  fd := fpOpen('trw.dat', O_RDONLY);
  if fd = -1 then
    show_error('unable to open data file read only');

  for i := 4095 downto 0 do
  begin
    seek_offset := TOff(i) * BUF_SIZE;
    file_offset := fpLSeek(fd, seek_offset, SEEK_SET);
    if file_offset <> seek_offset then
    begin
      WriteLn('file_offset ', file_offset, ', seek_offset ', seek_offset);
      show_error('lseek location not as expected');
    end;

    res := fpRead(fd, @buf, buf_bytes);
    if res <> buf_bytes then
      show_error('unable to read from file after lseek');

    for j := 0 to BUF_ELEMENTS - 1 do
    begin
      if (i mod 8) = 0 then
      begin
        if buf[j] <> SmallInt(i + $4000) then
          show_error('data read from file isn''t what was expected at point B');
      end
      else
      begin
        if buf[j] <> i then
          show_error('data read from file isn''t what was expected at point C');
      end;
    end;
  end;

  fpClose(fd);

{
  res := fpUnlink('trw.dat');
  if res <> 0 then
    show_error('can''t unlink test file');
}

  WriteLn('trw completed with great success');
end.
