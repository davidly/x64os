program ff;

{$notes off}
{$INLINE OFF}
{$mode objfpc}
{$H+}
{$R-}
{$Q-}

uses
  SysUtils, BaseUnix;

const
  MaxPathLen = 2048;

  S_IFMT_   = $F000;
  S_IFSOCK_ = $C000;
  S_IFLNK_  = $A000;
  S_IFREG_  = $8000;
  S_IFBLK_  = $6000;
  S_IFDIR_  = $4000;
  S_IFCHR_  = $2000;
  S_IFIFO_  = $1000;

  S_IRUSR_ = $0100;
  S_IWUSR_ = $0080;
  S_IXUSR_ = $0040;
  S_IRGRP_ = $0020;
  S_IWGRP_ = $0010;
  S_IXGRP_ = $0008;
  S_IROTH_ = $0004;
  S_IWOTH_ = $0002;
  S_IXOTH_ = $0001;

var
  g_exclude_mnt: Boolean = True;
  g_show_info: Boolean = False;
  g_case_sensitive: Boolean = True;

procedure Usage(const Err: string);
begin
  if Err <> '' then
    WriteLn('error: ', Err);

  WriteLn('usage: ff <start> pattern');
  WriteLn('  finds files given an optional starting folder');
  WriteLn('  arguments:');
  WriteLn('      <start>         optional folder where enumeration starts. default is root');
  WriteLn('      pattern         file pattern to search for, likely enclosed in quotes');
  WriteLn('      -c              case-insensitive filename matching');
  WriteLn('      -i              show file information (type, size, last modified time)');
  WriteLn('      -m              don''t exclude files under /mnt, /sys, or /proc');
  WriteLn('  examples:');
  WriteLn('      ff /home/user "*.txt"');
  WriteLn('      ff -c -i .. lesserafim.png');
  WriteLn('      ff "*gcc"');
  Halt(1);
end;

function ModeTypeChar(mode: LongWord): Char;
begin
  case mode and S_IFMT_ of
    S_IFSOCK_: Result := 's';
    S_IFLNK_:  Result := 'l';
    S_IFREG_:  Result := ' ';
    S_IFBLK_:  Result := 'b';
    S_IFDIR_:  Result := 'd';
    S_IFCHR_:  Result := 'c';
    S_IFIFO_:  Result := 'f';
  else
    Result := '?';
  end;
end;

function ModeStr(mode: LongWord): string;
begin
  SetLength(Result, 11);

  Result[1] := ModeTypeChar(mode);
  Result[2] := ' ';

  if (mode and S_IRUSR_) <> 0 then Result[3] := 'r' else Result[3] := '-';
  if (mode and S_IWUSR_) <> 0 then Result[4] := 'w' else Result[4] := '-';
  if (mode and S_IXUSR_) <> 0 then Result[5] := 'x' else Result[5] := '-';

  if (mode and S_IRGRP_) <> 0 then Result[6] := 'r' else Result[6] := '-';
  if (mode and S_IWGRP_) <> 0 then Result[7] := 'w' else Result[7] := '-';
  if (mode and S_IXGRP_) <> 0 then Result[8] := 'x' else Result[8] := '-';

  if (mode and S_IROTH_) <> 0 then Result[9] := 'r' else Result[9] := '-';
  if (mode and S_IWOTH_) <> 0 then Result[10] := 'w' else Result[10] := '-';
  if (mode and S_IXOTH_) <> 0 then Result[11] := 'x' else Result[11] := '-';
end;

function IsDirMode(mode: LongWord): Boolean;
begin
  Result := (mode and S_IFMT_) = S_IFDIR_;
end;

function IsExcluded(const p: string): Boolean;
begin
  if g_exclude_mnt then
    Result := (p = '/mnt') or (p = '/sys') or (p = '/proc')
  else
    Result := False;
end;

function DirEntName(p: PDirent): string;
begin
  Result := StrPas(@p^.d_name[0]);
end;

function JoinPath(const a, b: string): string;
begin
  if a = '/' then
    Result := '/' + b
  else if (a <> '') and (a[Length(a)] = '/') then
    Result := a + b
  else
    Result := a + '/' + b;
