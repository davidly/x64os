program trename;

{$mode objfpc}

uses
  BaseUnix, SysUtils;

const
  FileA = 'trenameA.txt';
  FileB = 'trenameB.txt';

function portable_filelen(const path: string): Int64;
var
  f: File;
begin
  Result := -1;
  AssignFile(f, path);
  {$I-}
  Reset(f, 1);
  {$I+}
  if IOResult <> 0 then
    Exit;
  Result := FileSize(f);
  CloseFile(f);
end;

procedure error(const err: string);
begin
  if err <> '' then
    WriteLn(Format('error: %s ==== errno: %d', [err, fpGetErrno]));
  Halt(1);
end;

var
  len: Int64;
  res: cInt;
  fbin: File;
  ftxt: TextFile;
  data: array[0..1023] of Byte;
  written: Integer;
  cwd: string;

begin
  fpUnlink(FileA);
  fpUnlink(FileB);

  len := portable_filelen(FileA);
  if len <> -1 then
    error('file A shouldn''t exist at point 1');

  len := portable_filelen(FileB);
  if len <> -1 then
    error('file B shouldn''t exist at point 1');

  AssignFile(fbin, FileA);
  {$I-}
  Rewrite(fbin, 1);
  {$I+}
  if IOResult <> 0 then
    error('can''t create file A');
  FillByte(data, SizeOf(data), 3);
  BlockWrite(fbin, data, SizeOf(data), written);
  if written <> SizeOf(data) then
  begin
    WriteLn(Format('result: %d', [written]));
    error('can''t write data to file A');
  end;
  {$I-}
  CloseFile(fbin);
  {$I+}
  if IOResult <> 0 then
    error('can''t close file A');

  res := fpRename(FileA, FileB);
  if res <> 0 then
    error('rename A to B failed');

  len := portable_filelen(FileA);
  if len <> -1 then
    error('file A shouldn''t exist after rename');

  len := portable_filelen(FileB);
  if len = -1 then
    error('file B should exist but apparently doesn''t');

  { linux (unlike windows or cp/m) supports renaming a file over an existing file! }
  cwd := GetCurrentDir;
  if cwd = '' then
    error('getcwd failed');

  if cwd <> '.' then
  begin
    AssignFile(ftxt, FileA);
    {$I-}
    Rewrite(ftxt);
    {$I+}
    if IOResult <> 0 then
      error('can''t create file A a second time');
    WriteLn(ftxt, 'fileA data I DONT CARE bdc');
    {$I-}
    CloseFile(ftxt);
    {$I+}
    if IOResult <> 0 then
      error('can''t close file A a second time');

    res := fpRename(FileA, FileB);
    if res <> 0 then
      error('rename A to B a second time failed');
  end;

  res := fpUnlink(FileB);
  if res <> 0 then
    error('can''t remove file B');

  WriteLn('trename completed with great success');
end.
