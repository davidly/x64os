program lenum;

{$mode objfpc}

uses
  BaseUnix;

var
  dir: pDir;
  entry: pDirent;
  count: Integer;
  parent_found: Boolean;
  ret: cInt;

begin
  dir := fpOpenDir(PChar('.'));
  if dir = nil then
  begin
    WriteLn('can''t open current folder, error ', fpGetErrno);
    Halt(1);
  end;

  count := 0;
  parent_found := False;

  repeat
    entry := fpReadDir(dir^);
    if entry = nil then
      break;
    if string(PAnsiChar(@entry^.d_name)) = '..' then
      parent_found := True;
    Inc(count);
  until False;

  if not parent_found then
  begin
    WriteLn('error: parent folder not found in enumeration out of ', count, ' files returned');
    Halt(1);
  end;

  ret := fpCloseDir(dir^);
  if ret <> 0 then
  begin
    WriteLn('error: closedir after enumeration failed, errno: ', fpGetErrno);
    Halt(1);
  end;

  WriteLn('linux file system enumeration completed with great success');
end.