end;

function WildMatchHere(const pat, txt: string; pi, ti: SizeInt): Boolean;
var
  pc: Char;
begin
  while pi <= Length(pat) do
  begin
    pc := pat[pi];

    if pc = '*' then
    begin
      while (pi <= Length(pat)) and (pat[pi] = '*') do
        Inc(pi);

      if pi > Length(pat) then
        Exit(True);

      while ti <= Length(txt) + 1 do
      begin
        if WildMatchHere(pat, txt, pi, ti) then
          Exit(True);
        Inc(ti);
      end;

      Exit(False);
    end
    else if pc = '?' then
    begin
      if ti > Length(txt) then
        Exit(False);
      Inc(pi);
      Inc(ti);
    end
    else
    begin
      if ti > Length(txt) then
        Exit(False);
      if pc <> txt[ti] then
        Exit(False);
      Inc(pi);
      Inc(ti);
    end;
  end;

  Result := ti > Length(txt);
end;

function WildMatch(const pat, txt: string): Boolean;
begin
  Result := WildMatchHere(pat, txt, 1, 1);
end;

procedure Search(const start, pattern: string);
var
  q: array of string;
  head, tail: SizeInt;
  current, next, name, matchName: string;
  sr: TSearchRec;
  st: TStat;
  err: cint;
  isDir: Boolean;
  timeText: string;
begin
  SetLength(q, 1024);
  head := 0;
  tail := 0;

  q[tail] := start;
  Inc(tail);

  while head < tail do
  begin
    current := q[head];
    Inc(head);

    err := FindFirst(JoinPath(current, '*'), faAnyFile, sr);
    if err <> 0 then
    begin
      if (err <> ESysEACCES) and (err <> ESysENOENT) then
        WriteLn('can''t open starting folder ''', current, ''', error ', err);
      Continue;
    end;

    repeat
      name := sr.Name;

      if (name = '.') or (name = '..') then
        Continue;

      next := JoinPath(current, name);

      if Length(next) >= MaxPathLen then
      begin
        WriteLn('error: path too long, skipping ''', name, '''');
        Continue;
      end;

      if IsExcluded(next) then
        Continue;

      if fpLStat(PChar(next), st) <> 0 then
        Continue;

      isDir := IsDirMode(st.st_mode);

      if isDir then
      begin
        if tail >= Length(q) then
          SetLength(q, Length(q) * 2);
        q[tail] := next;
        Inc(tail);
      end;

      matchName := name;
      if not g_case_sensitive then
        matchName := LowerCase(matchName);

      if WildMatch(pattern, matchName) then
      begin
        if g_show_info then
        begin
          timeText := FormatDateTime('yyyy-mm-dd hh:nn:ss', FileDateToDateTime(sr.Time));
          Write(ModeStr(st.st_mode), '  ', st.st_size:13, '  ', timeText, '  ');
        end;

        WriteLn(next);
      end;

    until FindNext(sr) <> 0;

    FindClose(sr);
  end;
end;

var
  pstart, ppattern: string;
  i: Integer;
  arg: string;

begin
  pstart := '';
  ppattern := '';

  for i := 1 to ParamCount do
  begin
    arg := ParamStr(i);

    if arg = '-c' then
      g_case_sensitive := False
    else if arg = '-i' then
      g_show_info := True
    else if arg = '-m' then
      g_exclude_mnt := False
    else if (Length(arg) > 0) and (arg[1] = '-') then
      Usage('unrecognized argument')
    else if ppattern = '' then
      ppattern := arg
    else if pstart = '' then
    begin
      pstart := ppattern;
      ppattern := arg;
    end
    else
      Usage('too many arguments');
  end;

  if ppattern = '' then
    Usage('missing pattern argument');

  if pstart = '' then
    pstart := '/';

  pstart := ExpandFileName(pstart);

  if not DirectoryExists(pstart) then
    Usage('unable to resolve starting path');

  if not g_case_sensitive then
    ppattern := LowerCase(ppattern);

  Search(pstart, ppattern);
end.