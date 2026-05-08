program tdir;

{$mode objfpc}

uses
  BaseUnix, SysUtils;

const
  foldername = 'tdir_testfolder';
  filename = 'tdir_testfile.txt';

function my_realpath(const path: string): string;
var
  saved: string;
begin
  Result := '';
  saved := GetCurrentDir;
  if fpChDir(path) = 0 then
  begin
    Result := GetCurrentDir;
    fpChDir(saved);
  end;
end;

var
  res: cInt;
  st: TStat;
  cwd_original, cwd, cwd_final: string;
  pslash_pos: Integer;
  f: TextFile;
  resolved: string;

begin
  res := fpMkDir(foldername, S_IRWXU or S_ISVTX);
  if res <> 0 then
  begin
    WriteLn('mkdir failed, error ', fpGetErrno);
    WriteLn('perhaps folder ''', foldername, ''' exists from a prior run; deleting it');
    Flush(Output);

    if fpChDir(foldername) = 0 then
    begin
      fpUnlink(filename);
      res := fpChDir('..');
      if res <> 0 then
      begin
        WriteLn('for some reason chdir .. failed: ', res);
        Halt(1);
      end;
    end;

    res := fpRmDir(foldername);
    if res <> 0 then
    begin
      WriteLn('start of app cleanup: rmdir of folder ', foldername, ' failed, error ', fpGetErrno);
      Halt(1);
    end;

    res := fpMkDir(foldername, S_IRWXU or S_ISVTX);
    if res <> 0 then
    begin
      WriteLn('creation of folder ', foldername, ' failed, error ', fpGetErrno);
      Halt(1);
    end;
  end;

  cwd_original := GetCurrentDir;
  if cwd_original = '' then
  begin
    WriteLn('unable to retrieve original current directory, error ', fpGetErrno);
    Halt(1);
  end;

  res := fpChDir(foldername);
  if res <> 0 then
  begin
    WriteLn('chdir into the test folder ', foldername, ' failed, error ', fpGetErrno);
    Halt(1);
  end;

  cwd := GetCurrentDir;
  if cwd = '' then
  begin
    WriteLn('unable to retrieve current (child) directory, error ', fpGetErrno);
    Halt(1);
  end;

  pslash_pos := LastDelimiter('/', cwd);
  if pslash_pos = 0 then
  begin
    WriteLn('unable to find a slash in cwd ''', cwd, '''');
    Halt(1);
  end;

  if Copy(cwd, pslash_pos + 1, MaxInt) <> foldername then
  begin
    WriteLn('cwd ''', Copy(cwd, pslash_pos + 1, MaxInt), ''' isn''t expected value ''', foldername, '''');
    Halt(1);
  end;

  AssignFile(f, filename);
  Rewrite(f);
  WriteLn(f, 'aespa winter');
  CloseFile(f);

  FillByte(st, SizeOf(st), 0);
  res := fpStat(filename, st);
  if res <> 0 then
  begin
    WriteLn('stat on file ''', filename, ''' failed, error ', fpGetErrno);
    Halt(1);
  end;

  if fpS_ISDIR(st.st_mode) then
  begin
    WriteLn('stat claims file ''', filename, ''' is a directory');
    Halt(1);
  end;

  FillByte(st, SizeOf(st), 0);
  res := fpLStat(filename, st);
  if res <> 0 then
  begin
    WriteLn('lstat on file ''', filename, ''' failed, error ', fpGetErrno);
    Halt(1);
  end;

  if fpS_ISDIR(st.st_mode) then
  begin
    WriteLn('lstat claims file ''', filename, ''' is a directory');
    Halt(1);
  end;

  res := fpUnlink(filename);
  if res <> 0 then
  begin
    WriteLn('removal of ', filename, ' file failed, error ', fpGetErrno);
    Halt(1);
  end;

  res := fpChDir('..');
  if res <> 0 then
  begin
    WriteLn('cd back up to parent folder .. failed, error ', fpGetErrno);
    Halt(1);
  end;

  FillByte(st, SizeOf(st), 0);
  res := fpStat(foldername, st);
  if res <> 0 then
  begin
    WriteLn('stat on folder ''', foldername, ''' failed, error ', fpGetErrno);
    Halt(1);
  end;

  if not fpS_ISDIR(st.st_mode) then
  begin
    WriteLn('stat claims directory ''', foldername, ''' isn''t a directory. st_mode: 0x', IntToHex(st.st_mode, 1));
    Halt(1);
  end;

  FillByte(st, SizeOf(st), 0);
  res := fpLStat(foldername, st);
  if res <> 0 then
  begin
    WriteLn('lstat on folder ''', foldername, ''' failed, error ', fpGetErrno);
    Halt(1);
  end;

  if not fpS_ISDIR(st.st_mode) then
  begin
    WriteLn('lstat claims directory ''', foldername, ''' isn''t a directory. st_mode: 0x', IntToHex(st.st_mode, 1));
    WriteLn('S_IFDIR: 0x', IntToHex(S_IFDIR, 1));
    Halt(1);
  end;

  resolved := my_realpath(foldername);
  if resolved = '' then
  begin
    WriteLn('realpath failed, error ', fpGetErrno);
    Halt(1);
  end;

  if resolved <> cwd then
  begin
    WriteLn('realpath of child folder ''', resolved, ''' doesn''t match getcwd result ''', cwd, '''');
    Halt(1);
  end;

  res := fpRmDir(foldername);
  if res <> 0 then
  begin
    WriteLn('end of app cleanup: rmdir of folder ''', foldername, ''' failed, error ', fpGetErrno);
    Halt(1);
  end;

  res := fpChDir(foldername);
  if res = 0 then
  begin
    WriteLn('cd into the removed test folder ''', foldername, ''' succeded, and it shouldn''t have');
    Halt(1);
  end;

  cwd_final := GetCurrentDir;
  if cwd_final = '' then
  begin
    WriteLn('unable to retrieve final current directory, error ', fpGetErrno);
    Halt(1);
  end;

  if cwd_original <> cwd_final then
  begin
    WriteLn('original directory ''', cwd_original, ''' isn''t the same as final directory ''', cwd_final, '''');
    Halt(1);
  end;

  WriteLn('tdir completed with great success');
end.
